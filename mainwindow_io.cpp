#include "mainwindow.h"
#include "mainwindow_support.h"
#include "projectsync.h"
#include "windowsintegration.h"
#include "exportdialog.h"
#include "printdialog.h"

#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QFileInfo>
#include <QDialog>
#include <QPrinter>
#include <QPrintPreviewDialog>
#include <QPdfWriter>
#include <algorithm>

import badge.documentio;
import badge.document;

void MainWindow::onExportPng() {
    requestLayoutRefresh("export png");
    flushInternalEvents();
    const QString defaultName = m_currentFile.isEmpty()
        ? QStringLiteral("layout.png")
        : QFileInfo(m_currentFile).completeBaseName() + QStringLiteral("_layout.png");
    ExportDialog dlg(ExportDialog::Format::Image, defaultName, this);
    if (dlg.exec() != QDialog::Accepted) return;
    QString outPath = dlg.filePath();
    if (outPath.isEmpty()) {
        QMessageBox::warning(this, "画像出力", "保存先を指定してください");
        return;
    }
    if (!outPath.endsWith(".png", Qt::CaseInsensitive)) outPath += ".png";
    if (!m_layoutWorkspace->exportPng(outPath, dlg.dpi(), dlg.whiteBackground(), dlg.includeGuides())) {
        showOperationWarning(this, QStringLiteral("画像出力"), QStringLiteral("PNGの書き出し"), outPath,
                             m_layoutWorkspace ? m_layoutWorkspace->lastError() : QString());
        return;
    }
    if (m_windowsIntegration) {
        m_windowsIntegration->showToast(QStringLiteral("PNGを書き出しました"), QFileInfo(outPath).fileName(),
                                        WindowsIntegration::ToastKind::Success);
    }
}

void MainWindow::onPrintPreview() {
    requestLayoutRefresh("print preview");
    flushInternalEvents();
    const auto pages = currentLayoutPages();
    const badge::DocumentData document = projectsync::currentDocument(
        m_layoutBadges, *m_comboPaperSize, *m_chkLandscape, *m_spinPaperMargin, *m_spinPaperSpacing, m_currentFile);
    PrintDialog settings(document.paper.widthMm, document.paper.heightMm, m_appSettings.printResolution, this);
    settings.setWindowTitle(QStringLiteral("印刷プレビューの設定"));
    if (settings.exec() != QDialog::Accepted) return;

    QPrinter printer(QPrinter::HighResolution);
    const QString printerName = settings.printerName();
    if (!printerName.isEmpty()) printer.setPrinterName(printerName);
    configurePrinterForDocument(printer, document, settings.resolution());
    printer.setColorMode(settings.grayScale() ? QPrinter::GrayScale : QPrinter::Color);
    const bool includeGuides = settings.includeGuides();
    QPrintPreviewDialog preview(&printer, this);
    preview.setWindowTitle(QStringLiteral("印刷プレビュー"));
    connect(&preview, &QPrintPreviewDialog::paintRequested, this, [this, includeGuides](QPrinter* previewPrinter) {
        if (m_layoutWorkspace) m_layoutWorkspace->print(previewPrinter, currentLayoutPages(), includeGuides);
    });
    if (preview.exec() == QDialog::Accepted) {
        m_appSettings.printResolution = std::max(72, settings.resolution());
        saveAppSettings();
    }
}

void MainWindow::onPrint() {
    requestLayoutRefresh("print");
    flushInternalEvents();
    const auto pages = currentLayoutPages();
    const badge::DocumentData document = projectsync::currentDocument(
        m_layoutBadges, *m_comboPaperSize, *m_chkLandscape, *m_spinPaperMargin, *m_spinPaperSpacing, m_currentFile);
    PrintDialog dlg(document.paper.widthMm, document.paper.heightMm, m_appSettings.printResolution, this);
    if (dlg.exec() != QDialog::Accepted) return;
    QPrinter printer(QPrinter::HighResolution);
    const QString printerName = dlg.printerName();
    if (!printerName.isEmpty()) printer.setPrinterName(printerName);
    configurePrinterForDocument(printer, document, dlg.resolution());
    printer.setColorMode(dlg.grayScale() ? QPrinter::GrayScale : QPrinter::Color);
    printer.setCopyCount(std::max(1, dlg.copies()));
    m_appSettings.printResolution = std::max(72, dlg.resolution());
    saveAppSettings();
    if (!m_layoutWorkspace->print(&printer, pages, dlg.includeGuides())) {
        showOperationWarning(this, QStringLiteral("印刷"), QStringLiteral("印刷"), QString(),
                             m_layoutWorkspace ? m_layoutWorkspace->lastError() : QString());
    } else if (m_windowsIntegration) {
        m_windowsIntegration->showToast(QStringLiteral("印刷を開始しました"), printer.printerName(),
                                        WindowsIntegration::ToastKind::Success);
    }
}

void MainWindow::onExportPdf() {
    requestLayoutRefresh("export pdf");
    flushInternalEvents();
    const auto pages = currentLayoutPages();
    const QString defaultName = m_currentFile.isEmpty() ? QStringLiteral("layout.pdf")
        : QFileInfo(m_currentFile).completeBaseName() + QStringLiteral("_layout.pdf");
    ExportDialog dlg(ExportDialog::Format::Pdf, defaultName, this);
    if (dlg.exec() != QDialog::Accepted) return;
    QString outPath = dlg.filePath();
    if (outPath.isEmpty()) {
        QMessageBox::warning(this, "PDF出力", "保存先を指定してください");
        return;
    }
    if (!outPath.endsWith(".pdf", Qt::CaseInsensitive)) outPath += ".pdf";
    QPdfWriter::ColorModel colorModel = QPdfWriter::ColorModel::RGB;
    if (dlg.pdfColorModelIndex() == 1) colorModel = QPdfWriter::ColorModel::CMYK;
    else if (dlg.pdfColorModelIndex() == 2) colorModel = QPdfWriter::ColorModel::Grayscale;
    if (!m_layoutWorkspace->exportPdf(pages, outPath, dlg.dpi(), colorModel, dlg.includeGuides())) {
        showOperationWarning(this, QStringLiteral("PDF出力"), QStringLiteral("PDFの書き出し"), outPath,
                             m_layoutWorkspace ? m_layoutWorkspace->lastError() : QString());
        return;
    }
    if (m_windowsIntegration) {
        m_windowsIntegration->showToast(QStringLiteral("PDFを書き出しました"), QFileInfo(outPath).fileName(),
                                        WindowsIntegration::ToastKind::Success);
    }
}


#include <QFileDialog>

void MainWindow::onNew() {
    m_currentFile.clear();
    m_badges.clear();
    m_layoutBadges.clear();
    m_layoutPages.clear();
    m_layoutPageNames.clear();
    m_layoutPageIndex = 0;
    m_layoutPreviewMode = LayoutPreviewMode::CurrentDesign;
    m_designer->clearBadges();
    BadgeItem blank;
    blank.clipToCircle = true;
    m_designer->addBadge(blank);
    m_designer->updateGuides(32);
    if (!m_isDesigner) {
        requestLayoutRefresh("new document");
        flushInternalEvents();
    }
    refreshDocumentFromDesigner();
    appendLog("新規プロジェクトを作成しました");
    updateTitle();
}

void MainWindow::onOpen() {
    const QString path = QFileDialog::getOpenFileName(this, "開く", QString(), "バッジエディタファイル (*.bge *.json)");
    if (!path.isEmpty()) openProjectPath(path);
}

void MainWindow::openProjectPath(const QString& path) {
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        showOperationWarning(this, QStringLiteral("開く"), QStringLiteral("ファイルの読み込み"), path, f.errorString());
        return;
    }
    const auto loaded = badge::loadDocumentFromJson(f.readAll());
    if (!loaded.ok) {
        showOperationWarning(this, QStringLiteral("開く"), QStringLiteral("ファイルの読み込み"), path,
                             loaded.errorMessage.isEmpty() ? QStringLiteral("JSON の形式を確認してください") : loaded.errorMessage);
        return;
    }
    onNew();
    projectsync::applyDocument(*m_designer, *m_layoutWorkspace, *m_comboPaperSize, *m_chkLandscape,
                               *m_spinPaperMargin, *m_spinPaperSpacing, loaded.document);
    refreshDocumentFromDesigner();
    m_layoutPages.clear();
    m_layoutPageNames.clear();
    m_layoutPageIndex = 0;
    m_layoutBadges = m_badges;
    m_layoutPreviewMode = LayoutPreviewMode::CurrentDesign;
    m_currentFile = path;
    if (!m_isDesigner) {
        requestLayoutRefresh("project opened");
        flushInternalEvents();
    }
    appendLog(QStringLiteral("開きました: %1").arg(path));
    updateTitle();
    if (m_windowsIntegration) {
        m_windowsIntegration->rememberFile(path);
        m_windowsIntegration->showToast(QStringLiteral("プロジェクトを開きました"), QFileInfo(path).fileName(),
                                        WindowsIntegration::ToastKind::Success);
    }
}

void MainWindow::onSave() {
    if (m_currentFile.isEmpty()) {
        onSaveAs();
        return;
    }
    QFile f(m_currentFile);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(badge::saveDocumentToJson(projectsync::currentDocument(
            m_badges, *m_comboPaperSize, *m_chkLandscape, *m_spinPaperMargin, *m_spinPaperSpacing, m_currentFile)));
        appendLog(QStringLiteral("保存しました: %1").arg(m_currentFile));
        requestDiagnosticsRefresh("project saved");
        if (m_windowsIntegration) {
            m_windowsIntegration->rememberFile(m_currentFile);
            m_windowsIntegration->showToast(QStringLiteral("保存しました"), QFileInfo(m_currentFile).fileName(),
                                            WindowsIntegration::ToastKind::Success);
        }
    } else {
        showOperationWarning(this, QStringLiteral("保存"), QStringLiteral("ファイルの保存"), m_currentFile, f.errorString());
    }
}

void MainWindow::onSaveAs() {
    const QString path = QFileDialog::getSaveFileName(this, "保存", "badges.bge", "バッジエディタファイル (*.bge)");
    if (path.isEmpty()) return;
    m_currentFile = path;
    onSave();
    appendLog(QStringLiteral("名前を付けて保存: %1").arg(path));
    updateTitle();
}

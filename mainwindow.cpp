#include "mainwindow.h"
#include "badgegraphicitem.h"
#include "batchlayoutdialog.h"
#include "layoutengine.h"
#include "projectsync.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QListWidget>
#include <QApplication>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QPushButton>
#include <QShortcut>
#include <QInputDialog>
#include <QSettings>
#include <DockManager.h>
#include <DockAreaWidget.h>
#include <DockWidget.h>
#include <QFileInfo>

import badge.documentio;
import badge.model;

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Badge Editor Pro");
    resize(1300, 900);
    setAcceptDrops(true);
    m_undoStack = new QUndoStack(this);

    // --- Menu ---
    auto* fileMenu = menuBar()->addMenu("ファイル(&F)");
    fileMenu->addAction("新規(&N)", this, &MainWindow::onNew, QKeySequence::New);
    fileMenu->addAction("開く(&O)...", this, &MainWindow::onOpen, QKeySequence::Open);
    fileMenu->addAction("保存(&S)", this, &MainWindow::onSave, QKeySequence::Save);
    fileMenu->addAction("名前を付けて保存(&A)...", this, &MainWindow::onSaveAs, QKeySequence("Ctrl+Shift+S"));
    fileMenu->addSeparator();
    fileMenu->addAction("PDF出力...", this, &MainWindow::onExportPdf);
    fileMenu->addAction("画像出力...", this, &MainWindow::onExportPng);

    auto* editMenu = menuBar()->addMenu("編集(&E)");
    editMenu->addAction("元に戻す(&U)", this, &MainWindow::onUndo, QKeySequence::Undo);
    editMenu->addAction("やり直し(&R)", this, &MainWindow::onRedo, QKeySequence::Redo);
    editMenu->addSeparator();
    editMenu->addAction("削除", this, &MainWindow::onDelete, QKeySequence::Delete);
    auto* backspaceShortcut = new QShortcut(QKeySequence(Qt::Key_Backspace), this);
    connect(backspaceShortcut, &QShortcut::activated, this, &MainWindow::onDelete);

    auto* viewMenu = menuBar()->addMenu("表示(&V)");
    viewMenu->addAction("ズームイン", this, [this]{ m_zoomScale *= 1.2; m_zoomLabel->setText(QString::number(m_zoomScale*100,'f',0)+"%"); });
    viewMenu->addAction("ズームアウト", this, [this]{ m_zoomScale /= 1.2; m_zoomLabel->setText(QString::number(m_zoomScale*100,'f',0)+"%"); });
    viewMenu->addSeparator();
    m_actTheme = viewMenu->addAction("テーマ切替", this, &MainWindow::onToggleTheme);
    viewMenu->addAction("ドック配置を初期化", this, &MainWindow::resetDockState);
    auto* perspectiveMenu = viewMenu->addMenu("Perspective");
    m_perspectiveMenu = perspectiveMenu;
    perspectiveMenu->addAction("現在を編集として保存", this, &MainWindow::saveDesignerPerspective);
    perspectiveMenu->addAction("現在を配置確認として保存", this, &MainWindow::saveLayoutPerspective);
    perspectiveMenu->addAction("名前を付けて保存...", this, &MainWindow::savePerspectiveAs);
    perspectiveMenu->addSeparator();
    perspectiveMenu->addAction("編集を開く", this, &MainWindow::openDesignerPerspective);
    perspectiveMenu->addAction("配置確認を開く", this, &MainWindow::openLayoutPerspective);

    // --- Toolbar ---
    m_toolbar = addToolBar("Main");
    m_toolbar->setMovable(false);
    m_actDesigner = m_toolbar->addAction("DESIGNER");
    m_actDesigner->setCheckable(true); m_actDesigner->setChecked(true);
    m_actLayout = m_toolbar->addAction("LAYOUT");
    m_actLayout->setCheckable(true);
    connect(m_actDesigner, &QAction::triggered, this, [this]{ onModeChanged(true); });
    connect(m_actLayout, &QAction::triggered, this, [this]{ onModeChanged(false); });

    QAction* addAct = m_toolbar->addAction("＋");
    connect(addAct, &QAction::triggered, this, &MainWindow::onAddBadge);
    QAction* batchAct = m_toolbar->addAction("一括");
    connect(batchAct, &QAction::triggered, this, &MainWindow::onBatchAdd);
    m_toolbar->addSeparator();
    QAction* autoAct = m_toolbar->addAction("自動配置");
    connect(autoAct, &QAction::triggered, this, &MainWindow::onAutoLayout);
    m_toolbar->addSeparator();
    QAction* designerPerspectiveAct = m_toolbar->addAction("編集");
    connect(designerPerspectiveAct, &QAction::triggered, this, &MainWindow::openDesignerPerspective);
    designerPerspectiveAct->setShortcut(QKeySequence("Ctrl+E"));
    QAction* layoutPerspectiveAct = m_toolbar->addAction("配置確認");
    connect(layoutPerspectiveAct, &QAction::triggered, this, &MainWindow::openLayoutPerspective);
    layoutPerspectiveAct->setShortcut(QKeySequence("Ctrl+L"));
    m_toolbar->addSeparator();
    m_zoomLabel = new QLabel("100%");
    m_toolbar->addWidget(m_zoomLabel);

    // Designer
    m_designer = new DesignerWidget;
    connect(m_designer, &DesignerWidget::badgeSelected, this, &MainWindow::onBadgeSelected);
    connect(m_designer, &DesignerWidget::badgeDeselected, this, &MainWindow::onBadgeDeselected);
    connect(m_designer, &DesignerWidget::badgeDoubleClicked, this, [this](BadgeGraphicItem*){ onSetImage(); });
    connect(m_designer, &DesignerWidget::badgeMoved, this, &MainWindow::onBadgeMoved);
    m_designer->updateGuides(57);

    // Layout
    m_layoutWorkspace = new LayoutWorkspaceWidget;

    // --- Inspector ---
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setMinimumWidth(280);
    scroll->setMaximumWidth(320);
    m_inspector = new QWidget;
    auto* inspLayout = new QVBoxLayout(m_inspector);

    // Properties group
    auto* propGroup = new QGroupBox("オブジェクト情報");
    auto* propForm = new QFormLayout(propGroup);
    m_propX = new QDoubleSpinBox; m_propX->setSuffix(" mm"); m_propX->setRange(0, 2000);
    m_propY = new QDoubleSpinBox; m_propY->setSuffix(" mm"); m_propY->setRange(0, 2000);
    m_propW = new QDoubleSpinBox; m_propW->setSuffix(" mm"); m_propW->setRange(2, 200);
    m_propH = new QDoubleSpinBox; m_propH->setSuffix(" mm"); m_propH->setRange(2, 200);
    m_propRotation = new QSlider(Qt::Horizontal); m_propRotation->setRange(0, 360);
    m_propText = new QLineEdit;
    propForm->addRow("X:", m_propX); propForm->addRow("Y:", m_propY);
    propForm->addRow("幅:", m_propW); propForm->addRow("高さ:", m_propH);
    // Size preset
    auto* sizePreset = new QComboBox;
    sizePreset->addItems({"カスタム", "32mm", "44mm", "57mm", "65mm", "76mm"});
    propForm->addRow("プリセット:", sizePreset);
    connect(sizePreset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        double sizes[] = {0, 32, 44, 57, 65, 76};
        if (idx > 0) { m_propW->setValue(sizes[idx]); m_propH->setValue(sizes[idx]); }
    });
    propForm->addRow("回転:", m_propRotation); propForm->addRow("テキスト:", m_propText);
    auto* imgBtn = new QPushButton("画像を変更...");
    connect(imgBtn, &QPushButton::clicked, this, &MainWindow::onSetImage);
    propForm->addRow(imgBtn);
    m_propColorSpace = new QLineEdit;
    m_propColorSpace->setReadOnly(true);
    propForm->addRow("色空間:", m_propColorSpace);
    inspLayout->addWidget(propGroup);

    connect(m_propX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onInspectorChanged);
    connect(m_propY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onInspectorChanged);
    connect(m_propW, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onInspectorChanged);
    connect(m_propH, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onInspectorChanged);
    connect(m_propRotation, &QSlider::valueChanged, this, [this]{ onInspectorChanged(); });
    connect(m_propText, &QLineEdit::textChanged, this, [this]{ onInspectorChanged(); });

    // Color correction group
    auto* colorGroup = new QGroupBox("色補正");
    auto* colorForm = new QFormLayout(colorGroup);
    m_propBrightness = new QSlider(Qt::Horizontal); m_propBrightness->setRange(-100, 100);
    m_propContrast = new QSlider(Qt::Horizontal); m_propContrast->setRange(-100, 100);
    m_propSaturation = new QSlider(Qt::Horizontal); m_propSaturation->setRange(-100, 100);
    colorForm->addRow("明るさ:", m_propBrightness);
    colorForm->addRow("コントラスト:", m_propContrast);
    colorForm->addRow("彩度:", m_propSaturation);
    auto* resetBtn = new QPushButton("リセット");
    connect(resetBtn, &QPushButton::clicked, this, [this]{ m_propBrightness->setValue(0); m_propContrast->setValue(0); m_propSaturation->setValue(0); onInspectorChanged(); });
    colorForm->addRow(resetBtn);
    inspLayout->addWidget(colorGroup);

    connect(m_propBrightness, &QSlider::valueChanged, this, [this]{ onInspectorChanged(); });
    connect(m_propContrast, &QSlider::valueChanged, this, [this]{ onInspectorChanged(); });
    connect(m_propSaturation, &QSlider::valueChanged, this, [this]{ onInspectorChanged(); });

    // Layer group
    auto* layerGroup = new QGroupBox("レイヤー");
    auto* layerLayout = new QVBoxLayout(layerGroup);
    auto* layerList = new QListWidget;
    layerList->setMaximumHeight(120);
    layerLayout->addWidget(layerList);
    auto* layerBtnRow = new QHBoxLayout;
    auto* btnAddLayer = new QPushButton("＋");
    auto* btnDelLayer = new QPushButton("－");
    auto* btnUpLayer = new QPushButton("▲");
    auto* btnDownLayer = new QPushButton("▼");
    btnAddLayer->setFixedWidth(30); btnDelLayer->setFixedWidth(30);
    btnUpLayer->setFixedWidth(30); btnDownLayer->setFixedWidth(30);
    layerBtnRow->addWidget(btnAddLayer);
    layerBtnRow->addWidget(btnDelLayer);
    layerBtnRow->addWidget(btnUpLayer);
    layerBtnRow->addWidget(btnDownLayer);
    layerBtnRow->addStretch();
    layerLayout->addLayout(layerBtnRow);
    inspLayout->addWidget(layerGroup);

    connect(btnAddLayer, &QPushButton::clicked, this, [this]{ onSetImage(); });
    connect(btnDelLayer, &QPushButton::clicked, this, [this, layerList]{
        int row = layerList->currentRow();
        if (row < 0 || m_selected.isEmpty()) return;
        auto& layers = m_selected.first()->badge().layers;
        if (row < layers.size()) { layers.removeAt(row); layerList->takeItem(row); m_selected.first()->update(); }
    });
    connect(btnUpLayer, &QPushButton::clicked, this, [this, layerList]{
        int row = layerList->currentRow();
        if (row <= 0 || m_selected.isEmpty()) return;
        auto& layers = m_selected.first()->badge().layers;
        if (row < layers.size()) { layers.swapItemsAt(row, row-1); layerList->takeItem(row); auto* item = layerList->takeItem(row-1);
            layerList->insertItem(row-1, layerList->item(row)); layerList->insertItem(row, item); layerList->setCurrentRow(row-1);
            m_selected.first()->update(); }
    });
    connect(btnDownLayer, &QPushButton::clicked, this, [this, layerList]{
        int row = layerList->currentRow();
        if (row < 0 || m_selected.isEmpty()) return;
        auto& layers = m_selected.first()->badge().layers;
        if (row + 1 < layers.size()) { layers.swapItemsAt(row, row+1); auto* item = layerList->takeItem(row);
            layerList->insertItem(row+1, item); layerList->setCurrentRow(row+1); m_selected.first()->update(); }
    });

    // Guide group
    auto* guideGroup = new QGroupBox("ガイド表示");
    auto* guideLayout = new QVBoxLayout(guideGroup);
    m_chkBleed = new QCheckBox("巻き込みエリア (塗り足し)"); m_chkBleed->setChecked(true);
    m_chkVisible = new QCheckBox("可視エリア (安全圏)"); m_chkVisible->setChecked(true);
    guideLayout->addWidget(m_chkBleed);
    guideLayout->addWidget(m_chkVisible);
    inspLayout->addWidget(guideGroup);
    connect(m_chkBleed, &QCheckBox::toggled, this, &MainWindow::onGuideToggle);
    connect(m_chkVisible, &QCheckBox::toggled, this, &MainWindow::onGuideToggle);

    // Effects group
    auto* effectGroup = new QGroupBox("エフェクト");
    auto* effectLayout = new QVBoxLayout(effectGroup);
    m_chkGlitter = new QCheckBox("ラメフィルタ");
    m_comboGlitter = new QComboBox;
    m_comboGlitter->addItems({"全面輝き", "星型", "雪型"});
    m_chkLighting = new QCheckBox("光源プレビュー");
    m_sliderLightAngle = new QSlider(Qt::Horizontal); m_sliderLightAngle->setRange(0, 360); m_sliderLightAngle->setValue(315);
    m_sliderLightIntensity = new QSlider(Qt::Horizontal); m_sliderLightIntensity->setRange(0, 100); m_sliderLightIntensity->setValue(80);
    effectLayout->addWidget(m_chkGlitter);
    effectLayout->addWidget(m_comboGlitter);
    effectLayout->addWidget(m_chkLighting);
    effectLayout->addWidget(new QLabel("光源角度"));
    effectLayout->addWidget(m_sliderLightAngle);
    effectLayout->addWidget(new QLabel("光源の強さ"));
    effectLayout->addWidget(m_sliderLightIntensity);
    inspLayout->addWidget(effectGroup);

    connect(m_chkLighting, &QCheckBox::toggled, this, &MainWindow::onLightingToggle);
    connect(m_chkGlitter, &QCheckBox::toggled, this, &MainWindow::onGlitterToggle);
    connect(m_comboGlitter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onGlitterPatternChanged);
    connect(m_sliderLightAngle, &QSlider::valueChanged, this, &MainWindow::onLightingSlider);
    connect(m_sliderLightIntensity, &QSlider::valueChanged, this, &MainWindow::onLightingSlider);

    // Layout config group
    auto* layoutGroup = new QGroupBox("用紙設定");
    auto* layoutForm = new QFormLayout(layoutGroup);
    m_comboPaperSize = new QComboBox; m_comboPaperSize->addItems({"A4 (210 x 297 mm)", "B5 (182 x 257 mm)"});
    m_chkLandscape = new QCheckBox("横向き");
    m_spinPaperMargin = new QDoubleSpinBox;
    m_spinPaperMargin->setSuffix(" mm");
    m_spinPaperMargin->setRange(0.0, 100.0);
    m_spinPaperMargin->setValue(10.0);
    m_spinPaperSpacing = new QDoubleSpinBox;
    m_spinPaperSpacing->setSuffix(" mm");
    m_spinPaperSpacing->setRange(0.0, 50.0);
    m_spinPaperSpacing->setValue(1.0);
    layoutForm->addRow("用紙:", m_comboPaperSize);
    layoutForm->addRow(m_chkLandscape);
    layoutForm->addRow("余白:", m_spinPaperMargin);
    layoutForm->addRow("間隔:", m_spinPaperSpacing);
    inspLayout->addWidget(layoutGroup);

    connect(m_comboPaperSize, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]{ if (!m_isDesigner) syncLayoutWorkspace(); });
    connect(m_chkLandscape, &QCheckBox::toggled, this, [this]{ if (!m_isDesigner) syncLayoutWorkspace(); });
    connect(m_spinPaperMargin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]{ if (!m_isDesigner) syncLayoutWorkspace(); });
    connect(m_spinPaperSpacing, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]{ if (!m_isDesigner) syncLayoutWorkspace(); });

    inspLayout->addStretch();
    scroll->setWidget(m_inspector);

    // --- Docking System ---
    ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::XmlCompressionEnabled, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::FocusHighlighting, true);
    m_dockManager = new ads::CDockManager(this);
    connect(m_dockManager, &ads::CDockManager::perspectiveOpened, this, &MainWindow::syncPerspectiveUi);

    m_designerDock = new ads::CDockWidget("編集");
    m_designerDock->setWidget(m_designer);
    m_designerDock->setFeature(ads::CDockWidget::DockWidgetClosable, false);

    m_layoutDock = new ads::CDockWidget("配置確認");
    m_layoutDock->setWidget(m_layoutWorkspace);
    m_layoutDock->setFeature(ads::CDockWidget::DockWidgetClosable, false);

    m_inspectorDock = new ads::CDockWidget("インスペクター");
    m_inspectorDock->setWidget(scroll);
    m_inspectorDock->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromDockWidget);
    m_inspectorDock->resize(300, 800);

    m_dockArea = m_dockManager->setCentralWidget(m_designerDock);
    m_dockManager->addDockWidgetTabToArea(m_layoutDock, m_dockArea);
    m_dockManager->addDockWidget(ads::RightDockWidgetArea, m_inspectorDock, m_dockArea);
    m_defaultDockState = m_dockManager->saveState(1);
    m_dockManager->addPerspective("designer");
    m_layoutDock->setAsCurrentTab();
    m_dockManager->addPerspective("layout");
    m_designerDock->setAsCurrentTab();
    loadDockState();
    refreshPerspectiveMenu();

    applyTheme(true);
    openDesignerPerspective();
    updateTitle();
}

// --- File slots ---
void MainWindow::onNew() {
    m_currentFile.clear();
    m_designer->clearBadges();
    m_designer->addBadge(BadgeItem{});
    m_designer->updateGuides(57);
    syncLayoutWorkspace();
    updateTitle();
}

void MainWindow::onOpen() {
    QString path = QFileDialog::getOpenFileName(this, "開く", QString(), "バッジエディタファイル (*.bge *.json)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        const auto loaded = badge::loadDocumentFromJson(f.readAll());
        if (!loaded.ok) {
            QMessageBox::warning(this, "開く", "ファイルの読み込みに失敗しました");
            return;
        }
        onNew();
        projectsync::applyDocument(*m_designer, *m_layoutWorkspace, *m_comboPaperSize, *m_chkLandscape, *m_spinPaperMargin, *m_spinPaperSpacing, loaded.document);
        m_currentFile = path;
        updateTitle();
    }
}

void MainWindow::onSave() {
    if (m_currentFile.isEmpty()) { onSaveAs(); return; }
    QFile f(m_currentFile);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(badge::saveDocumentToJson(projectsync::currentDocument(*m_designer, *m_comboPaperSize, *m_chkLandscape, *m_spinPaperMargin, *m_spinPaperSpacing, m_currentFile)));
    }
}

void MainWindow::onSaveAs() {
    QString path = QFileDialog::getSaveFileName(this, "保存", "badges.bge", "バッジエディタファイル (*.bge)");
    if (path.isEmpty()) return;
    m_currentFile = path;
    onSave();
    updateTitle();
}

void MainWindow::onExportPdf() { QMessageBox::information(this, "PDF出力", "準備中"); }
void MainWindow::onExportPng() { QMessageBox::information(this, "画像出力", "準備中"); }

void MainWindow::onUndo() { m_undoStack->undo(); }
void MainWindow::onRedo() { m_undoStack->redo(); }

void MainWindow::onDelete() {
    m_designer->removeSelectedBadges();
    m_selected.clear();
    onBadgeDeselected();
    syncLayoutWorkspace();
}

void MainWindow::onToggleTheme() { m_isDark = !m_isDark; applyTheme(m_isDark); }

void MainWindow::onModeChanged(bool designer) {
    m_isDesigner = designer;
    m_actDesigner->setChecked(designer);
    m_actLayout->setChecked(!designer);
    if (designer) {
        openDesignerPerspective();
    } else {
        openLayoutPerspective();
    }
}

// --- Inspector ---
void MainWindow::onBadgeSelected(BadgeGraphicItem* item) {
    m_selected = {item};
    m_updatingUI = true;
    BadgeItem& b = item->badge();
    m_propX->setValue(b.xMm); m_propY->setValue(b.yMm);
    m_propW->setValue(b.widthMm); m_propH->setValue(b.heightMm);
    m_propRotation->setValue(int(b.rotation));
    m_propText->setText(b.displayText);
    m_propColorSpace->setText(item->colorSpaceLabel());
    m_propBrightness->setValue(int(b.brightness));
    m_propContrast->setValue(int(b.contrast));
    m_propSaturation->setValue(int(b.saturation));
    m_updatingUI = false;
}

void MainWindow::onBadgeDeselected() {
    m_selected.clear();
    m_updatingUI = true;
    m_propX->setValue(0); m_propY->setValue(0);
    m_propW->setValue(32); m_propH->setValue(32);
    m_propRotation->setValue(0);
    m_propText->clear();
    m_propColorSpace->setText("未選択");
    m_updatingUI = false;
}

void MainWindow::onBadgeMoved(BadgeGraphicItem* item) {
    if (m_selected.isEmpty() || m_selected.first() != item) return;
    m_updatingUI = true;
    BadgeItem& b = item->badge();
    m_propX->setValue(b.xMm);
    m_propY->setValue(b.yMm);
    m_updatingUI = false;
    syncLayoutWorkspace();
}

void MainWindow::onInspectorChanged() {
    if (m_updatingUI || m_selected.isEmpty()) return;
    BadgeItem& b = m_selected.first()->badge();
    b.xMm = m_propX->value(); b.yMm = m_propY->value();
    b.widthMm = m_propW->value(); b.heightMm = m_propH->value();
    b.rotation = m_propRotation->value();
    b.displayText = m_propText->text();
    b.brightness = m_propBrightness->value();
    b.contrast = m_propContrast->value();
    b.saturation = m_propSaturation->value();
    m_selected.first()->applyColorCorrection();
    m_selected.first()->syncFromBadge();
    m_propColorSpace->setText(m_selected.first()->colorSpaceLabel());
    m_designer->updateGuides(b.widthMm);
    syncLayoutWorkspace();
}

void MainWindow::onSetImage() {
    if (m_selected.isEmpty()) return;
    QString path = QFileDialog::getOpenFileName(this, "画像を選択", QString(), "すべての画像 (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tiff *.tif *.svg *.ico);;PNG (*.png);;JPEG (*.jpg *.jpeg)");
    if (path.isEmpty()) return;
    m_selected.first()->badge().imagePath = path;
    m_selected.first()->applyColorCorrection();
    m_selected.first()->syncFromBadge();
    m_propColorSpace->setText(m_selected.first()->colorSpaceLabel());
    syncLayoutWorkspace();
}

// --- Effects ---
void MainWindow::onGuideToggle() {
    m_designer->setBleedVisible(m_chkBleed->isChecked());
    m_designer->setVisibleVisible(m_chkVisible->isChecked());
    syncLayoutWorkspace();
}

void MainWindow::onLightingToggle(bool on) { m_designer->setLightingEnabled(on); }
void MainWindow::onGlitterToggle(bool on) { m_designer->setGlitterEnabled(on); }
void MainWindow::onGlitterPatternChanged(int idx) { m_designer->setGlitterPattern(idx); }
void MainWindow::onLightingSlider() {
    m_designer->setLightAngle(m_sliderLightAngle->value());
    m_designer->setLightIntensity(m_sliderLightIntensity->value() / 100.0);
}

// --- Badge ---
void MainWindow::onAddBadge() {
    BadgeItem b;
    QPointF center = m_designer->mapToScene(m_designer->viewport()->rect().center());
    const double mmToPx = 96.0 / 25.4;
    b.xMm = center.x() / mmToPx - b.widthMm / 2;
    b.yMm = center.y() / mmToPx - b.heightMm / 2;
    m_designer->addBadge(b);
    syncLayoutWorkspace();
}

void MainWindow::onBatchAdd() {
    BatchLayoutDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        for (int r = 0; r < dlg.rows(); ++r)
            for (int c = 0; c < dlg.cols(); ++c) {
                BadgeItem b; b.widthMm = dlg.badgeWidth(); b.heightMm = dlg.badgeHeight();
                b.xMm = 10 + c * (b.widthMm + 1);
                b.yMm = 10 + r * (b.heightMm + 1);
                b.clipToCircle = dlg.clipCircle();
                m_designer->addBadge(b);
            }
        syncLayoutWorkspace();
    }
}

void MainWindow::onAutoLayout() {
    auto document = projectsync::currentDocument(*m_designer, *m_comboPaperSize, *m_chkLandscape, *m_spinPaperMargin, *m_spinPaperSpacing, m_currentFile);
    document.badges = badge::auto_layout(document.badges, document.paper);
    projectsync::applyDocument(*m_designer, *m_layoutWorkspace, *m_comboPaperSize, *m_chkLandscape, *m_spinPaperMargin, *m_spinPaperSpacing, document);
    onModeChanged(false);
}

void MainWindow::onImageDropped(const QString& filePath) {
    BadgeItem b; b.imagePath = filePath;
    b.label = QFileInfo(filePath).baseName();
    m_designer->addBadge(b);
    syncLayoutWorkspace();
}

void MainWindow::syncLayoutWorkspace() {
    m_layoutWorkspace->setDocument(projectsync::currentDocument(*m_designer, *m_comboPaperSize, *m_chkLandscape, *m_spinPaperMargin, *m_spinPaperSpacing, m_currentFile));
}

void MainWindow::applyTheme(bool dark) {
    QPalette pal;
    if (dark) {
        pal.setColor(QPalette::Window, QColor(0x1E, 0x1E, 0x1E));
        pal.setColor(QPalette::WindowText, QColor(0xE0, 0xE0, 0xE0));
        pal.setColor(QPalette::Base, QColor(0x3F, 0x3F, 0x46));
        pal.setColor(QPalette::AlternateBase, QColor(0x2D, 0x2D, 0x30));
        pal.setColor(QPalette::ToolTipBase, QColor(0x2D, 0x2D, 0x30));
        pal.setColor(QPalette::ToolTipText, QColor(0xE0, 0xE0, 0xE0));
        pal.setColor(QPalette::Text, QColor(0xE0, 0xE0, 0xE0));
        pal.setColor(QPalette::Button, QColor(0x3F, 0x3F, 0x46));
        pal.setColor(QPalette::ButtonText, QColor(0xE0, 0xE0, 0xE0));
        pal.setColor(QPalette::BrightText, Qt::red);
        pal.setColor(QPalette::Link, QColor(0x55, 0x55, 0xFF));
        pal.setColor(QPalette::Highlight, QColor(0x55, 0x55, 0x66));
        pal.setColor(QPalette::HighlightedText, QColor(0xE0, 0xE0, 0xE0));
    } else {
        pal = QApplication::style()->standardPalette();
    }
    QApplication::setPalette(pal);
    m_actTheme->setText(dark ? "☀ テーマ切替" : "☾ テーマ切替");
}

void MainWindow::refreshBadges() { m_designer->refreshAll(); }

void MainWindow::updateTitle() {
    QString name = m_currentFile.isEmpty() ? "新規プロジェクト" : QFileInfo(m_currentFile).fileName();
    setWindowTitle(name + " - Badge Editor Pro");
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event) {
    for (const auto& url : event->mimeData()->urls()) {
        QString path = url.toLocalFile();
        QString ext = QFileInfo(path).suffix().toLower();
        if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp")
            onImageDropped(path);
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveDockState();
    QMainWindow::closeEvent(event);
}

void MainWindow::loadDockState() {
    QSettings settings;
    m_dockManager->loadPerspectives(settings);
    const QByteArray state = settings.value("dock/state").toByteArray();
    if (!state.isEmpty()) {
        m_dockManager->restoreState(state, 1);
    }
    const QString activePerspective = settings.value("dock/activePerspective", "designer").toString();
    if (activePerspective == "layout") {
        openLayoutPerspective();
    } else {
        openDesignerPerspective();
    }
}

void MainWindow::saveDockState() {
    QSettings settings;
    settings.setValue("dock/state", m_dockManager->saveState(1));
    m_dockManager->savePerspectives(settings);
    settings.setValue("dock/activePerspective", m_isDesigner ? "designer" : "layout");
    refreshPerspectiveMenu();
}

void MainWindow::resetDockState() {
    if (m_defaultDockState.isEmpty()) {
        return;
    }
    m_dockManager->restoreState(m_defaultDockState, 1);
    m_designerDock->setAsCurrentTab();
    m_isDesigner = true;
    m_actDesigner->setChecked(true);
    m_actLayout->setChecked(false);
    syncLayoutWorkspace();
}

void MainWindow::openDesignerPerspective() {
    if (!m_dockManager || !m_designerDock) {
        return;
    }
    m_designerDock->setAsCurrentTab();
    m_dockManager->openPerspective("designer");
    syncPerspectiveUi("designer");
}

void MainWindow::openLayoutPerspective() {
    if (!m_dockManager || !m_layoutDock) {
        return;
    }
    m_layoutDock->setAsCurrentTab();
    m_dockManager->openPerspective("layout");
    syncPerspectiveUi("layout");
}

void MainWindow::saveDesignerPerspective() {
    if (!m_dockManager) {
        return;
    }
    m_dockManager->addPerspective("designer");
    saveDockState();
}

void MainWindow::saveLayoutPerspective() {
    if (!m_dockManager) {
        return;
    }
    m_dockManager->addPerspective("layout");
    saveDockState();
}

void MainWindow::savePerspectiveAs() {
    if (!m_dockManager) {
        return;
    }
    const QString name = QInputDialog::getText(this, "Perspective を保存", "名前:");
    if (name.trimmed().isEmpty()) {
        return;
    }
    m_dockManager->addPerspective(name.trimmed());
    saveDockState();
    m_dockManager->openPerspective(name.trimmed());
    syncPerspectiveUi(name.trimmed());
}

void MainWindow::deleteSavedPerspective() {
    if (!m_dockManager) {
        return;
    }

    const QStringList names = m_dockManager->perspectiveNames();
    QStringList deletable;
    for (const auto& name : names) {
        if (name != "designer" && name != "layout") {
            deletable.append(name);
        }
    }

    if (deletable.isEmpty()) {
        QMessageBox::information(this, "Perspective", "削除できる保存済み perspective がありません");
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getItem(this, "Perspective を削除", "削除する名前:", deletable, 0, false, &ok);
    if (!ok || name.isEmpty()) {
        return;
    }

    m_dockManager->removePerspective(name);
    saveDockState();

    if (m_dockManager->perspectiveNames().contains("designer")) {
        m_dockManager->openPerspective("designer");
        syncPerspectiveUi("designer");
    } else {
        syncPerspectiveUi(QString());
    }
}

void MainWindow::openSavedPerspective() {
    auto* act = qobject_cast<QAction*>(sender());
    if (!act || !m_dockManager) {
        return;
    }
    const QString name = act->data().toString();
    if (name.isEmpty()) {
        return;
    }
    m_dockManager->openPerspective(name);
    syncPerspectiveUi(name);
}

void MainWindow::syncPerspectiveUi(const QString& name) {
    Q_UNUSED(name);
    const bool designer = !m_dockArea || m_dockArea->currentDockWidget() != m_layoutDock;
    m_isDesigner = designer;
    m_actDesigner->setChecked(designer);
    m_actLayout->setChecked(!designer);
    if (designer) {
        m_designer->setGuidesVisible(true);
    }
    syncLayoutWorkspace();
}

void MainWindow::refreshPerspectiveMenu() {
    if (!m_perspectiveMenu || !m_dockManager) {
        return;
    }

    m_savedPerspectiveMenu = nullptr;
    m_perspectiveMenu->clear();
    m_perspectiveMenu->addAction("現在を編集として保存", this, &MainWindow::saveDesignerPerspective);
    m_perspectiveMenu->addAction("現在を配置確認として保存", this, &MainWindow::saveLayoutPerspective);
    m_perspectiveMenu->addAction("名前を付けて保存...", this, &MainWindow::savePerspectiveAs);
    m_perspectiveMenu->addSeparator();
    m_perspectiveMenu->addAction("編集を開く", this, &MainWindow::openDesignerPerspective);
    m_perspectiveMenu->addAction("配置確認を開く", this, &MainWindow::openLayoutPerspective);

    const QStringList names = m_dockManager->perspectiveNames();
    if (!names.isEmpty()) {
        m_savedPerspectiveMenu = m_perspectiveMenu->addMenu("保存済み");
        m_savedPerspectiveMenu->addAction("削除...", this, &MainWindow::deleteSavedPerspective);
        m_savedPerspectiveMenu->addSeparator();
        for (const auto& name : names) {
            auto* action = m_savedPerspectiveMenu->addAction(name, this, &MainWindow::openSavedPerspective);
            action->setData(name);
        }
    }
}

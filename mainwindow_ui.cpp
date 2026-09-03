#include "mainwindow.h"

#include "appsettingsdialog.h"
#include <DockManager.h>
#include <DockWidget.h>

#include <QDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QShowEvent>
#include <QCloseEvent>
#include <QSettings>
#include <algorithm>

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
    if (!m_backdropApplied) {
        applyWindowsBackdrop();
        m_backdropApplied = true;
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    saveAppSettings();
    saveDockState();
    QMainWindow::closeEvent(event);
}

void MainWindow::openDesignerPerspective() {
    if (!m_dockManager || !m_designerDock) return;
    refreshDocumentFromDesigner();
    m_designerDock->setAsCurrentTab();
    m_dockManager->openPerspective("designer");
    syncPerspectiveUiDeferred("designer");
}

void MainWindow::openLayoutPerspective() {
    if (!m_dockManager || !m_layoutDock) return;
    m_layoutDock->setAsCurrentTab();
    m_dockManager->openPerspective("layout");
    syncPerspectiveUiDeferred("layout");
}

void MainWindow::syncPerspectiveUiDeferred(const QString& name) {
    QTimer::singleShot(0, this, [this, name]() {
        syncPerspectiveUi(name);
    });
}

void MainWindow::saveDesignerPerspective() {
    if (!m_dockManager) return;
    m_dockManager->addPerspective("designer");
    saveDockState();
}

void MainWindow::saveLayoutPerspective() {
    if (!m_dockManager) return;
    m_dockManager->addPerspective("layout");
    saveDockState();
}

void MainWindow::savePerspectiveAs() {
    if (!m_dockManager) return;
    const QString name = QInputDialog::getText(this, "Perspective を保存", "名前:");
    if (name.trimmed().isEmpty()) return;
    m_dockManager->addPerspective(name.trimmed());
    saveDockState();
    m_dockManager->openPerspective(name.trimmed());
    syncPerspectiveUi(name.trimmed());
}

void MainWindow::deleteSavedPerspective() {
    if (!m_dockManager) return;
    const QStringList names = m_dockManager->perspectiveNames();
    QStringList deletable;
    for (const auto& name : names) {
        if (name != "designer" && name != "layout") deletable.append(name);
    }
    if (deletable.isEmpty()) {
        QMessageBox::information(this, "Perspective", "削除できる保存済み perspective がありません");
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getItem(this, "Perspective を削除", "削除する名前:", deletable, 0, false, &ok);
    if (!ok || name.isEmpty()) return;
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
    if (!act || !m_dockManager) return;
    const QString name = act->data().toString();
    if (name.isEmpty()) return;
    m_dockManager->openPerspective(name);
    syncPerspectiveUiDeferred(name);
}

void MainWindow::resetDockState() {
    if (m_defaultDockState.isEmpty()) return;
    m_dockManager->restoreState(m_defaultDockState, 1);
    m_designerDock->setAsCurrentTab();
    m_isDesigner = true;
    m_actDesigner->setChecked(true);
    m_actLayout->setChecked(false);
    updateInspectorMode();
    requestLayoutRefresh("dock state reset");
    flushInternalEvents();
}

void MainWindow::loadDockState() {
    QSettings settings;
    m_dockManager->loadPerspectives(settings);
    const QByteArray state = settings.value("dock/state").toByteArray();
    if (!state.isEmpty()) m_dockManager->restoreState(state, 1);
    const QString activePerspective = settings.value("dock/activePerspective", "designer").toString();
    if (activePerspective == "layout") openLayoutPerspective();
    else openDesignerPerspective();
}

void MainWindow::saveDockState() {
    QSettings settings;
    settings.setValue("dock/state", m_dockManager->saveState(1));
    m_dockManager->savePerspectives(settings);
    settings.setValue("dock/activePerspective", m_isDesigner ? "designer" : "layout");
    refreshPerspectiveMenu();
}

void MainWindow::saveAppSettings() {
    QSettings settings;
    settings.setValue("app/darkTheme", m_appSettings.darkTheme);
    settings.setValue("app/gridVisible", m_appSettings.gridVisible);
    settings.setValue("app/snapToGrid", m_appSettings.snapToGrid);
    settings.setValue("app/gridSpacingMm", m_appSettings.gridSpacingMm);
    settings.setValue("app/lightingEnabled", m_appSettings.lightingEnabled);
    settings.setValue("app/lightAngle", m_appSettings.lightAngle);
    settings.setValue("app/lightIntensity", m_appSettings.lightIntensity);
    settings.setValue("app/glitterEnabled", m_appSettings.glitterEnabled);
    settings.setValue("app/glitterPattern", m_appSettings.glitterPattern);
    settings.setValue("app/printResolution", std::max(72, m_appSettings.printResolution));
    settings.setValue("app/experimentalGpuViewport", m_appSettings.experimentalGpuViewport);
    settings.setValue("app/duplicateOffsetMm", std::max(0.0, m_appSettings.duplicateOffsetMm));
}

void MainWindow::loadAppSettings() {
    QSettings settings;
    AppSettings loaded;
    loaded.darkTheme = settings.value("app/darkTheme", loaded.darkTheme).toBool();
    loaded.gridVisible = settings.value("app/gridVisible", loaded.gridVisible).toBool();
    loaded.snapToGrid = settings.value("app/snapToGrid", loaded.snapToGrid).toBool();
    loaded.gridSpacingMm = settings.value("app/gridSpacingMm", loaded.gridSpacingMm).toDouble();
    loaded.lightingEnabled = settings.value("app/lightingEnabled", loaded.lightingEnabled).toBool();
    loaded.lightAngle = settings.value("app/lightAngle", loaded.lightAngle).toInt();
    loaded.lightIntensity = settings.value("app/lightIntensity", loaded.lightIntensity).toInt();
    loaded.glitterEnabled = settings.value("app/glitterEnabled", loaded.glitterEnabled).toBool();
    loaded.glitterPattern = settings.value("app/glitterPattern", loaded.glitterPattern).toInt();
    loaded.printResolution = settings.value("app/printResolution", loaded.printResolution).toInt();
    loaded.experimentalGpuViewport = settings.value("app/experimentalGpuViewport", loaded.experimentalGpuViewport).toBool();
    loaded.duplicateOffsetMm = settings.value("app/duplicateOffsetMm", loaded.duplicateOffsetMm).toDouble();
    applyAppSettings(loaded);
}

void MainWindow::onToggleTheme() {
    m_appSettings.darkTheme = !m_isDark;
    applyTheme(m_appSettings.darkTheme);
    saveAppSettings();
}

void MainWindow::onAppSettings() {
    AppSettingsDialog dlg(m_appSettings, this);
    if (dlg.exec() != QDialog::Accepted) return;
    applyAppSettings(dlg.settings());
    saveAppSettings();
}

void MainWindow::onToggleGrid(bool on) {
    if (m_designer) m_designer->setGridVisible(on);
    m_appSettings.gridVisible = on;
    saveAppSettings();
}

void MainWindow::onToggleSnapToGrid(bool on) {
    if (m_designer) m_designer->setSnapToGrid(on);
    m_appSettings.snapToGrid = on;
    saveAppSettings();
}

void MainWindow::onModeChanged(bool designer) {
    m_isDesigner = designer;
    m_actDesigner->setChecked(designer);
    m_actLayout->setChecked(!designer);
    updateInspectorMode();
    updateToolbarsForMode();
    if (designer) {
        openDesignerPerspective();
    } else {
        requestLayoutRefresh("mode changed to layout");
        flushInternalEvents();
        openLayoutPerspective();
    }
}

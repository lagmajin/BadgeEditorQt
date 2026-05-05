#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QLabel>
#include <QSpinBox>
#include <QLineEdit>
#include <QMenu>
#include <QToolBar>
#include <QUndoStack>
#include <QByteArray>
#include "badgeitem.h"
#include "designerwidget.h"
#include "layoutworkspacewidget.h"

namespace ads {
class CDockManager;
class CDockWidget;
class CDockAreaWidget;
}

class BadgeGraphicItem;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    // File
    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onExportPdf();
    void onExportPng();

    // Edit
    void onUndo();
    void onRedo();
    void onDelete();

    // View
    void onToggleTheme();
    void onModeChanged(bool designer);

    // Inspector
    void onBadgeSelected(BadgeGraphicItem* item);
    void onBadgeDeselected();
    void onBadgeMoved(BadgeGraphicItem* item);
    void onInspectorChanged();
    void onSetImage();

    // Effects
    void onGuideToggle();
    void onLightingToggle(bool on);
    void onGlitterToggle(bool on);
    void onGlitterPatternChanged(int idx);
    void onLightingSlider();

    // Badge
    void onAddBadge();
    void onBatchAdd();
    void onImageDropped(const QString& filePath);
    void onAutoLayout();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void applyTheme(bool dark);
    void refreshBadges();
    void syncLayoutWorkspace();
    void updateTitle();
    void loadDockState();
    void saveDockState();
    void resetDockState();
    void openDesignerPerspective();
    void openLayoutPerspective();
    void saveDesignerPerspective();
    void saveLayoutPerspective();
    void syncPerspectiveUi(const QString& name);
    void savePerspectiveAs();
    void openSavedPerspective();
    void deleteSavedPerspective();
    void refreshPerspectiveMenu();

    // UI
    ads::CDockManager* m_dockManager = nullptr;
    ads::CDockAreaWidget* m_dockArea = nullptr;
    ads::CDockWidget* m_inspectorDock = nullptr;
    ads::CDockWidget* m_designerDock = nullptr;
    ads::CDockWidget* m_layoutDock = nullptr;
    QWidget* m_inspector;
    DesignerWidget* m_designer;
    LayoutWorkspaceWidget* m_layoutWorkspace;

    // Toolbar
    QToolBar* m_toolbar;
    QAction* m_actDesigner;
    QAction* m_actLayout;
    QLabel* m_zoomLabel;
    QAction* m_actTheme;
    QMenu* m_perspectiveMenu = nullptr;
    QMenu* m_savedPerspectiveMenu = nullptr;

    // Inspector - properties
    QDoubleSpinBox* m_propX;
    QDoubleSpinBox* m_propY;
    QDoubleSpinBox* m_propW;
    QDoubleSpinBox* m_propH;
    QSlider* m_propRotation;
    QLineEdit* m_propText;
    QLineEdit* m_propColorSpace;

    // Inspector - color correction
    QSlider* m_propBrightness;
    QSlider* m_propContrast;
    QSlider* m_propSaturation;

    // Inspector - guides & effects
    QCheckBox* m_chkBleed;
    QCheckBox* m_chkVisible;
    QCheckBox* m_chkLighting;
    QCheckBox* m_chkGlitter;
    QComboBox* m_comboGlitter;
    QSlider* m_sliderLightAngle;
    QSlider* m_sliderLightIntensity;

    // Layout config
    QComboBox* m_comboPaperSize;
    QCheckBox* m_chkLandscape;
    QDoubleSpinBox* m_spinPaperMargin;
    QDoubleSpinBox* m_spinPaperSpacing;

    // State
    QList<BadgeGraphicItem*> m_selected;
    QString m_currentFile;
    QUndoStack* m_undoStack;
    QByteArray m_defaultDockState;
    double m_zoomScale = 1.0;
    bool m_isDark = true;
    bool m_updatingUI = false;
    bool m_isDesigner = true;
};

#endif

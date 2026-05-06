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
#include <QShowEvent>
#include <functional>
#include <wobjectdefs.h>
#include "appsettingsdialog.h"
#include "dockstylemanager.h"
#include "badgeitem.h"
#include "designerwidget.h"
#include "layoutworkspacewidget.h"
#include "transferdebugdialog.h"

namespace ads {
class CDockManager;
class CDockWidget;
class CDockAreaWidget;
}

class BadgeGraphicItem;
class QListWidget;
class QGroupBox;
class MainWindow : public QMainWindow {
    W_OBJECT(MainWindow)
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
    void onPrintPreview();
    void onPrint();

    // Edit
    void onUndo();
    void onRedo();
    void onDelete();

    // View
    void onToggleTheme();
    void onAppSettings();
    void onToggleGrid(bool on);
    void onToggleSnapToGrid(bool on);
    void onModeChanged(bool designer);

    // Inspector
    void onBadgeSelected(BadgeGraphicItem* item);
    void onSelectionChanged();
    void onBadgeDeselected();
    void onBadgeMoved(BadgeGraphicItem* item);
    void onInspectorChanged();
    void onSetImage();
    void onNudgeRequested(double dxMm, double dyMm);
    void onAlignLeft();
    void onAlignHCenter();
    void onAlignRight();
    void onAlignTop();
    void onAlignVCenter();
    void onAlignBottom();

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
    void onSendToLayout();
    void onClearLayout();
    void onShowTransferDebug();

protected:
    void showEvent(QShowEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void applyTheme(bool dark);
    void applyAppSettings(const AppSettings& settings);
    void applyWindowsBackdrop();
    void refreshBadges();
    void refreshLayerList();
    void updateInspectorMode();
    void syncLayoutWorkspace();
    void refreshDocumentFromDesigner();
    void applyDesignerBadges(const QList<BadgeItem>& badges, const QList<int>& selectedIndices = {});
    QList<int> selectedBadgeIndices() const;
    QList<BadgeItem> currentDesignerBadges() const;
    void pushBadgeChange(const QString& label, const QList<BadgeItem>& beforeBadges, const QList<int>& beforeSelection, const QList<BadgeItem>& afterBadges, const QList<int>& afterSelection);
    void appendLog(const QString& message);
    void refreshDiagnostics();
    void updateTitle();
    void loadDockState();
    void saveDockState();
    void loadAppSettings();
    void saveAppSettings();
    void loadPrintSettings();
    void savePrintSettings();
    void resetDockState();
    void openDesignerPerspective();
    void openLayoutPerspective();
    void syncPerspectiveUiDeferred(const QString& name);
    void saveDesignerPerspective();
    void saveLayoutPerspective();
    void syncPerspectiveUi(const QString& name);
    void savePerspectiveAs();
    void openSavedPerspective();
    void deleteSavedPerspective();
    void refreshPerspectiveMenu();
    void updateToolbarsForMode();

    // UI
    ads::CDockManager* m_dockManager = nullptr;
    ads::CDockAreaWidget* m_dockArea = nullptr;
    ads::CDockWidget* m_inspectorDock = nullptr;
    ads::CDockWidget* m_designerDock = nullptr;
    ads::CDockWidget* m_layoutDock = nullptr;
    DockStyleManager* m_dockStyleManager = nullptr;
    QWidget* m_inspector;
    DesignerWidget* m_designer;
    LayoutWorkspaceWidget* m_layoutWorkspace;

    // Toolbar
    QToolBar* m_commonToolbar = nullptr;
    QToolBar* m_designerToolbar = nullptr;
    QToolBar* m_layoutToolbar = nullptr;
    QAction* m_actDesigner;
    QAction* m_actLayout;
    QLabel* m_zoomLabel;
    QAction* m_actTheme;
    QAction* m_actAppSettings = nullptr;
    QAction* m_actTransferDebug = nullptr;
    QAction* m_actPrintPreview = nullptr;
    QAction* m_actPrint = nullptr;
    QAction* m_actGridVisible = nullptr;
    QAction* m_actSnapToGrid = nullptr;
    QMenu* m_perspectiveMenu = nullptr;
    QMenu* m_savedPerspectiveMenu = nullptr;
    QAction* m_actAddBadge = nullptr;
    QAction* m_actBatchAdd = nullptr;
    QAction* m_actSendToLayout = nullptr;
    QAction* m_actClearLayout = nullptr;
    QAction* m_actOpenDesignerPerspective = nullptr;
    QAction* m_actOpenLayoutPerspective = nullptr;
    QAction* m_actZoomIn = nullptr;
    QAction* m_actZoomOut = nullptr;

    // Inspector - properties
    QDoubleSpinBox* m_propX;
    QDoubleSpinBox* m_propY;
    QDoubleSpinBox* m_propW;
    QDoubleSpinBox* m_propH;
    QDoubleSpinBox* m_propImageScale = nullptr;
    QComboBox* m_sizePreset = nullptr;
    QSlider* m_propRotation;
    QLineEdit* m_propText;
    QCheckBox* m_propClipCircle = nullptr;
    QLineEdit* m_propColorSpace;
    QListWidget* m_layerList = nullptr;
    QGroupBox* m_propGroup = nullptr;
    QGroupBox* m_colorGroup = nullptr;
    QGroupBox* m_layerGroup = nullptr;
    QGroupBox* m_guideGroup = nullptr;
    QGroupBox* m_effectGroup = nullptr;
    QGroupBox* m_layoutGroup = nullptr;
    ads::CDockWidget* m_logDock = nullptr;
    QListWidget* m_logList = nullptr;
    QListWidget* m_issueList = nullptr;
    QListWidget* m_linkList = nullptr;

    // Inspector - color correction
    QSlider* m_propBrightness;
    QSlider* m_propContrast;
    QSlider* m_propSaturation;

    // Inspector - guides & effects
    QCheckBox* m_chkBleed;
    QCheckBox* m_chkVisible;
    QCheckBox* m_chkLighting;
    QCheckBox* m_chkGlitter;
    QComboBox* m_comboMaterial = nullptr;
    QComboBox* m_comboGlitter;
    QSlider* m_sliderSpecular = nullptr;
    QSlider* m_sliderEnvReflection = nullptr;
    QSlider* m_sliderGlitterStrength = nullptr;
    QSlider* m_sliderLightAngle;
    QSlider* m_sliderLightIntensity;

    // Layout config
    QComboBox* m_comboPaperSize;
    QCheckBox* m_chkLandscape;
    QDoubleSpinBox* m_spinPaperMargin;
    QDoubleSpinBox* m_spinPaperSpacing;
    QCheckBox* m_chkCutFriendlyLayout = nullptr;

    // State
    QList<BadgeGraphicItem*> m_selected;
    QList<BadgeItem> m_badges;
    QList<BadgeItem> m_layoutBadges;
    enum class LayoutPreviewMode {
        CurrentDesign,
        FillPageFromSelection,
        AutoLayoutAll,
    };
    LayoutPreviewMode m_layoutPreviewMode = LayoutPreviewMode::CurrentDesign;
    QImage m_lastTransferDesignerImage;
    QImage m_lastTransferLayoutImage;
    QString m_lastTransferDebugTitle;
    QString m_lastTransferDebugDetail;
    TransferDebugDialog* m_transferDebugDialog = nullptr;
    double m_paperWidthMm = 210.0;
    double m_paperHeightMm = 297.0;
    double m_paperMarginMm = 10.0;
    double m_paperSpacingMm = 1.0;
    QString m_currentFile;
    QUndoStack* m_undoStack;
    QByteArray m_defaultDockState;
    double m_zoomScale = 1.0;
    bool m_isDark = true;
    AppSettings m_appSettings;
    bool m_updatingUI = false;
    bool m_isDesigner = true;
    bool m_backdropApplied = false;
    bool m_skipNextLayoutSync = false;
    int m_printResolution = 300;
};

#endif

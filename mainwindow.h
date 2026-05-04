#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QStackedWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QLabel>
#include <QSpinBox>
#include <QLineEdit>
#include <QToolBar>
#include <QUndoStack>
#include "badgeitem.h"
#include "designerwidget.h"

class BadgeGraphicItem;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
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

    void setupLayoutMode();
    void updatePaperCanvas();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void applyTheme(bool dark);
    void refreshBadges();
    void syncLayoutBadges();
    void saveState();
    void updateTitle();
    QList<BadgeItem> collectBadges() const;

    // UI
    QSplitter* m_splitter;
    QWidget* m_inspector;
    DesignerWidget* m_designer;
    QGraphicsView* m_layoutView;
    QGraphicsScene* m_layoutScene;
    QStackedWidget* m_stack;

    // Toolbar
    QToolBar* m_toolbar;
    QAction* m_actDesigner;
    QAction* m_actLayout;
    QLabel* m_zoomLabel;
    QAction* m_actTheme;

    // Inspector - properties
    QDoubleSpinBox* m_propX;
    QDoubleSpinBox* m_propY;
    QDoubleSpinBox* m_propW;
    QDoubleSpinBox* m_propH;
    QSlider* m_propRotation;
    QLineEdit* m_propText;

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

    // State
    QList<BadgeItem> m_badges;
    QList<BadgeGraphicItem*> m_selected;
    QString m_currentFile;
    QUndoStack* m_undoStack;
    double m_zoomScale = 1.0;
    bool m_isDark = true;
    bool m_updatingUI = false;
    bool m_isDesigner = true;
};

#endif

#include "mainwindow.h"
#include "badgegraphicitem.h"
#include "batchlayoutdialog.h"
#include "layoutengine.h"
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
#include <QJsonDocument>
#include <QJsonArray>
#include <QApplication>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QPushButton>
#include <DockManager.h>
#include <DockWidget.h>
#include <QFileInfo>
#include <cmath>

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

    auto* viewMenu = menuBar()->addMenu("表示(&V)");
    viewMenu->addAction("ズームイン", this, [this]{ m_zoomScale *= 1.2; m_zoomLabel->setText(QString::number(m_zoomScale*100,'f',0)+"%"); });
    viewMenu->addAction("ズームアウト", this, [this]{ m_zoomScale /= 1.2; m_zoomLabel->setText(QString::number(m_zoomScale*100,'f',0)+"%"); });
    viewMenu->addSeparator();
    m_actTheme = viewMenu->addAction("テーマ切替", this, &MainWindow::onToggleTheme);

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
    m_zoomLabel = new QLabel("100%");
    m_toolbar->addWidget(m_zoomLabel);

    // --- Central Stack ---
    m_stack = new QStackedWidget;

    // Designer
    m_designer = new DesignerWidget;
    connect(m_designer, &DesignerWidget::badgeSelected, this, &MainWindow::onBadgeSelected);
    connect(m_designer, &DesignerWidget::badgeDeselected, this, &MainWindow::onBadgeDeselected);
    connect(m_designer, &DesignerWidget::badgeDoubleClicked, this, [this](BadgeGraphicItem*){ onSetImage(); });
    m_designer->updateGuides(32);
    m_stack->addWidget(m_designer);

    // Layout
    m_layoutScene = new QGraphicsScene(this);
    m_layoutView = new QGraphicsView(m_layoutScene);
    m_layoutView->setRenderHints(QPainter::Antialiasing);
    m_layoutView->setBackgroundBrush(Qt::darkGray);
    m_stack->addWidget(m_layoutView);

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
    propForm->addRow("回転:", m_propRotation); propForm->addRow("テキスト:", m_propText);
    auto* imgBtn = new QPushButton("画像を変更...");
    connect(imgBtn, &QPushButton::clicked, this, &MainWindow::onSetImage);
    propForm->addRow(imgBtn);
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
    layoutForm->addRow("用紙:", m_comboPaperSize);
    layoutForm->addRow(m_chkLandscape);
    inspLayout->addWidget(layoutGroup);

    inspLayout->addStretch();
    scroll->setWidget(m_inspector);

    // --- Docking System ---
    ads::CDockManager::setConfigFlag(ads::CDockManager::OpaqueSplitterResize, true);
    ads::CDockManager::setConfigFlag(ads::CDockManager::XmlCompressionEnabled, false);
    ads::CDockManager::setConfigFlag(ads::CDockManager::FocusHighlighting, true);
    m_dockManager = new ads::CDockManager(this);

    m_workspaceDock = new ads::CDockWidget("ワークスペース");
    m_workspaceDock->setWidget(m_stack);
    m_workspaceDock->setFeature(ads::CDockWidget::DockWidgetClosable, false);

    m_inspectorDock = new ads::CDockWidget("インスペクター");
    m_inspectorDock->setWidget(scroll);
    m_inspectorDock->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromContent);

    auto* area = m_dockManager->setCentralWidget(m_workspaceDock);
    m_dockManager->addDockWidget(ads::RightDockWidgetArea, m_inspectorDock, area);

    applyTheme(true);
    onModeChanged(true);
    updateTitle();
}

// --- File slots ---
void MainWindow::onNew() {
    m_badges.clear();
    m_currentFile.clear();
    while (!m_designer->graphicItems().isEmpty()) { auto* gi = m_designer->graphicItems().first(); m_designer->graphicItems().removeFirst(); gi->scene()->removeItem(gi); delete gi; }
    m_designer->addBadge(BadgeItem{});
    m_designer->updateGuides(32);
    updateTitle();
}

void MainWindow::onOpen() {
    QString path = QFileDialog::getOpenFileName(this, "開く", QString(), "バッジエディタファイル (*.bge *.json)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        m_badges = BadgeItem::fromJsonArray(doc.array());
        onNew();
        for (const auto& b : m_badges) m_designer->addBadge(b);
        m_currentFile = path;
        updateTitle();
    }
}

void MainWindow::onSave() {
    if (m_currentFile.isEmpty()) { onSaveAs(); return; }
    QFile f(m_currentFile);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(BadgeItem::toJsonArray(collectBadges())).toJson());
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
}

void MainWindow::onToggleTheme() { m_isDark = !m_isDark; applyTheme(m_isDark); }

void MainWindow::onModeChanged(bool designer) {
    m_isDesigner = designer;
    m_actDesigner->setChecked(designer);
    m_actLayout->setChecked(!designer);
    m_designer->setVisible(designer);
    m_layoutView->setVisible(!designer);
    if (designer) {
        m_designer->setGuidesVisible(true);
    } else {
        setupLayoutMode();
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
    m_updatingUI = false;
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
    m_designer->updateGuides(b.widthMm);
}

void MainWindow::onSetImage() {
    if (m_selected.isEmpty()) return;
    QString path = QFileDialog::getOpenFileName(this, "画像を選択", QString(), "画像 (*.png *.jpg *.bmp)");
    if (path.isEmpty()) return;
    m_selected.first()->badge().imagePath = path;
    m_selected.first()->applyColorCorrection();
    m_selected.first()->syncFromBadge();
}

// --- Effects ---
void MainWindow::onGuideToggle() {
    m_designer->setBleedVisible(m_chkBleed->isChecked());
    m_designer->setVisibleVisible(m_chkVisible->isChecked());
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
    m_badges.append(b);
    m_designer->addBadge(b);
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
                m_badges.append(b);
                m_designer->addBadge(b);
            }
    }
}

void MainWindow::onAutoLayout() {
    PaperConfig cfg;
    cfg.widthMm = m_comboPaperSize->currentIndex() == 1 ? 182 : 210;
    cfg.heightMm = m_comboPaperSize->currentIndex() == 1 ? 257 : 297;
    if (m_chkLandscape->isChecked()) std::swap(cfg.widthMm, cfg.heightMm);
    m_badges = LayoutEngine::autoLayout(collectBadges(), cfg);
    onModeChanged(false);
}

void MainWindow::onImageDropped(const QString& filePath) {
    BadgeItem b; b.imagePath = filePath;
    b.label = QFileInfo(filePath).baseName();
    m_badges.append(b);
    m_designer->addBadge(b);
}

void MainWindow::setupLayoutMode() {
    m_layoutScene->clear();
    const double mmToPx = 96.0 / 25.4;
    PaperConfig cfg;
    cfg.widthMm = m_comboPaperSize->currentIndex() == 1 ? 182 : 210;
    cfg.heightMm = m_comboPaperSize->currentIndex() == 1 ? 257 : 297;
    if (m_chkLandscape->isChecked()) std::swap(cfg.widthMm, cfg.heightMm);
    updatePaperCanvas();

    auto* paper = m_layoutScene->addRect(0, 0, cfg.widthMm * mmToPx, cfg.heightMm * mmToPx, QPen(Qt::black), QBrush(Qt::white));

    // Safe area
    QPen safePen(Qt::red, 1, Qt::DashLine);
    safePen.setDashPattern({5, 5});
    m_layoutScene->addRect(5 * mmToPx, 5 * mmToPx, (cfg.widthMm - 10) * mmToPx, (cfg.heightMm - 10) * mmToPx, safePen, Qt::NoBrush);

    syncLayoutBadges();
}

void MainWindow::updatePaperCanvas() {
    const double mmToPx = 96.0 / 25.4;
    double w = m_comboPaperSize->currentIndex() == 1 ? 182 : 210;
    double h = m_comboPaperSize->currentIndex() == 1 ? 257 : 297;
    if (m_chkLandscape->isChecked()) std::swap(w, h);
    m_layoutScene->setSceneRect(-50, -50, w * mmToPx + 100, h * mmToPx + 100);
}

void MainWindow::syncLayoutBadges() {
    const double mmToPx = 96.0 / 25.4;
    for (const auto& b : collectBadges()) {
        auto* rect = m_layoutScene->addRect(0, 0, b.widthMm * mmToPx, b.heightMm * mmToPx, QPen(Qt::black), QBrush(Qt::cyan));
        rect->setPos(b.xMm * mmToPx, b.yMm * mmToPx);
    }
}

QList<BadgeItem> MainWindow::collectBadges() const {
    QList<BadgeItem> list;
    for (auto* gi : m_designer->graphicItems())
        list.append(gi->badge());
    return list;
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

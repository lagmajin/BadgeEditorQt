#include "mixedlayoutdialog.h"

#include <QCheckBox>
#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QWindow>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QPalette>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW static_cast<DWM_SYSTEMBACKDROP_TYPE>(3)
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND static_cast<DWM_WINDOW_CORNER_PREFERENCE>(2)
#endif
#endif

namespace {

QString badgeModeText(ProductMode mode) {
    switch (mode) {
    case ProductMode::Sticker:
        return QStringLiteral("Sticker");
    case ProductMode::Badge:
    default:
        return QStringLiteral("Badge");
    }
}

#ifdef Q_OS_WIN
static void applyWinBackdrop(HWND hwnd) {
    if (!hwnd) {
        return;
    }
    const DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_TRANSIENTWINDOW;
    const DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
}
#endif

QString templateName(const BadgeItem& badge) {
    if (!badge.label.isEmpty()) {
        return badge.label;
    }
    if (!badge.displayText.isEmpty()) {
        return badge.displayText;
    }
    if (!badge.imagePath.isEmpty()) {
        return QFileInfo(badge.imagePath).baseName();
    }
    if (!badge.layers.isEmpty() && !badge.layers.first().name.isEmpty()) {
        return badge.layers.first().name;
    }
    return QStringLiteral("Untitled");
}

QString templateSizeText(const BadgeItem& badge) {
    return QStringLiteral("%1 × %2 mm")
        .arg(QString::number(badge.widthMm, 'f', 1), QString::number(badge.heightMm, 'f', 1));
}

} // namespace

MixedLayoutDialog::MixedLayoutDialog(const QList<BadgeItem>& templates, QWidget* parent)
    : QDialog(parent), m_templates(templates) {
    setWindowTitle(QStringLiteral("混在面付け"));
    setAttribute(Qt::WA_StyledBackground, true);

    auto* root = new QVBoxLayout(this);

    auto* info = new QLabel(QStringLiteral("選択中のバッジやステッカーをまとめて面付けします。各テンプレートの枚数を指定してください。"));
    info->setWordWrap(true);
    root->addWidget(info);

    m_table = new QTableWidget;
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({QStringLiteral("テンプレート"), QStringLiteral("サイズ"), QStringLiteral("種類"), QStringLiteral("枚数")});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setVisible(false);
    m_table->setAlternatingRowColors(true);
    m_table->setRowCount(m_templates.size());

    for (int row = 0; row < m_templates.size(); ++row) {
        const auto& badge = m_templates[row];
        auto* nameItem = new QTableWidgetItem(templateName(badge));
        auto* sizeItem = new QTableWidgetItem(templateSizeText(badge));
        auto* kindItem = new QTableWidgetItem(badgeModeText(badge.productMode));
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        sizeItem->setFlags(sizeItem->flags() & ~Qt::ItemIsEditable);
        kindItem->setFlags(kindItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, 0, nameItem);
        m_table->setItem(row, 1, sizeItem);
        m_table->setItem(row, 2, kindItem);

        auto* countSpin = new QSpinBox;
        countSpin->setRange(1, 999);
        countSpin->setValue(1);
        countSpin->setSuffix(QStringLiteral(" 枚"));
        connect(countSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) {
            refreshSummary();
        });
        m_table->setCellWidget(row, 3, countSpin);
    }
    root->addWidget(m_table);

    auto* optionsRow = new QHBoxLayout;
    m_sortBySize = new QCheckBox(QStringLiteral("大きい順に配置する"));
    m_sortBySize->setChecked(true);
    optionsRow->addWidget(m_sortBySize);
    optionsRow->addStretch(1);
    root->addLayout(optionsRow);

    m_summary = new QLabel;
    m_summary->setWordWrap(true);
    auto summaryPalette = m_summary->palette();
    summaryPalette.setColor(QPalette::WindowText, summaryPalette.color(QPalette::Mid));
    summaryPalette.setColor(QPalette::Text, summaryPalette.color(QPalette::Mid));
    m_summary->setPalette(summaryPalette);
    root->addWidget(m_summary);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    refreshSummary();
}

QList<BadgeItem> MixedLayoutDialog::expandedTemplates() const {
    QList<BadgeItem> result;
    if (!m_table) {
        return result;
    }

    for (int row = 0; row < m_templates.size(); ++row) {
        auto* countSpin = qobject_cast<QSpinBox*>(m_table->cellWidget(row, 3));
        const int count = countSpin ? countSpin->value() : 1;
        for (int i = 0; i < count; ++i) {
            result.append(m_templates[row]);
        }
    }
    return result;
}

bool MixedLayoutDialog::sortBySize() const {
    return m_sortBySize && m_sortBySize->isChecked();
}

void MixedLayoutDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    if (!m_backdropApplied) {
        applyBackdrop();
        m_backdropApplied = true;
    }
}

void MixedLayoutDialog::applyBackdrop() {
#ifdef Q_OS_WIN
    if (windowHandle()) {
        applyWinBackdrop(HWND(windowHandle()->winId()));
    }
#endif
}

void MixedLayoutDialog::refreshSummary() {
    if (!m_summary || !m_table) {
        return;
    }
    int totalCopies = 0;
    for (int row = 0; row < m_templates.size(); ++row) {
        auto* countSpin = qobject_cast<QSpinBox*>(m_table->cellWidget(row, 3));
        totalCopies += countSpin ? countSpin->value() : 1;
    }
    m_summary->setText(QStringLiteral("%1 種類 / %2 枚")
                           .arg(QString::number(m_templates.size()),
                                QString::number(totalCopies)));
}

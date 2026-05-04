#include "batchlayoutdialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>

BatchLayoutDialog::BatchLayoutDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("バッジ一括作成");
    auto* form = new QFormLayout(this);

    m_width = new QDoubleSpinBox;
    m_width->setRange(10, 100); m_width->setValue(32); m_width->setSuffix(" mm");
    form->addRow("幅:", m_width);

    m_height = new QDoubleSpinBox;
    m_height->setRange(10, 100); m_height->setValue(32); m_height->setSuffix(" mm");
    form->addRow("高さ:", m_height);

    m_rows = new QSpinBox; m_rows->setRange(1, 20); m_rows->setValue(3);
    form->addRow("行数:", m_rows);

    m_cols = new QSpinBox; m_cols->setRange(1, 20); m_cols->setValue(5);
    form->addRow("列数:", m_cols);

    m_clip = new QCheckBox("円形クリップ"); m_clip->setChecked(true);
    form->addRow(m_clip);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    form->addRow(buttons);
}

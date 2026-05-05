#ifndef BATCHLAYOUTDIALOG_H
#define BATCHLAYOUTDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include "badgeitem.h"

class BatchLayoutDialog : public QDialog {
public:
    explicit BatchLayoutDialog(QWidget* parent = nullptr);
    double badgeWidth() const { return m_width->value(); }
    double badgeHeight() const { return m_height->value(); }
    int rows() const { return m_rows->value(); }
    int cols() const { return m_cols->value(); }
    bool clipCircle() const { return m_clip->isChecked(); }
private:
    QDoubleSpinBox* m_width;
    QDoubleSpinBox* m_height;
    QSpinBox* m_rows;
    QSpinBox* m_cols;
    QCheckBox* m_clip;
};

#endif

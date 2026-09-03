#include "mainwindow.h"

#include <QColor>
#include <QPalette>
#include <QSignalBlocker>
#include <QListWidget>
#include <QPushButton>
#include <QGroupBox>
#include <algorithm>
#include <cmath>

void MainWindow::updateLayerBlendModeUi() {
    if (!m_comboLayerBlendMode) return;
    const bool hasSelection = !m_selected.isEmpty();
    const int row = m_layerList ? m_layerList->currentRow() : -1;
    const bool valid = hasSelection && row >= 0 && row < m_selected.first()->badge().layers.size();
    const QSignalBlocker blocker(m_comboLayerBlendMode);
    m_comboLayerBlendMode->setEnabled(valid);
    if (!valid) {
        m_comboLayerBlendMode->setCurrentIndex(0);
        updateLayerPreviewUi();
        updateLayerFillUi();
        return;
    }
    m_comboLayerBlendMode->setCurrentIndex(layerBlendModeToInt(m_selected.first()->badge().layers[row].blendMode));
    updateLayerPreviewUi();
    updateLayerFillUi();
}

void MainWindow::updateLayerOpacityUi() {
    if (!m_sliderLayerOpacity) return;
    const bool hasSelection = !m_selected.isEmpty();
    const int row = m_layerList ? m_layerList->currentRow() : -1;
    const bool valid = hasSelection && row >= 0 && row < m_selected.first()->badge().layers.size();
    const QSignalBlocker blocker(m_sliderLayerOpacity);
    m_sliderLayerOpacity->setEnabled(valid);
    if (!valid) {
        m_sliderLayerOpacity->setValue(100);
        updateLayerPreviewUi();
        updateLayerFillUi();
        return;
    }
    const auto& layer = m_selected.first()->badge().layers[row];
    m_sliderLayerOpacity->setValue(int(std::round(std::clamp(layer.opacity, 0.0, 1.0) * 100.0)));
    updateLayerPreviewUi();
    updateLayerFillUi();
}

void MainWindow::updateLayerFillUi() {
    QColor color;
    const int row = m_layerList ? m_layerList->currentRow() : -1;
    const bool hasTarget = !m_selected.isEmpty() && row >= 0;
    if (hasTarget) {
        const auto& layers = m_selected.first()->badge().layers;
        if (row >= 0 && row < layers.size()) color = layers[row].fillColor;
    }
    if (m_propPickedColor) {
        if (color.isValid()) {
            const QString hex = color.alpha() == 255 ? color.name().toUpper() : color.name(QColor::HexArgb).toUpper();
            m_propPickedColor->setText(hex);
        } else {
            m_propPickedColor->clear();
            m_propPickedColor->setPlaceholderText(QStringLiteral("未設定"));
        }
    }
    if (m_pickedColorSwatch) {
        QPalette pal = m_pickedColorSwatch->palette();
        pal.setColor(QPalette::Window, color.isValid() ? color : QColor(96, 96, 96));
        pal.setColor(QPalette::WindowText, Qt::black);
        m_pickedColorSwatch->setPalette(pal);
    }
    if (m_btnEyedropper) {
        m_btnEyedropper->setEnabled(hasTarget);
        if (!hasTarget && m_btnEyedropper->isChecked()) m_btnEyedropper->setChecked(false);
    }
}

void MainWindow::updateInspectorMode() {
    const bool designer = m_isDesigner;
    if (m_propGroup) m_propGroup->setVisible(designer);
    if (m_colorGroup) m_colorGroup->setVisible(designer);
    if (m_layerGroup) m_layerGroup->setVisible(designer);
    if (m_guideGroup) m_guideGroup->setVisible(designer);
    if (m_effectGroup) m_effectGroup->setVisible(designer);
    if (m_layoutGroup) m_layoutGroup->setVisible(!designer);
    if (!designer && m_designer) m_designer->setEyedropperActive(false);
}

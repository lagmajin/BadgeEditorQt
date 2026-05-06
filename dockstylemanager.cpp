#include "dockstylemanager.h"

#include <QAbstractButton>
#include <QApplication>
#include <QEvent>
#include <QColor>
#include <QLabel>
#include <QFont>
#include <QProxyStyle>
#include <QTimer>
#include <QWidget>
#include <wobjectimpl.h>

#include <DockManager.h>
#include <DockWidget.h>
#include <DockWidgetTab.h>

W_OBJECT_IMPL(DockStyleManager)

namespace {

class DockProxyStyle final : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;

    int pixelMetric(PixelMetric metric, const QStyleOption* option = nullptr,
                    const QWidget* widget = nullptr) const override {
        switch (metric) {
        case PM_TabBarTabHSpace:
            return QProxyStyle::pixelMetric(metric, option, widget) + 10;
        case PM_TabBarTabVSpace:
            return QProxyStyle::pixelMetric(metric, option, widget) + 4;
        case PM_TabBarBaseHeight:
            return QProxyStyle::pixelMetric(metric, option, widget) + 1;
        case PM_SmallIconSize:
            return 18;
        default:
            break;
        }
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
};

QColor blend(const QColor& a, const QColor& b, qreal ratio) {
    const qreal clamped = qBound<qreal>(0.0, ratio, 1.0);
    return QColor::fromRgbF(
        a.redF()   * (1.0 - clamped) + b.redF()   * clamped,
        a.greenF() * (1.0 - clamped) + b.greenF() * clamped,
        a.blueF()  * (1.0 - clamped) + b.blueF()  * clamped,
        a.alphaF() * (1.0 - clamped) + b.alphaF() * clamped
    );
}

void repolish(QWidget* widget) {
    if (!widget) {
        return;
    }
    widget->setAttribute(Qt::WA_StyledBackground, true);
    if (auto* style = widget->style()) {
        style->unpolish(widget);
        style->polish(widget);
    }
    widget->update();
}

} // namespace

bool DockStyleManager::isDockRelatedObject(QObject* watched, ads::CDockManager* dockManager) {
    if (!watched || !dockManager) {
        return false;
    }
    if (watched == dockManager) {
        return true;
    }
    if (qobject_cast<ads::CDockWidget*>(watched) || qobject_cast<ads::CDockWidgetTab*>(watched)) {
        return true;
    }

    auto* widget = qobject_cast<QWidget*>(watched);
    if (!widget) {
        return false;
    }
    if (dockManager->isAncestorOf(widget)) {
        return true;
    }
    for (auto* w = widget->parentWidget(); w; w = w->parentWidget()) {
        if (qobject_cast<QWidget*>(w) && w->windowType() == Qt::Tool) {
            return true;
        }
    }
    return false;
}

ads::CDockWidget* DockStyleManager::dockFromObject(QObject* object) {
    QObject* cursor = object;
    while (cursor) {
        if (auto* tab = qobject_cast<ads::CDockWidgetTab*>(cursor)) {
            return tab->dockWidget();
        }
        if (auto* dock = qobject_cast<ads::CDockWidget*>(cursor)) {
            return dock;
        }
        cursor = cursor->parent();
    }
    return nullptr;
}

DockStyleManager::DockStyleManager(ads::CDockManager* dockManager, QObject* parent)
    : QObject(parent), m_dockManager(dockManager) {
    if (!m_dockManager) {
        return;
    }

    m_proxyStyle = new DockProxyStyle(QApplication::style());
    if (qApp) {
        m_proxyStyle->setParent(qApp);
        qApp->installEventFilter(this);
    }
    m_dockManager->setStyle(m_proxyStyle);
    scheduleRefresh();
}

DockStyleManager::~DockStyleManager() {
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

void DockStyleManager::applyTheme(const QPalette& palette) {
    m_palette = palette;
    scheduleRefresh();
}

bool DockStyleManager::eventFilter(QObject* watched, QEvent* event) {
    if (!m_dockManager || !event) {
        return QObject::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::FocusIn:
    case QEvent::FocusOut:
    case QEvent::Hide:
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::Show:
    case QEvent::WindowActivate:
    case QEvent::WindowDeactivate:
    case QEvent::ZOrderChange:
        break;
    default:
        return QObject::eventFilter(watched, event);
    }

    if (isDockRelatedObject(watched, m_dockManager)) {
        if ((event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease) &&
            watched) {
            if (auto* dock = dockFromObject(watched)) {
                dock->setProperty("badgeDockFocused", true);
            }
        }
        scheduleRefresh();
    }

    return QObject::eventFilter(watched, event);
}

void DockStyleManager::repolishWidget(QWidget* widget) {
    repolish(widget);
}

void DockStyleManager::applyWidgetPalette(QWidget* widget, const QPalette& palette) {
    if (!widget) {
        return;
    }
    widget->setPalette(palette);
    widget->setAutoFillBackground(true);
    widget->setAttribute(Qt::WA_StyledBackground, true);
    repolish(widget);
}

void DockStyleManager::scheduleRefresh() {
    if (!m_dockManager || m_refreshScheduled) {
        return;
    }
    m_refreshScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        m_refreshScheduled = false;
        refreshDockDecorations();
    });
}

void DockStyleManager::refreshDockDecorations() {
    if (!m_dockManager) {
        return;
    }

    const auto docks = m_dockManager->dockWidgetsMap().values();
    for (auto* dock : docks) {
        if (!dock) {
            continue;
        }

        const QColor windowColor = m_palette.color(QPalette::Window);
        const QColor textColor = m_palette.color(QPalette::WindowText);
        const QColor borderColor = m_palette.color(QPalette::Mid);
        const QColor baseColor = m_palette.color(QPalette::Base);
        const QColor activeTabBg = blend(m_palette.color(QPalette::Button), m_palette.color(QPalette::Highlight), 0.12);
        const QColor inactiveTabBg = blend(windowColor, baseColor, 0.72);
        const QColor inactiveTextColor = textColor.lighter(108);
        const bool isActiveTab = dock->tabWidget() && dock->tabWidget()->isActiveTab();
        const QColor tabBg = isActiveTab ? activeTabBg : inactiveTabBg;

        auto dockPalette = dock->palette();
        dockPalette.setColor(QPalette::Window, windowColor);
        dockPalette.setColor(QPalette::WindowText, textColor);
        dockPalette.setColor(QPalette::Base, baseColor);
        dockPalette.setColor(QPalette::Button, tabBg);
        dockPalette.setColor(QPalette::ButtonText, textColor);
        dockPalette.setColor(QPalette::Mid, borderColor.lighter(125));
        dockPalette.setColor(QPalette::Light, tabBg.lighter(120));
        dockPalette.setColor(QPalette::Dark, tabBg.darker(145));
        dock->setPalette(dockPalette);
        dock->setAttribute(Qt::WA_StyledBackground, true);
        repolishWidget(dock);

        if (auto* tab = dock->tabWidget()) {
            auto tabPalette = tab->palette();
            tabPalette.setColor(QPalette::Window, tabBg);
            tabPalette.setColor(QPalette::Base, tabBg);
            tabPalette.setColor(QPalette::Button, tabBg);
            tabPalette.setColor(QPalette::WindowText, isActiveTab ? textColor : inactiveTextColor);
            tabPalette.setColor(QPalette::Text, isActiveTab ? textColor : inactiveTextColor);
            tabPalette.setColor(QPalette::ButtonText, textColor);
            tabPalette.setColor(QPalette::Mid, borderColor.lighter(125));
            tabPalette.setColor(QPalette::Light, tabBg.lighter(120));
            tabPalette.setColor(QPalette::Dark, tabBg.darker(145));
            tabPalette.setColor(QPalette::Highlight, m_palette.color(QPalette::Highlight));
            tabPalette.setColor(QPalette::HighlightedText, m_palette.color(QPalette::HighlightedText));
            tab->setPalette(tabPalette);
            tab->setAttribute(Qt::WA_StyledBackground, true);
            repolishWidget(tab);

            for (auto* label : tab->findChildren<QLabel*>()) {
                if (!label) {
                    continue;
                }
                auto pal = label->palette();
                pal.setColor(QPalette::WindowText, isActiveTab ? textColor : inactiveTextColor);
                pal.setColor(QPalette::Text, isActiveTab ? textColor : inactiveTextColor);
                pal.setColor(QPalette::Base, tabBg);
                pal.setColor(QPalette::Button, tabBg);
                label->setPalette(pal);
                auto font = label->font();
                font.setBold(isActiveTab);
                label->setFont(font);
                repolishWidget(label);
            }

            for (auto* button : tab->findChildren<QAbstractButton*>()) {
                if (!button) {
                    continue;
                }
                auto pal = button->palette();
                pal.setColor(QPalette::Window, tabBg);
                pal.setColor(QPalette::Base, tabBg);
                pal.setColor(QPalette::Button, tabBg);
                pal.setColor(QPalette::WindowText, isActiveTab ? textColor : inactiveTextColor);
                pal.setColor(QPalette::Text, isActiveTab ? textColor : inactiveTextColor);
                pal.setColor(QPalette::ButtonText, textColor);
                button->setPalette(pal);
                button->setAutoFillBackground(true);
                auto font = button->font();
                font.setBold(isActiveTab);
                button->setFont(font);
                repolishWidget(button);
            }
        }
    }

    m_dockManager->update();
}

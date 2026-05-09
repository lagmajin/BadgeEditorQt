#ifndef MIXEDLAYOUTDIALOG_H
#define MIXEDLAYOUTDIALOG_H

#include <QDialog>
#include <QShowEvent>
#include <QList>

#include "badgeitem.h"

class QCheckBox;
class QLabel;
class QTableWidget;
class QWidget;

class MixedLayoutDialog : public QDialog {
public:
    explicit MixedLayoutDialog(const QList<BadgeItem>& templates, QWidget* parent = nullptr);

    QList<BadgeItem> expandedTemplates() const;
    bool sortBySize() const;

protected:
    void showEvent(QShowEvent* event) override;

private:
    void applyBackdrop();
    void refreshSummary();
    static QString templateTitle(const BadgeItem& badge);
    static QString templateSize(const BadgeItem& badge);
    static QString templateKind(const BadgeItem& badge);

    QList<BadgeItem> m_templates;
    QTableWidget* m_table = nullptr;
    QCheckBox* m_sortBySize = nullptr;
    QLabel* m_summary = nullptr;
    bool m_backdropApplied = false;
};

#endif

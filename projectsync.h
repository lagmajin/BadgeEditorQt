#ifndef PROJECTSYNC_H
#define PROJECTSYNC_H

#include <QString>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>

class DesignerWidget;
class LayoutWorkspaceWidget;

namespace badge {
struct DocumentData;
}

namespace projectsync {

badge::DocumentData currentDocument(const DesignerWidget& designer,
                                   const QComboBox& paperSize,
                                   const QCheckBox& landscape,
                                   const QDoubleSpinBox& marginMm,
                                   const QDoubleSpinBox& spacingMm,
                                   const QString& title);
void applyDocument(DesignerWidget& designer,
                   LayoutWorkspaceWidget& layoutWorkspace,
                   QComboBox& paperSize,
                   QCheckBox& landscape,
                   QDoubleSpinBox& marginMm,
                   QDoubleSpinBox& spacingMm,
                   const badge::DocumentData& document);

}

#endif

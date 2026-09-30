#include "mainwindow.h"

void MainWindow::onUndo() {
    if (m_undoStack) {
        const QString label = m_undoStack->undoText();
        m_undoStack->undo();
        appendLog(label.isEmpty() ? QStringLiteral("元に戻しました")
                                  : QStringLiteral("元に戻しました: %1").arg(label));
    }
}

void MainWindow::onRedo() {
    if (m_undoStack) {
        const QString label = m_undoStack->redoText();
        m_undoStack->redo();
        appendLog(label.isEmpty() ? QStringLiteral("やり直しました")
                                  : QStringLiteral("やり直しました: %1").arg(label));
    }
}

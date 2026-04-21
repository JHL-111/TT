#include "cad_ui/StatusBar.h"
#include <QString>

namespace cad_ui {

    StatusBar::StatusBar(QWidget* parent) : QStatusBar(parent), m_mousePositionLabel(nullptr) {
        setObjectName("StatusBar");
        setupMousePositionDisplay();
    }

    void StatusBar::setupMousePositionDisplay() {
        // Create the mouse-position label

        m_mousePositionLabel = new QLabel("MousePosition: (0, 0)");
        m_mousePositionLabel->setObjectName("MousePositionLabel");
        m_mousePositionLabel->setMinimumWidth(200);
        m_mousePositionLabel->setStyleSheet("QLabel { padding: 2px 8px; border: 1px solid #ccc; border-radius: 3px; background: #f8f8f8; }");

        // Add the label to the right side of the status bar as a permanent widget

        addPermanentWidget(m_mousePositionLabel);

        // Initial display state

        updateMousePosition2D(0, 0);
    }

    void StatusBar::updateMousePosition(double x, double y, double z) {
        if (m_mousePositionLabel) {
            QString posText = QString("3DPosition: (%1, %2, %3)")
                .arg(x, 0, 'f', 2)
                .arg(y, 0, 'f', 2)
                .arg(z, 0, 'f', 2);
            m_mousePositionLabel->setText(posText);
        }
    }

    void StatusBar::updateMousePosition2D(int screenX, int screenY) {
        if (m_mousePositionLabel) {
            QString posText = QString("MousePosition: (%1, %2)")
                .arg(screenX)
                .arg(screenY);
            m_mousePositionLabel->setText(posText);
        }
    }

} // namespace cad_ui

#include "StatusBar.moc"
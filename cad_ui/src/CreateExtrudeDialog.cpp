#include "cad_ui/CreateExtrudeDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMessageBox>

namespace cad_ui {

    CreateExtrudeDialog::CreateExtrudeDialog(QWidget* parent) : QDialog(parent), m_selectedShape(nullptr) {
        setWindowTitle("Extrude");
        setMinimumWidth(280);

        // Set as a non-modal floating panel (Non-modal floating window)

        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_DeleteOnClose); // Delete automatically when closed


        QVBoxLayout* mainLayout = new QVBoxLayout(this);

        // Status label

        m_statusLabel = new QLabel("Please select a profile or face...", this);
        m_statusLabel->setStyleSheet("color: #0055ff; font-weight: bold;");
        mainLayout->addWidget(m_statusLabel);

        // Distance input area

        QHBoxLayout* distLayout = new QHBoxLayout();
        QLabel* distLabel = new QLabel("Distance (Height):");
        m_distanceSpinBox = new QDoubleSpinBox(this);
        m_distanceSpinBox->setRange(0.1, 10000.0);
        m_distanceSpinBox->setValue(10.0);
        m_distanceSpinBox->setDecimals(2);
        m_distanceSpinBox->setSuffix(" mm");

        distLayout->addWidget(distLabel);
        distLayout->addWidget(m_distanceSpinBox);
        mainLayout->addLayout(distLayout);

        // Button area

        QHBoxLayout* btnLayout = new QHBoxLayout();
        QPushButton* okBtn = new QPushButton("OK", this);
        QPushButton* cancelBtn = new QPushButton("Cancel", this);
        btnLayout->addWidget(okBtn);
        btnLayout->addWidget(cancelBtn);
        mainLayout->addLayout(btnLayout);

        connect(okBtn, &QPushButton::clicked, this, &CreateExtrudeDialog::OnOkClicked);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        // Trigger preview on value change
        connect(m_distanceSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this]() {
                if (m_selectedShape) {
                    emit previewRequested(m_selectedShape, m_distanceSpinBox->value());
                }
            });
    }

    void CreateExtrudeDialog::SetSelectedShape(const cad_core::ShapePtr& shape) {
        m_selectedShape = shape;
        if (shape) {
            m_statusLabel->setText("Selection received. Click OK to extrude.");
            m_statusLabel->setStyleSheet("color: #00aa00; font-weight: bold;"); // Turn green when a valid selection is made

        }
        else {
            m_statusLabel->setText("Please select a profile or face...");
            m_statusLabel->setStyleSheet("color: #0055ff; font-weight: bold;");
        }
        emit previewRequested(m_selectedShape, m_distanceSpinBox->value());
    }

    double CreateExtrudeDialog::GetDistance() const {
        return m_distanceSpinBox->value();
    }

    void CreateExtrudeDialog::OnOkClicked() {
        if (!m_selectedShape) {
            QMessageBox::warning(this, "Warning", "Please select a face or profile first!");
            return;
        }
        // Emit the execution signal

        emit extrudeRequested(m_selectedShape, GetDistance());
        accept(); // Close the dialog

    }

    void CreateExtrudeDialog::closeEvent(QCloseEvent* event) {
        emit dialogClosed();
        QDialog::closeEvent(event);
    }

    

} // namespace cad_ui
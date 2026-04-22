#include "cad_ui/CreateRevolveDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QMessageBox>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace cad_ui {

    CreateRevolveDialog::CreateRevolveDialog(QWidget* parent)
        : QDialog(parent), m_selectedShape(nullptr)
    {
        setWindowTitle("Revolve");
        setMinimumWidth(300);
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_DeleteOnClose);

        QVBoxLayout* mainLayout = new QVBoxLayout(this);

        // Status Notification
        m_statusLabel = new QLabel("Please select a profile or face...", this);
        m_statusLabel->setStyleSheet("color: #0055ff; font-weight: bold;");
        mainLayout->addWidget(m_statusLabel);

		// Angle
        QHBoxLayout* angleLayout = new QHBoxLayout();
        angleLayout->addWidget(new QLabel("Angle (degrees):"));
        m_angleSpinBox = new QDoubleSpinBox(this);
        m_angleSpinBox->setRange(0.1, 360.0);
        m_angleSpinBox->setValue(360.0);
        m_angleSpinBox->setDecimals(1);
        m_angleSpinBox->setSuffix(" бу");
        angleLayout->addWidget(m_angleSpinBox);
        mainLayout->addLayout(angleLayout);

        // Axis Origin
        QGroupBox* originGroup = new QGroupBox("Axis Origin", this);
        QHBoxLayout* originLayout = new QHBoxLayout(originGroup);

        auto makeCoordSpin = [this](double val) {
            QDoubleSpinBox* spin = new QDoubleSpinBox(this);
            spin->setRange(-10000.0, 10000.0);
            spin->setValue(val);
            spin->setDecimals(2);
            return spin;
            };

        originLayout->addWidget(new QLabel("X:"));
        m_axOriginX = makeCoordSpin(0.0);
        originLayout->addWidget(m_axOriginX);
        originLayout->addWidget(new QLabel("Y:"));
        m_axOriginY = makeCoordSpin(0.0);
        originLayout->addWidget(m_axOriginY);
        originLayout->addWidget(new QLabel("Z:"));
        m_axOriginZ = makeCoordSpin(0.0);
        originLayout->addWidget(m_axOriginZ);
        mainLayout->addWidget(originGroup);

        // Axis Direction
        QGroupBox* dirGroup = new QGroupBox("Axis Direction", this);
        QHBoxLayout* dirLayout = new QHBoxLayout(dirGroup);

        dirLayout->addWidget(new QLabel("X:"));
        m_axDirX = makeCoordSpin(0.0);
        dirLayout->addWidget(m_axDirX);
        dirLayout->addWidget(new QLabel("Y:"));
        m_axDirY = makeCoordSpin(0.0);
        dirLayout->addWidget(m_axDirY);
        dirLayout->addWidget(new QLabel("Z:"));
        m_axDirZ = makeCoordSpin(1.0);  
        dirLayout->addWidget(m_axDirZ);
        mainLayout->addWidget(dirGroup);

        // Buttons
        QHBoxLayout* btnLayout = new QHBoxLayout();
        QPushButton* okBtn = new QPushButton("OK", this);
        QPushButton* cancelBtn = new QPushButton("Cancel", this);
        btnLayout->addWidget(okBtn);
        btnLayout->addWidget(cancelBtn);
        mainLayout->addLayout(btnLayout);

        connect(okBtn, &QPushButton::clicked, this, &CreateRevolveDialog::OnOkClicked);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    }

    void CreateRevolveDialog::SetSelectedShape(const cad_core::ShapePtr& shape) {
        m_selectedShape = shape;
        if (shape) {
            m_statusLabel->setText("Selection received. Click OK to revolve.");
            m_statusLabel->setStyleSheet("color: #00aa00; font-weight: bold;");
        }
        else {
            m_statusLabel->setText("Please select a profile or face...");
            m_statusLabel->setStyleSheet("color: #0055ff; font-weight: bold;");
        }
    }

    void CreateRevolveDialog::OnOkClicked() {
        if (!m_selectedShape) {
            QMessageBox::warning(this, "Warning", "Please select a face or profile first!");
            return;
        }

        // Check if the axis direction is not a zero vector
        double dx = m_axDirX->value(), dy = m_axDirY->value(), dz = m_axDirZ->value();
        if (std::abs(dx) < 1e-10 && std::abs(dy) < 1e-10 && std::abs(dz) < 1e-10) {
            QMessageBox::warning(this, "Warning", "Axis direction cannot be zero!");
            return;
        }

        // Convert degrees to radians
        double angleRad = m_angleSpinBox->value() * M_PI / 180.0;

        emit revolveRequested(m_selectedShape, angleRad,
            m_axOriginX->value(), m_axOriginY->value(), m_axOriginZ->value(),
            dx, dy, dz);
        accept();
    }

    void CreateRevolveDialog::closeEvent(QCloseEvent* event) {
        emit dialogClosed();
        QDialog::closeEvent(event);
    }

} // namespace cad_ui
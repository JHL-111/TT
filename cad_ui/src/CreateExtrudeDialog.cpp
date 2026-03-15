#include "cad_ui/CreateExtrudeDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMessageBox>

namespace cad_ui {

    CreateExtrudeDialog::CreateExtrudeDialog(QWidget* parent) : QDialog(parent), m_selectedShape(nullptr) {
        setWindowTitle("Extrude");
        setMinimumWidth(280);

        // 核心：设为非模态悬浮窗 (Non-modal floating window)
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_DeleteOnClose); // 关闭时自动销毁

        QVBoxLayout* mainLayout = new QVBoxLayout(this);

        // 状态提示文字
        m_statusLabel = new QLabel("Please select a profile or face...", this);
        m_statusLabel->setStyleSheet("color: #0055ff; font-weight: bold;");
        mainLayout->addWidget(m_statusLabel);

        // 距离输入区
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

        // 按钮区
        QHBoxLayout* btnLayout = new QHBoxLayout();
        QPushButton* okBtn = new QPushButton("OK", this);
        QPushButton* cancelBtn = new QPushButton("Cancel", this);
        btnLayout->addWidget(okBtn);
        btnLayout->addWidget(cancelBtn);
        mainLayout->addLayout(btnLayout);

        connect(okBtn, &QPushButton::clicked, this, &CreateExtrudeDialog::OnOkClicked);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    }

    void CreateExtrudeDialog::SetSelectedShape(const cad_core::ShapePtr& shape) {
        m_selectedShape = shape;
        if (shape) {
            m_statusLabel->setText("Profile selected! Click OK to extrude.");
            m_statusLabel->setStyleSheet("color: #00aa00; font-weight: bold;"); // 选中有反馈变绿
        }
        else {
            m_statusLabel->setText("Please select a profile or face...");
            m_statusLabel->setStyleSheet("color: #0055ff; font-weight: bold;");
        }
    }

    double CreateExtrudeDialog::GetDistance() const {
        return m_distanceSpinBox->value();
    }

    void CreateExtrudeDialog::OnOkClicked() {
        if (!m_selectedShape) {
            QMessageBox::warning(this, "Warning", "Please select a face or profile first!");
            return;
        }
        // 触发执行信号
        emit extrudeRequested(m_selectedShape, GetDistance());
        accept(); // 关闭对话框
    }

    void CreateExtrudeDialog::closeEvent(QCloseEvent* event) {
        emit dialogClosed();
        QDialog::closeEvent(event);
    }

} // namespace cad_ui
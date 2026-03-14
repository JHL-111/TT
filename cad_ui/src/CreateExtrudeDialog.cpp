#include "cad_ui/CreateExtrudeDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>

namespace cad_ui {

    CreateExtrudeDialog::CreateExtrudeDialog(QWidget* parent) : QDialog(parent) {
        setWindowTitle("Extrude Sketch");
        setMinimumWidth(250);

        QVBoxLayout* mainLayout = new QVBoxLayout(this);

        // 距离输入区 (Distance Input)
        QHBoxLayout* distLayout = new QHBoxLayout();
        QLabel* distLabel = new QLabel("Distance (Height):");
        m_distanceSpinBox = new QDoubleSpinBox(this);
        m_distanceSpinBox->setRange(0.1, 10000.0); // 最小 0.1，最大 10000
        m_distanceSpinBox->setValue(10.0);         // 默认拉伸高度 10.0
        m_distanceSpinBox->setDecimals(2);         // 保留两位小数
        m_distanceSpinBox->setSuffix(" mm");

        distLayout->addWidget(distLabel);
        distLayout->addWidget(m_distanceSpinBox);
        mainLayout->addLayout(distLayout);

        // 确认/取消按钮 (Dialog Buttons)
        QDialogButtonBox* buttonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            Qt::Horizontal, this);

        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

        mainLayout->addWidget(buttonBox);
    }

    double CreateExtrudeDialog::GetDistance() const {
        return m_distanceSpinBox->value();
    }

} // namespace cad_ui
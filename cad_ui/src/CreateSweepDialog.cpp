#include "cad_ui/CreateSweepDialog.h"
#include "cad_ui/QtOccView.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>

namespace cad_ui {

    CreateSweepDialog::CreateSweepDialog(QtOccView* view, QWidget* parent)
        : QDialog(parent), m_view(view) {

        setWindowTitle(tr("Sweep Feature"));
        setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint);
        setMinimumWidth(300);

        SetupUI();
    }

    void CreateSweepDialog::SetupUI() {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);

        // 1. 顶部操作提示 (Instruction)
        m_instructionLabel = new QLabel(tr("Operation steps:\n"
            "1. Click on the screen to select a profile.\n"
            "2. Rotate and confirm the direction of the path plane.\n"
            "3. Draw the Sweep path.\n"
            "4. Click the 'Apply' button below."));
        m_instructionLabel->setStyleSheet("color: #666; font-style: italic;");
        mainLayout->addWidget(m_instructionLabel);

        m_btnCreatePath = new QPushButton(tr("1. Draw Path"), this);
        m_btnCreatePath->setCheckable(true); // 让它可以像开关一样被按下和弹起
        // 用 CSS 给按下的状态加个醒目的绿色背景
        m_btnCreatePath->setStyleSheet("QPushButton:checked { background-color: #d4edda; border: 2px solid #28a745; font-weight: bold; color: #155724; }");
        mainLayout->addWidget(m_btnCreatePath);

        connect(m_btnCreatePath, &QPushButton::toggled, this, &CreateSweepDialog::OnCreatePathToggled);

        // 2. 参数表单 (Parameter Form)
        QFormLayout* formLayout = new QFormLayout();

        m_twistSpinner = new QDoubleSpinBox(this);
        m_twistSpinner->setRange(-3600.0, 3600.0);
        m_twistSpinner->setValue(0.0);
        formLayout->addRow(tr("Twist Angle:"), m_twistSpinner);

        m_scaleSpinner = new QDoubleSpinBox(this);
        m_scaleSpinner->setRange(0.01, 100.0);
        m_scaleSpinner->setValue(1.0);
        m_scaleSpinner->setSingleStep(0.1);
        formLayout->addRow(tr("Scale Factor:"), m_scaleSpinner);

        m_keepOrientationCheck = new QCheckBox(tr("Keep Orientation"), this);
        m_keepOrientationCheck->setChecked(true);
        formLayout->addRow("", m_keepOrientationCheck);

        mainLayout->addLayout(formLayout);

        // 3. 底部按钮 (Buttons)
        QHBoxLayout* btnLayout = new QHBoxLayout();
        m_btnApply = new QPushButton(tr("Apply"), this);
        m_btnApply->setDefault(true);
        m_btnCancel = new QPushButton(tr("Cancel"), this);

        btnLayout->addStretch();
        btnLayout->addWidget(m_btnCancel);
        btnLayout->addWidget(m_btnApply);
        mainLayout->addLayout(btnLayout);

        // 绑定信号槽
        connect(m_btnApply, &QPushButton::clicked, this, &CreateSweepDialog::OnApplyClicked);
        connect(m_btnCancel, &QPushButton::clicked, this, &CreateSweepDialog::OnCancelClicked);
    }

    void CreateSweepDialog::OnCreatePathToggled(bool checked) {
        if (checked) {
            m_btnCreatePath->setText(tr("2. Drawing..."));
        }
        else {
            m_btnCreatePath->setText(tr("1. Draw Path"));
        }

        // 通知底层视图层切换工具
        if (m_view) {
            m_view->ToggleSweepPathTool(checked);
        }
    }

    void CreateSweepDialog::OnApplyClicked() {
        if (!m_view) return;

        // 从视图层获取选中的截面和组装好的路径
        auto profile = m_view->GetSweepProfileShape();
        auto path = m_view->GetSweepPathShape();

        if (!profile || !path) {
            QMessageBox::warning(this, tr("Sweep Failed"), tr("Please ensure you have selected a profile and drawn a valid path."));
            return;
        }

        double twist = m_twistSpinner->value();
        double scale = m_scaleSpinner->value();
        bool keepOri = m_keepOrientationCheck->isChecked();

        // 向主窗口发送生成请求
        emit sweepRequested(profile, path, twist, scale, keepOri);
        accept(); // 关闭面板
    }

    void CreateSweepDialog::OnCancelClicked() {
        if (m_view) {
            m_view->CancelSweepInteraction();
        }
        reject();
    }

} // namespace cad_ui
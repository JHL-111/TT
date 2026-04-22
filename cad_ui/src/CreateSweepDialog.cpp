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

        // 1. Top instruction area

        m_instructionLabel = new QLabel(tr("Operation steps:\n"
            "1. Click on the screen to select a profile.\n"
            "2. Rotate and confirm the direction of the path plane.\n"
            "3. Draw the Sweep path.\n"
            "4. Click the 'Apply' button below."));
        m_instructionLabel->setStyleSheet("color: #666; font-style: italic;");
        mainLayout->addWidget(m_instructionLabel);

        m_btnCreatePath = new QPushButton(tr("1. Draw Path"), this);
        m_btnCreatePath->setCheckable(true);

        // Use CSS to add a green background to the pressed state

        m_btnCreatePath->setStyleSheet("QPushButton:checked { background-color: #d4edda; border: 2px solid #28a745; font-weight: bold; color: #155724; }");
        mainLayout->addWidget(m_btnCreatePath);

        connect(m_btnCreatePath, &QPushButton::toggled, this, &CreateSweepDialog::OnCreatePathToggled);

        // 2. Parameter form

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

        // 3. Bottom buttons

        QHBoxLayout* btnLayout = new QHBoxLayout();
        m_btnApply = new QPushButton(tr("Apply"), this);
        m_btnApply->setDefault(true);
        m_btnCancel = new QPushButton(tr("Cancel"), this);

        btnLayout->addStretch();
        btnLayout->addWidget(m_btnCancel);
        btnLayout->addWidget(m_btnApply);
        mainLayout->addLayout(btnLayout);

        // Connect signals and slots

        connect(m_btnApply, &QPushButton::clicked, this, &CreateSweepDialog::OnApplyClicked);
        connect(m_btnCancel, &QPushButton::clicked, this, &CreateSweepDialog::OnCancelClicked);

		// 1. bind UI controls to trigger preview updates when parameters change
        connect(m_twistSpinner, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CreateSweepDialog::TryPreview);
        connect(m_scaleSpinner, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &CreateSweepDialog::TryPreview);
        connect(m_keepOrientationCheck, &QCheckBox::toggled, this, &CreateSweepDialog::TryPreview);

		// 2. make the viewer layer notify us when the sketch history changes
        connect(m_view, &QtOccView::SketchHistoryChanged, this, &CreateSweepDialog::TryPreview);
        connect(m_view, &QtOccView::SketchToolChanged, this, [this](const QString& toolName) {
            if (toolName == "None") {
                TryPreview();

                if (m_btnCreatePath->isChecked()) {
                    m_btnCreatePath->blockSignals(true);
                    m_btnCreatePath->setChecked(false);
                    m_btnCreatePath->setText(tr("1. Draw Path"));
                    m_btnCreatePath->blockSignals(false);
                }
            }
        });
    }

    void CreateSweepDialog::OnCreatePathToggled(bool checked) {
        if (checked) {
            m_btnCreatePath->setText(tr("2. Drawing..."));
        }
        else {
            m_btnCreatePath->setText(tr("1. Draw Path"));
            TryPreview(); 
        }

        if (m_view) {
            m_view->ToggleSweepPathTool(checked);
        }
    }

    void CreateSweepDialog::OnApplyClicked() {
        if (!m_view) return;

        // Get the selected profile and assembled path from the viewer layer

        auto profile = m_view->GetSweepProfileShape();
        auto path = m_view->GetSweepPathShape();

        if (!profile || !path) {
            QMessageBox::warning(this, tr("Sweep Failed"), tr("Please ensure you have selected a profile and drawn a valid path."));
            return;
        }

        double twist = m_twistSpinner->value();
        double scale = m_scaleSpinner->value();
        bool keepOri = m_keepOrientationCheck->isChecked();

        // Send the generation request to the main window

        emit sweepRequested(profile, path, twist, scale, keepOri);
        accept(); // Close the panel

    }

    void CreateSweepDialog::reject() {
        // 1. Reset the path-drawing button if it is still pressed

        // setChecked(false) automatically triggers the toggled signal, allowing safe cleanup

        if (m_btnCreatePath->isChecked()) {
            m_btnCreatePath->setChecked(false);
        }

        // 2. Cancel sweep interaction in the viewer layer

        if (m_view) {
            m_view->CancelSweepInteraction();
        }

        // 3. Call the base reject() to close the dialog

        QDialog::reject();
    }


    void CreateSweepDialog::OnCancelClicked() {
        reject();
    }

    void CreateSweepDialog::TryPreview() {
        if (!m_view) return;
        auto profile = m_view->GetSweepProfileShape();
        auto path = m_view->GetSweepPathShape();

		// only trigger preview if both profile and path are available
        if (profile && path) {
            emit previewRequested(profile, path, m_twistSpinner->value(), m_scaleSpinner->value(), m_keepOrientationCheck->isChecked());
        }
    }

} // namespace cad_ui
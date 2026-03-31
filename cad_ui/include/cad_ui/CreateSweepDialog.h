#pragma once

#include <QDialog>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>

namespace cad_ui {

    class QtOccView; // 前向声明

    class CreateSweepDialog : public QDialog {
        Q_OBJECT

    public:
        explicit CreateSweepDialog(QtOccView* view, QWidget* parent = nullptr);
        ~CreateSweepDialog() = default;

    private slots:
        void OnApplyClicked();
        void OnCancelClicked();
        void OnCreatePathToggled(bool checked);

    private:
        QtOccView* m_view;

        // UI 控件
        QLabel* m_instructionLabel;
        QDoubleSpinBox* m_twistSpinner;
        QDoubleSpinBox* m_scaleSpinner;
        QCheckBox* m_keepOrientationCheck;
        QPushButton* m_btnApply;
        QPushButton* m_btnCancel;      
        QPushButton* m_btnCreatePath;

        void SetupUI();
    };

} // namespace cad_ui

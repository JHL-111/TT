#pragma once

#include <QDialog>
#include <QDoubleSpinBox>
#include <memory>

namespace cad_ui {

    class CreateExtrudeDialog : public QDialog {
        Q_OBJECT

    public:
        explicit CreateExtrudeDialog(QWidget* parent = nullptr);
        ~CreateExtrudeDialog() override = default;

        /** 获取用户输入的拉伸距离 */
        double GetDistance() const;

    private:
        QDoubleSpinBox* m_distanceSpinBox;
    };

} // namespace cad_ui#pragma once

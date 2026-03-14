#pragma once
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include "cad_core/Shape.h"

namespace cad_ui {
    class CreateExtrudeDialog : public QDialog {
        Q_OBJECT
    public:
        explicit CreateExtrudeDialog(QWidget* parent = nullptr);
        double GetDistance() const;

        // 供外部将选中的形状传入对话框
        void SetSelectedShape(const cad_core::ShapePtr& shape);

    signals:
        // 点击 OK 时发射信号，交由 MainWindow 执行真正拉伸
        void extrudeRequested(cad_core::ShapePtr baseShape, double distance);
        void dialogClosed(); // 对话框关闭时通知外部清理临时面

    protected:
        void closeEvent(QCloseEvent* event) override;

    private slots:
        void OnOkClicked();

    private:
        QDoubleSpinBox* m_distanceSpinBox;
        QLabel* m_statusLabel;
        cad_core::ShapePtr m_selectedShape;
    };
} // namespace cad_ui
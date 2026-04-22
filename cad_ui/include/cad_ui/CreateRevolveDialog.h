#pragma once
#include <QDialog>
#include <QDoubleSpinBox>
#include <QLabel>
#include "cad_core/Shape.h"

namespace cad_ui {
    class CreateRevolveDialog : public QDialog {
        Q_OBJECT
    public:
        explicit CreateRevolveDialog(QWidget* parent = nullptr);

        void SetSelectedShape(const cad_core::ShapePtr& shape);

    signals:
        void revolveRequested(cad_core::ShapePtr baseShape, double angle,
            double axOriginX, double axOriginY, double axOriginZ,
            double axDirX, double axDirY, double axDirZ);
        void dialogClosed();

        // Revolve preview signal
        void previewRequested(cad_core::ShapePtr baseShape, double angle,
            double axOriginX, double axOriginY, double axOriginZ,
            double axDirX, double axDirY, double axDirZ);

    protected:
        void closeEvent(QCloseEvent* event) override;

    private slots:
        void OnOkClicked();

    private:
        QLabel* m_statusLabel;
        cad_core::ShapePtr m_selectedShape;

        QDoubleSpinBox* m_angleSpinBox;

        QDoubleSpinBox* m_axOriginX;
        QDoubleSpinBox* m_axOriginY;
        QDoubleSpinBox* m_axOriginZ;

        QDoubleSpinBox* m_axDirX;
        QDoubleSpinBox* m_axDirY;
        QDoubleSpinBox* m_axDirZ;
    };
} // namespace cad_ui#pragma once

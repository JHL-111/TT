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

        // Pass the externally selected shape to the dialog
        void SetSelectedShape(const cad_core::ShapePtr& shape);

    signals:
        // Emitted when OK is clicked; MainWindow executes the extrude operation
        void extrudeRequested(cad_core::ShapePtr baseShape, double distance);
        void dialogClosed(); // Notifies external code to clean up temporary state when the dialog closes

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
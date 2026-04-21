#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QFrame>
#include <QListWidget>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <vector>
#include "cad_core/Shape.h"
#include "cad_core/TransformCommand.h"

namespace cad_ui {

    /**
     * @class TransformOperationDialog
     * @brief Transform operation dialog supporting translation, rotation, and scaling
     */
    class TransformOperationDialog : public QDialog {
        Q_OBJECT

    public:
        explicit TransformOperationDialog(QWidget* parent = nullptr);
        ~TransformOperationDialog() = default;

        // Get the selected objects
        std::vector<cad_core::ShapePtr> getSelectedObjects() const { return m_selectedObjects; }

        // Get the current transform command (for execution)
        std::shared_ptr<cad_core::TransformCommand> getCurrentTransformCommand() const;

    public slots:
        void onObjectSelectionClicked();
        void onObjectSelected(const cad_core::ShapePtr& shape);
        void onSelectionFinished();
        void onPreviewClicked();
        void onResetClicked();
        void onTransformTypeChanged();

        // Slots for parameter changes
        void onTranslationParameterChanged();
        void onRotationParameterChanged();
        void onScaleParameterChanged();

    signals:
        void selectionModeChanged(bool enabled, const QString& prompt);
        void transformRequested(std::shared_ptr<cad_core::TransformCommand> command);
        void previewRequested(std::shared_ptr<cad_core::TransformCommand> command);
        void resetRequested();

    private slots:
        void accept() override;
        void reject() override;

    private:
        void setupUI();
        void setupTranslationTab();
        void setupRotationTab();
        void setupScaleTab();
        void updatePreview();
        void updateSelectionDisplay();
        void resetAllParameters();

        // UI components
        QVBoxLayout* m_mainLayout;

        // Object selection area
        QGroupBox* m_selectionGroup;
        QGridLayout* m_selectionLayout;
        QFrame* m_objectFrame;
        QHBoxLayout* m_objectFrameLayout;
        QLabel* m_objectCount;
        QPushButton* m_objectSelectButton;
        QListWidget* m_objectList;

        // Transform type selection
        QTabWidget* m_transformTabs;

        // Translation parameters
        QWidget* m_translateTab;
        QGroupBox* m_translationGroup;
        QGridLayout* m_translationLayout;
        QDoubleSpinBox* m_translateX;
        QDoubleSpinBox* m_translateY;
        QDoubleSpinBox* m_translateZ;

        // Rotation parameters
        QWidget* m_rotateTab;
        QGroupBox* m_rotationGroup;
        QGridLayout* m_rotationLayout;

        // Rotation axis point
        QGroupBox* m_axisPointGroup;
        QDoubleSpinBox* m_axisPointX;
        QDoubleSpinBox* m_axisPointY;
        QDoubleSpinBox* m_axisPointZ;

        // Rotation axis direction
        QGroupBox* m_axisDirectionGroup;
        QDoubleSpinBox* m_axisDirectionX;
        QDoubleSpinBox* m_axisDirectionY;
        QDoubleSpinBox* m_axisDirectionZ;
        QButtonGroup* m_axisPresetGroup;
        QRadioButton* m_axisXButton;
        QRadioButton* m_axisYButton;
        QRadioButton* m_axisZButton;
        QRadioButton* m_axisCustomButton;

        // Rotation angle
        QGroupBox* m_angleGroup;
        QDoubleSpinBox* m_rotationAngle;
        QRadioButton* m_angleRadians;
        QRadioButton* m_angleDegrees;

        // Scale parameters
        QWidget* m_scaleTab;
        QGroupBox* m_scaleGroup;
        QGridLayout* m_scaleLayout;

        // Scale centre
        QGroupBox* m_scaleCenterGroup;
        QDoubleSpinBox* m_scaleCenterX;
        QDoubleSpinBox* m_scaleCenterY;
        QDoubleSpinBox* m_scaleCenterZ;
        QPushButton* m_centerAtOriginButton;
        QPushButton* m_centerAtBoundingBoxButton;

        // Scale factors
        QGroupBox* m_scaleFactorGroup;
        QCheckBox* m_uniformScaleCheckBox;
        QDoubleSpinBox* m_scaleFactorUniform;
        QDoubleSpinBox* m_scaleFactorX;
        QDoubleSpinBox* m_scaleFactorY;
        QDoubleSpinBox* m_scaleFactorZ;

        // Control buttons
        QHBoxLayout* m_buttonLayout;
        QPushButton* m_previewButton;
        QPushButton* m_resetButton;
        QPushButton* m_okButton;
        QPushButton* m_cancelButton;

        // Data
        std::vector<cad_core::ShapePtr> m_selectedObjects;
        bool m_selectingObjects;
        bool m_previewActive;

        // Preset axis direction constants
        static constexpr double AXIS_X[] = { 1.0, 0.0, 0.0 };
        static constexpr double AXIS_Y[] = { 0.0, 1.0, 0.0 };
        static constexpr double AXIS_Z[] = { 0.0, 0.0, 1.0 };
    };

} // namespace cad_ui
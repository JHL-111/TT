#pragma once

#include "ICommand.h"
#include "Shape.h"
#include "Point.h"
#include <vector>
#include <gp_Trsf.hxx>
#include <gp_Vec.hxx>
#include <gp_Ax1.hxx>

namespace cad_core {

    /**
     * @enum TransformationType
     * @brief Types of transform operation
     */
    enum class TransformationType {
        Translate,  // Translation
        Rotate,     // Rotation
        Scale       // Scaling
    };

    /**
     * @class TransformCommand
     * @brief Base class for transform commands; implements geometric transform operations
     */
    class TransformCommand : public ICommand {
    public:
        TransformCommand(const std::vector<ShapePtr>& shapes, TransformationType type);
        virtual ~TransformCommand() = default;

        bool Execute() override;
        bool Undo() override;
        bool Redo() override;
        const char* GetName() const override;

        // Get transformed shapes (used for preview)
        virtual std::vector<ShapePtr> GetTransformedShapes() const;

        // Set transform parameters (implemented by derived classes)
        virtual void SetTransformParameters() = 0;

    protected:
        virtual gp_Trsf CreateTransformation() const = 0;
        virtual const char* GetTypeName() const = 0;

        std::vector<ShapePtr> m_originalShapes;
        std::vector<ShapePtr> m_transformedShapes;
        TransformationType m_type;
        bool m_executed;
    };

    /**
     * @class TranslateCommand
     * @brief Translation command
     */
    class TranslateCommand : public TransformCommand {
    public:
        TranslateCommand(const std::vector<ShapePtr>& shapes,
            const Point& translation);
        TranslateCommand(const std::vector<ShapePtr>& shapes,
            double dx, double dy, double dz);

        void SetTransformParameters() override;
        void SetTranslation(const Point& translation);
        void SetTranslation(double dx, double dy, double dz);

    protected:
        gp_Trsf CreateTransformation() const override;
        const char* GetTypeName() const override;

    private:
        Point m_translation;
    };

    /**
     * @class RotateCommand
     * @brief Rotation command
     */
    class RotateCommand : public TransformCommand {
    public:
        RotateCommand(const std::vector<ShapePtr>& shapes,
            const Point& axisPoint, const Point& axisDirection,
            double angleRadians);

        void SetTransformParameters() override;
        void SetRotationAxis(const Point& axisPoint, const Point& axisDirection);
        void SetRotationAngle(double angleRadians);
        void SetRotationAngleDegrees(double angleDegrees);

    protected:
        gp_Trsf CreateTransformation() const override;
        const char* GetTypeName() const override;

    private:
        Point m_axisPoint;
        Point m_axisDirection;
        double m_angleRadians;
    };

    /**
     * @class ScaleCommand
     * @brief Scaling command
     */
    class ScaleCommand : public TransformCommand {
    public:
        ScaleCommand(const std::vector<ShapePtr>& shapes,
            const Point& centerPoint, double scaleFactor);
        ScaleCommand(const std::vector<ShapePtr>& shapes,
            const Point& centerPoint, double scaleX, double scaleY, double scaleZ);

        void SetTransformParameters() override;
        void SetScaleCenter(const Point& centerPoint);
        void SetUniformScale(double scaleFactor);
        void SetNonUniformScale(double scaleX, double scaleY, double scaleZ);
        bool Execute() override;
        std::vector<ShapePtr> GetTransformedShapes() const override;

    protected:
        gp_Trsf CreateTransformation() const override;
        const char* GetTypeName() const override;

    private:
        Point m_centerPoint;
        double m_scaleX, m_scaleY, m_scaleZ;
        bool m_isUniform;
    };

} // namespace cad_core
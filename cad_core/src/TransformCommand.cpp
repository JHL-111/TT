#include "cad_core/TransformCommand.h"
#include <BRepBuilderAPI_Transform.hxx>
#include <gp_Vec.hxx>
#include <gp_Ax1.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <Standard_Real.hxx>
#include <cmath>
#include <BRepBuilderAPI_GTransform.hxx>
#include <gp_GTrsf.hxx>
#include <gp_Mat.hxx>
#include <gp_XYZ.hxx>

namespace cad_core {

    // =============================================================================
    // TransformCommand base class implementation
    // =============================================================================

    TransformCommand::TransformCommand(const std::vector<ShapePtr>& shapes, TransformationType type)
        : m_originalShapes(shapes), m_type(type), m_executed(false) {
    }

    bool TransformCommand::Execute() {
        if (m_executed) {
            return true;
        }

        try {
            // Build the transformation matrix
            gp_Trsf transformation = CreateTransformation();

            // Apply the transformation to each shape
            m_transformedShapes.clear();
            m_transformedShapes.reserve(m_originalShapes.size());

            for (const auto& shape : m_originalShapes) {
                if (!shape || !shape->IsValid()) {
                    continue;
                }

                // Apply the transformation
                BRepBuilderAPI_Transform transformer(shape->GetOCCTShape(), transformation);

                if (!transformer.IsDone()) {
                    return false;
                }

                // Store the transformed shape
                auto transformedShape = std::make_shared<Shape>(transformer.Shape());
                m_transformedShapes.push_back(transformedShape);
            }

            m_executed = true;
            return true;
        }
        catch (const std::exception& e) {
            return false;
        }
    }

    bool TransformCommand::Undo() {
        if (!m_executed) {
            return false;
        }

        // Simply mark as not executed; actual OCAF operations are handled by MainWindow
        m_executed = false;
        return true;
    }

    bool TransformCommand::Redo() {
        if (m_executed) {
            return true;
        }

        return Execute();
    }

    const char* TransformCommand::GetName() const {
        return GetTypeName();
    }

    std::vector<ShapePtr> TransformCommand::GetTransformedShapes() const {
        if (!m_executed) {
            // Create temporary transformed shapes for preview
            std::vector<ShapePtr> previewShapes;
            previewShapes.reserve(m_originalShapes.size());

            try {
                gp_Trsf transformation = CreateTransformation();

                for (const auto& shape : m_originalShapes) {
                    if (!shape || !shape->IsValid()) {
                        continue;
                    }

                    BRepBuilderAPI_Transform transformer(shape->GetOCCTShape(), transformation);
                    if (transformer.IsDone()) {
                        auto previewShape = std::make_shared<Shape>(transformer.Shape());
                        previewShapes.push_back(previewShape);
                    }
                }
            }
            catch (const std::exception& e) {
                // Return empty vector if transformation fails
            }

            return previewShapes;
        }

        return m_transformedShapes;
    }

    // =============================================================================
    // TranslateCommand implementation
    // =============================================================================

    TranslateCommand::TranslateCommand(const std::vector<ShapePtr>& shapes, const Point& translation)
        : TransformCommand(shapes, TransformationType::Translate), m_translation(translation) {
    }

    TranslateCommand::TranslateCommand(const std::vector<ShapePtr>& shapes, double dx, double dy, double dz)
        : TransformCommand(shapes, TransformationType::Translate), m_translation(dx, dy, dz) {
    }

    void TranslateCommand::SetTransformParameters() {
        // Called when parameters are set via a dialog
    }

    void TranslateCommand::SetTranslation(const Point& translation) {
        m_translation = translation;
    }

    void TranslateCommand::SetTranslation(double dx, double dy, double dz) {
        m_translation = Point(dx, dy, dz);
    }

    gp_Trsf TranslateCommand::CreateTransformation() const {
        gp_Trsf transform;
        gp_Vec translation(m_translation.X(), m_translation.Y(), m_translation.Z());
        transform.SetTranslation(translation);
        return transform;
    }

    const char* TranslateCommand::GetTypeName() const {
        return "translation";
    }

    // =============================================================================
    // RotateCommand implementation
    // =============================================================================

    RotateCommand::RotateCommand(const std::vector<ShapePtr>& shapes,
        const Point& axisPoint, const Point& axisDirection,
        double angleRadians)
        : TransformCommand(shapes, TransformationType::Rotate),
        m_axisPoint(axisPoint), m_axisDirection(axisDirection), m_angleRadians(angleRadians) {
    }

    void RotateCommand::SetTransformParameters() {
        // Called when parameters are set via a dialog
    }

    void RotateCommand::SetRotationAxis(const Point& axisPoint, const Point& axisDirection) {
        m_axisPoint = axisPoint;
        m_axisDirection = axisDirection;
    }

    void RotateCommand::SetRotationAngle(double angleRadians) {
        m_angleRadians = angleRadians;
    }

    void RotateCommand::SetRotationAngleDegrees(double angleDegrees) {
        m_angleRadians = angleDegrees * M_PI / 180.0;
    }

    gp_Trsf RotateCommand::CreateTransformation() const {
        gp_Trsf transform;

        // Build the rotation axis
        gp_Pnt axisPoint(m_axisPoint.X(), m_axisPoint.Y(), m_axisPoint.Z());
        gp_Dir axisDirection(m_axisDirection.X(), m_axisDirection.Y(), m_axisDirection.Z());
        gp_Ax1 rotationAxis(axisPoint, axisDirection);

        // Set the rotation transformation
        transform.SetRotation(rotationAxis, m_angleRadians);

        return transform;
    }

    const char* RotateCommand::GetTypeName() const {
        return "rotation";
    }

    // =============================================================================
    // ScaleCommand implementation
    // =============================================================================

    ScaleCommand::ScaleCommand(const std::vector<ShapePtr>& shapes,
        const Point& centerPoint, double scaleFactor)
        : TransformCommand(shapes, TransformationType::Scale),
        m_centerPoint(centerPoint), m_scaleX(scaleFactor), m_scaleY(scaleFactor),
        m_scaleZ(scaleFactor), m_isUniform(true) {
    }

    ScaleCommand::ScaleCommand(const std::vector<ShapePtr>& shapes,
        const Point& centerPoint, double scaleX, double scaleY, double scaleZ)
        : TransformCommand(shapes, TransformationType::Scale),
        m_centerPoint(centerPoint), m_scaleX(scaleX), m_scaleY(scaleY),
        m_scaleZ(scaleZ), m_isUniform(false) {
    }

    void ScaleCommand::SetTransformParameters() {
        // Called when parameters are set via a dialog
    }

    void ScaleCommand::SetScaleCenter(const Point& centerPoint) {
        m_centerPoint = centerPoint;
    }

    void ScaleCommand::SetUniformScale(double scaleFactor) {
        m_scaleX = m_scaleY = m_scaleZ = scaleFactor;
        m_isUniform = true;
    }

    void ScaleCommand::SetNonUniformScale(double scaleX, double scaleY, double scaleZ) {
        m_scaleX = scaleX;
        m_scaleY = scaleY;
        m_scaleZ = scaleZ;
        m_isUniform = false;
    }

    bool ScaleCommand::Execute() {
        if (m_executed) {
            return true;
        }

        try {
            m_transformedShapes.clear();
            m_transformedShapes.reserve(m_originalShapes.size());

            if (m_isUniform) {
                // [Uniform scale] - use the standard transformation path
                gp_Trsf transformation = CreateTransformation();
                for (const auto& shape : m_originalShapes) {
                    if (!shape || !shape->IsValid()) continue;
                    BRepBuilderAPI_Transform transformer(shape->GetOCCTShape(), transformation);
                    if (transformer.IsDone()) {
                        m_transformedShapes.push_back(std::make_shared<Shape>(transformer.Shape()));
                    }
                }
            }
            else {
                // [Non-uniform scale] - use the generalised transformation path (gp_GTrsf)
                gp_GTrsf gTrsf;

                // 1. Set the scale matrix (diagonal matrix)
                gp_Mat mat(m_scaleX, 0.0, 0.0,
                    0.0, m_scaleY, 0.0,
                    0.0, 0.0, m_scaleZ);
                gTrsf.SetVectorialPart(mat);

                // 2. Handle the scale centre offset
                // Transform formula: P' = Center + Mat * (P - Center) = Mat * P + (Center - Mat * Center)
                gp_XYZ center(m_centerPoint.X(), m_centerPoint.Y(), m_centerPoint.Z());
                gp_XYZ transPart = center - (mat * center);
                gTrsf.SetTranslationPart(transPart);

                // 3. Apply the generalised transformation (converts geometry to NURBS)
                for (const auto& shape : m_originalShapes) {
                    if (!shape || !shape->IsValid()) continue;

                    BRepBuilderAPI_GTransform transformer(shape->GetOCCTShape(), gTrsf, Standard_True);
                    if (transformer.IsDone()) {
                        m_transformedShapes.push_back(std::make_shared<Shape>(transformer.Shape()));
                    }
                }
            }

            m_executed = true;
            return true;
        }
        catch (const std::exception& e) {
            return false;
        }
    }

    std::vector<ShapePtr> ScaleCommand::GetTransformedShapes() const {
        if (!m_executed) {
            std::vector<ShapePtr> previewShapes;
            previewShapes.reserve(m_originalShapes.size());

            try {
                if (m_isUniform) {
                    gp_Trsf transformation = CreateTransformation();
                    for (const auto& shape : m_originalShapes) {
                        if (!shape || !shape->IsValid()) continue;
                        BRepBuilderAPI_Transform transformer(shape->GetOCCTShape(), transformation);
                        if (transformer.IsDone()) {
                            previewShapes.push_back(std::make_shared<Shape>(transformer.Shape()));
                        }
                    }
                }
                else {
                    gp_GTrsf gTrsf;
                    gp_Mat mat(m_scaleX, 0.0, 0.0,
                        0.0, m_scaleY, 0.0,
                        0.0, 0.0, m_scaleZ);
                    gTrsf.SetVectorialPart(mat);

                    gp_XYZ center(m_centerPoint.X(), m_centerPoint.Y(), m_centerPoint.Z());
                    gp_XYZ transPart = center - (mat * center);
                    gTrsf.SetTranslationPart(transPart);

                    for (const auto& shape : m_originalShapes) {
                        if (!shape || !shape->IsValid()) continue;
                        BRepBuilderAPI_GTransform transformer(shape->GetOCCTShape(), gTrsf, Standard_True);
                        if (transformer.IsDone()) {
                            previewShapes.push_back(std::make_shared<Shape>(transformer.Shape()));
                        }
                    }
                }
            }
            catch (const std::exception& e) {
                // Return empty vector
            }
            return previewShapes;
        }
        return m_transformedShapes;
    }

    gp_Trsf ScaleCommand::CreateTransformation() const {
        gp_Trsf transform;

        if (m_isUniform) {
            // Uniform scale: return the standard matrix
            gp_Pnt centerPoint(m_centerPoint.X(), m_centerPoint.Y(), m_centerPoint.Z());
            transform.SetScale(centerPoint, m_scaleX);
        }
        else {
            // Non-uniform scale: throw - caller must use the GTrsf path instead
            throw std::logic_error("Non-uniform scale cannot use CreateTransformation!");
        }

        return transform;
    }

    const char* ScaleCommand::GetTypeName() const {
        return "scale";
    }

} // namespace cad_core
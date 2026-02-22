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
// TransformCommand 基类实现
// =============================================================================

TransformCommand::TransformCommand(const std::vector<ShapePtr>& shapes, TransformationType type)
    : m_originalShapes(shapes), m_type(type), m_executed(false) {
}

bool TransformCommand::Execute() {
    if (m_executed) {
        return true;
    }

    try {
        // 创建变换矩阵
        gp_Trsf transformation = CreateTransformation();

        // 对每个形状应用变换
        m_transformedShapes.clear();
        m_transformedShapes.reserve(m_originalShapes.size());

        for (const auto& shape : m_originalShapes) {
            if (!shape || !shape->IsValid()) {
                continue;
            }

            // 应用变换
            BRepBuilderAPI_Transform transformer(shape->GetOCCTShape(), transformation);

            if (!transformer.IsDone()) {
                return false;
            }

            // 创建变换后的形状
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
    
    // 简单地标记为未执行，实际的OCAF操作由MainWindow处理
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
        // 为预览创建临时变换形状
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
            // 返回空vector如果变换失败
        }

        return previewShapes;
    }

    return m_transformedShapes;
}

// =============================================================================
// TranslateCommand 实现
// =============================================================================

TranslateCommand::TranslateCommand(const std::vector<ShapePtr>& shapes, const Point& translation)
    : TransformCommand(shapes, TransformationType::Translate), m_translation(translation) {
}

TranslateCommand::TranslateCommand(const std::vector<ShapePtr>& shapes, double dx, double dy, double dz)
    : TransformCommand(shapes, TransformationType::Translate), m_translation(dx, dy, dz) {
}

void TranslateCommand::SetTransformParameters() {
    // 在对话框中设置参数时调用
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
    return "平移";
}

// =============================================================================
// RotateCommand 实现  
// =============================================================================

RotateCommand::RotateCommand(const std::vector<ShapePtr>& shapes,
                           const Point& axisPoint, const Point& axisDirection, 
                           double angleRadians)
    : TransformCommand(shapes, TransformationType::Rotate),
      m_axisPoint(axisPoint), m_axisDirection(axisDirection), m_angleRadians(angleRadians) {
}

void RotateCommand::SetTransformParameters() {
    // 在对话框中设置参数时调用
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
    
    // 创建旋转轴
    gp_Pnt axisPoint(m_axisPoint.X(), m_axisPoint.Y(), m_axisPoint.Z());
    gp_Dir axisDirection(m_axisDirection.X(), m_axisDirection.Y(), m_axisDirection.Z());
    gp_Ax1 rotationAxis(axisPoint, axisDirection);
    
    // 设置旋转变换
    transform.SetRotation(rotationAxis, m_angleRadians);
    
    return transform;
}

const char* RotateCommand::GetTypeName() const {
    return "旋转";
}

// =============================================================================
// ScaleCommand 实现
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
    // 在对话框中设置参数时调用
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
            // 【均匀缩放】走标准路线
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
            // 【非均匀缩放】走广义变换路线 (gp_GTrsf)
            gp_GTrsf gTrsf;

            // 1. 设置缩放矩阵 (对角矩阵)
            gp_Mat mat(m_scaleX, 0.0, 0.0,
                0.0, m_scaleY, 0.0,
                0.0, 0.0, m_scaleZ);
            gTrsf.SetVectorialPart(mat);

            // 2. 处理缩放中心点偏移
            // 变换公式: P' = Center + Mat * (P - Center) = Mat * P + (Center - Mat * Center)
            gp_XYZ center(m_centerPoint.X(), m_centerPoint.Y(), m_centerPoint.Z());
            gp_XYZ transPart = center - (mat * center);
            gTrsf.SetTranslationPart(transPart);

            // 3. 应用广义变换
            for (const auto& shape : m_originalShapes) {
                if (!shape || !shape->IsValid()) continue;

                // 使用 BRepBuilderAPI_GTransform，Standard_True 表示生成新的形状而不破坏原几何特征
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
            // 返回空
        }
        return previewShapes;
    }
    return m_transformedShapes;
}

gp_Trsf ScaleCommand::CreateTransformation() const {
    gp_Trsf transform;
    
    if (m_isUniform) {
        // 均匀缩放
        gp_Pnt centerPoint(m_centerPoint.X(), m_centerPoint.Y(), m_centerPoint.Z());
        transform.SetScale(centerPoint, m_scaleX);
    } else {
        // 非均匀缩放需要组合变换
        // 先平移到原点，再缩放，再平移回去
        gp_Trsf translateToOrigin, scale, translateBack;
        
        gp_Vec translation(-m_centerPoint.X(), -m_centerPoint.Y(), -m_centerPoint.Z());
        translateToOrigin.SetTranslation(translation);
        
        // OpenCASCADE不直接支持非均匀缩放，需要使用矩阵变换
        // 这里简化为均匀缩放，实际应用中可能需要更复杂的处理
        scale.SetScale(gp_Pnt(0, 0, 0), m_scaleX);
        
        gp_Vec translationBack(m_centerPoint.X(), m_centerPoint.Y(), m_centerPoint.Z());
        translateBack.SetTranslation(translationBack);
        
        // 组合变换
        transform = translateBack * scale * translateToOrigin;
    }
    
    return transform;
}

const char* ScaleCommand::GetTypeName() const {
    return "缩放";
}

} // namespace cad_core
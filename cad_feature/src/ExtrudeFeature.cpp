#include "cad_feature/ExtrudeFeature.h"
#include "cad_core/CreateBoxCommand.h"
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <gp_Vec.hxx>

namespace cad_feature {

ExtrudeFeature::ExtrudeFeature() : Feature(FeatureType::Extrude, "Extrude") {
    SetParameter("distance", 10.0);
    SetParameter("direction_x", 0.0);
    SetParameter("direction_y", 0.0);
    SetParameter("direction_z", 1.0);
    SetParameter("taper_angle", 0.0);
    SetParameter("midplane", 0.0);
}

ExtrudeFeature::ExtrudeFeature(const std::string& name) : Feature(FeatureType::Extrude, name) {
    SetParameter("distance", 10.0);
    SetParameter("direction_x", 0.0);
    SetParameter("direction_y", 0.0);
    SetParameter("direction_z", 1.0);
    SetParameter("taper_angle", 0.0);
    SetParameter("midplane", 0.0);
}

void ExtrudeFeature::SetSketch(const cad_sketch::SketchPtr& sketch) {
    m_sketch = sketch;
}

const cad_sketch::SketchPtr& ExtrudeFeature::GetSketch() const {
    return m_sketch;
}

void ExtrudeFeature::SetDistance(double distance) {
    SetParameter("distance", distance);
}

double ExtrudeFeature::GetDistance() const {
    return GetParameter("distance");
}

void ExtrudeFeature::SetDirection(double x, double y, double z) {
    SetParameter("direction_x", x);
    SetParameter("direction_y", y);
    SetParameter("direction_z", z);
}

void ExtrudeFeature::GetDirection(double& x, double& y, double& z) const {
    x = GetParameter("direction_x");
    y = GetParameter("direction_y");
    z = GetParameter("direction_z");
}

void ExtrudeFeature::SetTaperAngle(double angle) {
    SetParameter("taper_angle", angle);
}

double ExtrudeFeature::GetTaperAngle() const {
    return GetParameter("taper_angle");
}

void ExtrudeFeature::SetMidplane(bool midplane) {
    SetParameter("midplane", midplane ? 1.0 : 0.0);
}

bool ExtrudeFeature::GetMidplane() const {
    return GetParameter("midplane") != 0.0;
}

cad_core::ShapePtr ExtrudeFeature::CreateShape() const {
    if (!ValidateParameters()) {
        return nullptr;
    }
    
    return ExtrudeSketch();
}

bool ExtrudeFeature::ValidateParameters() const {
    if (!IsSketchValid()) {
        return false;
    }
    
    double distance = GetDistance();
    if (distance <= 0.0) {
        return false;
    }
    
    double dx, dy, dz;
    GetDirection(dx, dy, dz);
    double length = std::sqrt(dx*dx + dy*dy + dz*dz);
    if (length < 1e-10) {
        return false;
    }
    
    return true;
}

std::shared_ptr<cad_core::ICommand> ExtrudeFeature::CreateCommand() const {
    // For now, return a simple box command as placeholder
    return std::make_shared<cad_core::CreateBoxCommand>(GetDistance(), GetDistance(), GetDistance());
}

bool ExtrudeFeature::IsSketchValid() const {
    return m_sketch && !m_sketch->IsEmpty();
}

cad_core::ShapePtr ExtrudeFeature::ExtrudeSketch() const {
    // 1. 检查草图是否有效
    if (!IsSketchValid()) {
        return nullptr;
    }

    try {
        // 2. 获取我们在 Sketch.cpp 里写好的闭合线框 (Wire)
        TopoDS_Wire profileWire = m_sketch->GetProfileWire();

        // 3. 检查线框是否闭合 (Check if closed)
        // 如果用户画的线没有首尾相连，这里就会拦截下来
        if (profileWire.IsNull() || !profileWire.Closed()) {
            // TODO: 未来可以在这里抛出异常或通过 UI 提示用户"草图未闭合"
            return nullptr;
        }

        // 4. 将线框转化为平面 (Make Face)
        BRepBuilderAPI_MakeFace faceMaker(profileWire);
        if (!faceMaker.IsDone()) {
            return nullptr;
        }
        TopoDS_Face profileFace = faceMaker.Face();

        // 5. 获取拉伸的距离和方向 (Get Distance and Direction)
        double distance = GetDistance();
        double dx, dy, dz;
        GetDirection(dx, dy, dz);

        // 构造拉伸向量 (Extrusion Vector)
        gp_Vec extrudeVec(dx, dy, dz);
        if (extrudeVec.Magnitude() > 1e-10) {
            extrudeVec.Normalize(); // 归一化方向向量
            extrudeVec *= distance; // 乘以拉伸距离
        }
        else {
            // 如果方向没设置好，默认沿着 Z 轴拉伸
            extrudeVec = gp_Vec(0, 0, distance);
        }

        // 6. 执行真正的 3D 拉伸 (Make Prism)
        BRepPrimAPI_MakePrism prismMaker(profileFace, extrudeVec);
        if (!prismMaker.IsDone()) {
            return nullptr;
        }

        // 7. 将生成的 3D 实体 (Solid) 包装进你的 Shape 类中返回
        return std::make_shared<cad_core::Shape>(prismMaker.Shape());

    }
    catch (...) {
        // 捕获任何 OpenCASCADE 可能抛出的底层异常
        return nullptr;
    }
}

} // namespace cad_feature
#include "cad_feature/ExtrudeFeature.h"
#include "cad_core/CreateBoxCommand.h"

// 包含 OCC 相关的拉伸库
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Tool.hxx>
#include <GeomLProp_SLProps.hxx>
#include <BRepTools.hxx>
#include <TopoDS.hxx>
#include <gp_Vec.hxx>

namespace cad_feature {

    ExtrudeFeature::ExtrudeFeature() : Feature(FeatureType::Extrude, "Extrude") {
        SetParameter("distance", 10.0);
    }

    ExtrudeFeature::ExtrudeFeature(const std::string& name) : Feature(FeatureType::Extrude, name) {
        SetParameter("distance", 10.0);
    }

    void ExtrudeFeature::SetProfileShape(const cad_core::ShapePtr& profile) {
        m_profileShape = profile;
    }

    const cad_core::ShapePtr& ExtrudeFeature::GetProfileShape() const {
        return m_profileShape;
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

    bool ExtrudeFeature::ValidateParameters() const {
        if (!m_profileShape) return false;
        if (GetDistance() <= 0.0) return false;
        return true;
    }

    std::shared_ptr<cad_core::ICommand> ExtrudeFeature::CreateCommand() const {
        return nullptr; // 暂时不用
    }

    // Feature 自己执行拉伸操作
    cad_core::ShapePtr ExtrudeFeature::CreateShape() const {
        if (!ValidateParameters()) {
            return nullptr;
        }

        try {
            TopoDS_Shape topoShape = m_profileShape->GetOCCTShape();
            TopoDS_Face profileFace;
            gp_Dir extrudeNormal(0, 0, 1);

            // 识别选中的是线框还是平面
            if (topoShape.ShapeType() == TopAbs_WIRE || topoShape.ShapeType() == TopAbs_EDGE) {
                BRepBuilderAPI_MakeWire wireMaker;
                if (topoShape.ShapeType() == TopAbs_EDGE) {
                    wireMaker.Add(TopoDS::Edge(topoShape));
                }
                else {
                    wireMaker.Add(TopoDS::Wire(topoShape));
                }
                if (!wireMaker.IsDone() || !wireMaker.Wire().Closed()) {
                    throw std::runtime_error("Profile is not closed!");
                }
                BRepBuilderAPI_MakeFace faceMaker(wireMaker.Wire(), true);
                if (!faceMaker.IsDone()) throw std::runtime_error("Failed to make face.");
                profileFace = faceMaker.Face();
            }
            else if (topoShape.ShapeType() == TopAbs_FACE) {
                profileFace = TopoDS::Face(topoShape);
            }
            else {
                return nullptr;
            }

            // 计算法线 (自动计算面的垂直方向)
            Handle(Geom_Surface) surface = BRep_Tool::Surface(profileFace);
            Standard_Real uMin, uMax, vMin, vMax;
            BRepTools::UVBounds(profileFace, uMin, uMax, vMin, vMax);
            Standard_Real uMid = (uMin + uMax) / 2.0;
            Standard_Real vMid = (vMin + vMax) / 2.0;
            GeomLProp_SLProps props(surface, uMid, vMid, 1, Precision::Confusion());

            if (props.IsNormalDefined()) {
                extrudeNormal = props.Normal();
                if (profileFace.Orientation() == TopAbs_REVERSED) extrudeNormal.Reverse();
            }

            // 构造拉伸向量
            gp_Vec extrudeVec(extrudeNormal.XYZ() * GetDistance());

            // 执行 3D 拉伸
            BRepPrimAPI_MakePrism prismMaker(profileFace, extrudeVec);
            if (!prismMaker.IsDone()) return nullptr;

            return std::make_shared<cad_core::Shape>(prismMaker.Shape());

        }
        catch (...) {
            return nullptr;
        }
    }

} // namespace cad_feature
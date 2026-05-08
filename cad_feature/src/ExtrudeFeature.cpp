#include "cad_feature/ExtrudeFeature.h"
#include "cad_core/CreateBoxCommand.h"

// OCC extrusion libraries
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

 
    bool ExtrudeFeature::ValidateParameters() const {
        if (!m_profileShape) return false;
        if (GetDistance() <= 0.0) return false;
        return true;
    }

    std::shared_ptr<cad_core::ICommand> ExtrudeFeature::CreateCommand() const {
        return nullptr;
    }

    // The feature performs the extrusion operation itself
    cad_core::ShapePtr ExtrudeFeature::CreateShape() const {
        if (!ValidateParameters()) {
            return nullptr;
        }

        try {
            TopoDS_Shape topoShape = m_profileShape->GetOCCTShape();
            gp_Dir extrudeNormal(0, 0, 1);

            // The extrude feature operates on planar faces produced by the
            // closed-profile-detection pipeline 
            if (topoShape.ShapeType() != TopAbs_FACE) {
                return nullptr;
            }
            TopoDS_Face profileFace = TopoDS::Face(topoShape);

            // Compute the face normal (automatic surface normal direction)
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

            // Build the extrusion vector
            gp_Vec extrudeVec(extrudeNormal.XYZ() * GetDistance());

            // Perform the 3D extrusion
            BRepPrimAPI_MakePrism prismMaker(profileFace, extrudeVec);
            if (!prismMaker.IsDone()) return nullptr;

            return std::make_shared<cad_core::Shape>(prismMaker.Shape());

        }
        catch (...) {
            return nullptr;
        }
    }

} // namespace cad_feature
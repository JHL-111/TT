#include "cad_feature/RevolveFeature.h"
#include "cad_core/CreateCylinderCommand.h"
#include <BRepPrimAPI_MakeRevol.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <gp_Ax1.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <cmath>

namespace cad_feature {

    RevolveFeature::RevolveFeature() : Feature(FeatureType::Revolve, "Revolve") {
        SetParameter("angle", 2.0 * M_PI);
        SetParameter("axis_x", 0.0);
        SetParameter("axis_y", 0.0);
        SetParameter("axis_z", 1.0);
        SetParameter("axis_origin_x", 0.0);
        SetParameter("axis_origin_y", 0.0);
        SetParameter("axis_origin_z", 0.0);
  
    }

    RevolveFeature::RevolveFeature(const std::string& name) : Feature(FeatureType::Revolve, name) {
        SetParameter("angle", 2.0 * M_PI);
        SetParameter("axis_x", 0.0);
        SetParameter("axis_y", 0.0);
        SetParameter("axis_z", 1.0);
        SetParameter("axis_origin_x", 0.0);
        SetParameter("axis_origin_y", 0.0);
        SetParameter("axis_origin_z", 0.0);
    }

    void RevolveFeature::SetSketch(const cad_sketch::SketchPtr& sketch) {
        m_sketch = sketch;
    }

    const cad_sketch::SketchPtr& RevolveFeature::GetSketch() const {
        return m_sketch;
    }

    void RevolveFeature::SetProfileShape(const cad_core::ShapePtr& profile) {
        m_profileShape = profile;
    }

    const cad_core::ShapePtr& RevolveFeature::GetProfileShape() const {
        return m_profileShape;
    }

    void RevolveFeature::SetAngle(double angle) {
        SetParameter("angle", angle);
    }

    double RevolveFeature::GetAngle() const {
        return GetParameter("angle");
    }

    void RevolveFeature::SetAxis(double x, double y, double z) {
        SetParameter("axis_x", x);
        SetParameter("axis_y", y);
        SetParameter("axis_z", z);
    }

    void RevolveFeature::GetAxis(double& x, double& y, double& z) const {
        x = GetParameter("axis_x");
        y = GetParameter("axis_y");
        z = GetParameter("axis_z");
    }

    void RevolveFeature::SetAxisOrigin(double x, double y, double z) {
        SetParameter("axis_origin_x", x);
        SetParameter("axis_origin_y", y);
        SetParameter("axis_origin_z", z);
    }

    void RevolveFeature::GetAxisOrigin(double& x, double& y, double& z) const {
        x = GetParameter("axis_origin_x");
        y = GetParameter("axis_origin_y");
        z = GetParameter("axis_origin_z");
    }


    cad_core::ShapePtr RevolveFeature::CreateShape() const {
        if (!ValidateParameters()) {
            return nullptr;
        }
        return RevolveSketch();
    }

    bool RevolveFeature::ValidateParameters() const {
        //  at least one source of profile
        if (!m_profileShape && !IsSketchValid()) {
            return false;
        }

        double angle = GetAngle();
        if (angle <= 0.0 || angle > 2.0 * M_PI) {
            return false;
        }

        double ax, ay, az;
        GetAxis(ax, ay, az);
        double length = std::sqrt(ax * ax + ay * ay + az * az);
        if (length < 1e-10) {
            return false;
        }

        return true;
    }

    std::shared_ptr<cad_core::ICommand> RevolveFeature::CreateCommand() const {
        return nullptr; 
    }

    bool RevolveFeature::IsSketchValid() const {
        return m_sketch && !m_sketch->IsEmpty();
    }

    cad_core::ShapePtr RevolveFeature::RevolveSketch() const {
        try {
            // 1. Get profile: Use the directly input profileShape first
            TopoDS_Face profileFace;

            if (m_profileShape && m_profileShape->IsValid()) {
                TopoDS_Shape topoShape = m_profileShape->GetOCCTShape();
                if (topoShape.ShapeType() == TopAbs_FACE) {
                    profileFace = TopoDS::Face(topoShape);
                }
            }
            else if (IsSketchValid()) {
                auto profiles = m_sketch->GetProfiles();
                if (!profiles.empty()) {
                    profileFace = profiles[0]->GetFace();
                }
            }

            if (profileFace.IsNull()) {
                return nullptr;
            }

            // 2. Construct the rotation axis
            double ox, oy, oz;
            GetAxisOrigin(ox, oy, oz);
            gp_Pnt axisOrigin(ox, oy, oz);

            double ax, ay, az;
            GetAxis(ax, ay, az);
            gp_Dir axisDir(ax, ay, az);

            gp_Ax1 revolveAxis(axisOrigin, axisDir);

            // 3. Get rotation angle
            double angle = GetAngle();

            // 4. Perform revolution
            BRepPrimAPI_MakeRevol revolMaker(profileFace, revolveAxis, angle);

            if (revolMaker.IsDone()) {
                return std::make_shared<cad_core::Shape>(revolMaker.Shape());
            }

            return nullptr;
        }
        catch (...) {
            return nullptr;
        }
    }

} // namespace cad_feature
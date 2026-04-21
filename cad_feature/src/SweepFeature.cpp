#include "cad_feature/SweepFeature.h"
#include <BRepOffsetAPI_MakePipeShell.hxx>
#include <Law_Linear.hxx>
#include <Law_Constant.hxx>
#include <GeomFill_LocationLaw.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Edge.hxx>
#include <TopExp_Explorer.hxx>
#include <BRepAdaptor_CompCurve.hxx>
#include <QDebug>
#include <cmath>
#include <GeomFill_CorrectedFrenet.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


namespace cad_feature {

    SweepFeature::SweepFeature() : Feature(FeatureType::Sweep, "Sweep") {
        SetParameter("twist_angle", 0.0);
        SetParameter("scale_factor", 1.0);
        SetParameter("keep_orientation", 1.0);
    }

    SweepFeature::SweepFeature(const std::string& name) : Feature(FeatureType::Sweep, name) {
        // Same parameter initialisation as the default constructor
        SetParameter("twist_angle", 0.0);
        SetParameter("scale_factor", 1.0);
        SetParameter("keep_orientation", 1.0);
    }

    void SweepFeature::SetProfileShape(const cad_core::ShapePtr& profile) {
        m_profileShape = profile;
    }

    const cad_core::ShapePtr& SweepFeature::GetProfileShape() const {
        return m_profileShape;
    }

    void SweepFeature::SetPathShape(const cad_core::ShapePtr& path) {
        m_pathShape = path;
    }

    const cad_core::ShapePtr& SweepFeature::GetPathShape() const {
        return m_pathShape;
    }

    void SweepFeature::SetTwistAngle(double angle) {
        SetParameter("twist_angle", angle);
    }

    double SweepFeature::GetTwistAngle() const {
        return GetParameter("twist_angle");
    }

    void SweepFeature::SetScaleFactor(double factor) {
        SetParameter("scale_factor", factor);
    }

    double SweepFeature::GetScaleFactor() const {
        return GetParameter("scale_factor");
    }

    void SweepFeature::SetKeepOriginalOrientation(bool keep) {
        SetParameter("keep_orientation", keep ? 1.0 : 0.0);
    }

    bool SweepFeature::GetKeepOriginalOrientation() const {
        return GetParameter("keep_orientation") != 0.0;
    }

    cad_core::ShapePtr SweepFeature::CreateShape() const {
        if (!ValidateParameters()) return nullptr;
        return SweepProfile();
    }

    bool SweepFeature::ValidateParameters() const {
        if (!IsProfileValid() || !IsPathValid()) return false;
        if (GetScaleFactor() <= 0.0) return false;
        return true;
    }

    std::shared_ptr<cad_core::ICommand> SweepFeature::CreateCommand() const {
        return nullptr;
    }

    bool SweepFeature::IsProfileValid() const {
        return m_profileShape && m_profileShape->IsValid();
    }

    bool SweepFeature::IsPathValid() const {
        return m_pathShape && m_pathShape->IsValid();
    }

    // Generate the actual swept solid
    cad_core::ShapePtr SweepFeature::SweepProfile() const {
        if (!IsProfileValid() || !IsPathValid()) return nullptr;

        try {
            TopoDS_Shape profileOCC = m_profileShape->GetOCCTShape();
            TopoDS_Shape pathOCC = m_pathShape->GetOCCTShape();

            // 1. Path processing: must be a Wire
            TopoDS_Wire pathWire;
            if (pathOCC.ShapeType() == TopAbs_WIRE) {
                pathWire = TopoDS::Wire(pathOCC);
            }
            else if (pathOCC.ShapeType() == TopAbs_EDGE) {
                pathWire = BRepBuilderAPI_MakeWire(TopoDS::Edge(pathOCC)).Wire();
            }
            else {
                return nullptr;
            }

            // Profile processing: use Wire directly, do NOT convert to Face.
            // If the input is a Face, extract its outer boundary Wire.
            TopoDS_Shape sweepSection = profileOCC;
            if (profileOCC.ShapeType() == TopAbs_FACE) {
                TopExp_Explorer exp(profileOCC, TopAbs_WIRE);
                if (exp.More()) sweepSection = exp.Current();
            }

            BRepOffsetAPI_MakePipeShell pipeShell(pathWire);

            // Build the 3D twist guide spine
            double twistAngle = GetTwistAngle(); // Twist angle in degrees from the UI

            if (std::abs(twistAngle) > 1e-6) {
                // Convert degrees to radians
                double twistRad = twistAngle * M_PI / 180.0;

                // 1. Wrap the path wire as a differentiable curve handle
                Handle(Adaptor3d_Curve) hPathCurve = new BRepAdaptor_CompCurve(pathWire, Standard_True);

                // 2. Use CorrectedFrenet mode to compute a stable frame at each point along the path
                Handle(GeomFill_CorrectedFrenet) trihedron = new GeomFill_CorrectedFrenet();
                trihedron->SetCurve(hPathCurve);

                // 3. Sample 30 points - sufficient for a very smooth twist
                int sampleCount = 30;
                Handle(TColgp_HArray1OfPnt) guidePts = new TColgp_HArray1OfPnt(1, sampleCount);

                double firstParam = hPathCurve->FirstParameter();
                double lastParam = hPathCurve->LastParameter();
                double paramStep = (lastParam - firstParam) / (sampleCount - 1);

                for (int i = 1; i <= sampleCount; ++i) {
                    double t = firstParam + (i - 1) * paramStep;
                    if (i == sampleCount) t = lastParam; // Guard against floating-point overshoot at end

                    gp_Vec T, N, B;
                    trihedron->D0(t, T, N, B); // Tangent (T), normal (N), binormal (B) at this point

                    gp_Pnt P = hPathCurve->Value(t); // Physical centre point on the path

                    // Compute the twist angle at this point proportional to progress along the path
                    double currentTwist = twistRad * (t - firstParam) / (lastParam - firstParam);

                    // Rotate the normal vector (N) around the tangent (T)
                    gp_Ax1 tangentAxis(P, gp_Dir(T));
                    gp_Dir rotatedNormal = gp_Dir(N).Rotated(tangentAxis, currentTwist);

                    // Offset 10 units along the rotated normal to obtain the guide point
                    gp_Pnt guideP = P.Translated(gp_Vec(rotatedNormal) * 10.0);
                    guidePts->SetValue(i, guideP);
                }

                // 4. Interpolate the 30 control points into a smooth 3D B-spline curve
                GeomAPI_Interpolate interp(guidePts, Standard_False, Precision::Confusion());
                interp.Perform();

                if (interp.IsDone()) {
                    TopoDS_Edge guideEdge = BRepBuilderAPI_MakeEdge(interp.Curve());
                    TopoDS_Wire guideWire = BRepBuilderAPI_MakeWire(guideEdge);

                    // 5. Pass the guide wire to the sweep engine.
                    // Standard_False = parameterisation-based synchronisation
                    pipeShell.SetMode(guideWire, Standard_False);
                }
                else {
                    // Interpolation failed - fall back to default (no twist)
                    pipeShell.SetMode(!GetKeepOriginalOrientation());
                }
            }
            else {
                // No twist - use standard normal-keeping logic
                pipeShell.SetMode(!GetKeepOriginalOrientation());
            }

            // Apply scaling law
            double scale = GetScaleFactor();
            if (std::abs(scale - 1.0) > 1e-6) {
                BRepAdaptor_CompCurve pathCurve(pathWire, Standard_True);
                double firstParam = pathCurve.FirstParameter();
                double lastParam = pathCurve.LastParameter();

                Handle(Law_Linear) scaleLaw = new Law_Linear();
                scaleLaw->Set(firstParam, 1.0, lastParam, scale);

                // When a scaling law is applied, use SetLaw only - never call Add as well.
                // The cross-section is fully controlled by the dynamic mathematical law.
                pipeShell.SetLaw(sweepSection, scaleLaw, Standard_False, Standard_True);
            }
            else {
                // Only Add the section as a rigid constraint when there is no scaling (factor == 1.0)
                pipeShell.Add(sweepSection);
            }

            pipeShell.Build();

            if (pipeShell.IsDone()) {
                // Convert to a solid as the final step for a more robust result
                pipeShell.MakeSolid();
                return std::make_shared<cad_core::Shape>(pipeShell.Shape());
            }

            return nullptr;
        }
        catch (...) {
            return nullptr;
        }
    }

} // namespace cad_feature
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

namespace cad_feature {

    SweepFeature::SweepFeature() : Feature(FeatureType::Sweep, "Sweep") {
        SetParameter("twist_angle", 0.0);
        SetParameter("scale_factor", 1.0);
        SetParameter("keep_orientation", 1.0);
    }

    SweepFeature::SweepFeature(const std::string& name) : Feature(FeatureType::Sweep, name) {
        // 同上，保持参数初始化
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
        // 
        return nullptr;
    }

    bool SweepFeature::IsProfileValid() const {
        return m_profileShape && m_profileShape->IsValid();
    }

    bool SweepFeature::IsPathValid() const {
        return m_pathShape && m_pathShape->IsValid();
    }

    // 生成真实的 Sweep 实体 
    cad_core::ShapePtr SweepFeature::SweepProfile() const {
        if (!IsProfileValid() || !IsPathValid()) return nullptr;

        try {
            TopoDS_Shape profileOCC = m_profileShape->GetOCCTShape();
            TopoDS_Shape pathOCC = m_pathShape->GetOCCTShape();

            // 1. 路径处理：必须是 Wire
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

            // 2. 截面处理：直接使用 Wire，不要转 Face
            // 如果输入是 Face，提取它的外轮廓 Wire
            TopoDS_Shape sweepSection = profileOCC;
            if (profileOCC.ShapeType() == TopAbs_FACE) {
                TopExp_Explorer exp(profileOCC, TopAbs_WIRE);
                if (exp.More()) sweepSection = exp.Current();
            }

            BRepOffsetAPI_MakePipeShell pipeShell(pathWire);

            // 3. 修正模式设置：KeepOrientation 为 True 时，IsFrenet 应为 False
            // False = CorrectedFrenet (平稳), True = Frenet (随曲率大幅扭转)
            pipeShell.SetMode(!GetKeepOriginalOrientation());

            // 4. 应用缩放规律 (Scaling Law)
            double scale = GetScaleFactor();
            if (std::abs(scale - 1.0) > 1e-6) {
                BRepAdaptor_CompCurve pathCurve(pathWire, Standard_True);
                double firstParam = pathCurve.FirstParameter();
                double lastParam = pathCurve.LastParameter();

                Handle(Law_Linear) scaleLaw = new Law_Linear();
                scaleLaw->Set(firstParam, 1.0, lastParam, scale);

                // 有缩放规律时，只用 SetLaw，绝对不能调 Add！
                // 让截面完全受控于动态数学规律。
                pipeShell.SetLaw(sweepSection, scaleLaw, Standard_False, Standard_True);
            }
            else {
                // 只有在没有任何变形（比例为 1.0）时，才作为刚性约束 Add 进去。
                pipeShell.Add(sweepSection);
            }

            pipeShell.Build();

            if (pipeShell.IsDone()) {
                // 最后一步再转 Solid，这样生成的实体更加鲁棒
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
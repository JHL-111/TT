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

            // 截面处理：直接使用 Wire，不要转 Face
            // 如果输入是 Face，提取它的外轮廓 Wire
            TopoDS_Shape sweepSection = profileOCC;
            if (profileOCC.ShapeType() == TopAbs_FACE) {
                TopExp_Explorer exp(profileOCC, TopAbs_WIRE);
                if (exp.More()) sweepSection = exp.Current();
            }

            BRepOffsetAPI_MakePipeShell pipeShell(pathWire);

        // 生成 3D 扭转引导线 (Auxiliary Spine) 
            double twistAngle = GetTwistAngle(); // 从 UI 获取的扭转度数

            if (std::abs(twistAngle) > 1e-6) {
                // 将角度转为弧度
                double twistRad = twistAngle * M_PI / 180.0;

                // 1. 将路径线框包装为 OCC 底层可求导的曲线句柄
                Handle(Adaptor3d_Curve) hPathCurve = new BRepAdaptor_CompCurve(pathWire, Standard_True);

                // 2. 使用“平稳模式 (CorrectedFrenet)” 计算路径上每一点的稳固标架
                Handle(GeomFill_CorrectedFrenet) trihedron = new GeomFill_CorrectedFrenet();
                trihedron->SetCurve(hPathCurve);

                // 3. 采样 30 个点，像拧麻花一样生成一根“DNA双螺旋引导线”
                int sampleCount = 30; // 30 个采样点足够保证极其顺滑的扭转
                Handle(TColgp_HArray1OfPnt) guidePts = new TColgp_HArray1OfPnt(1, sampleCount);

                double firstParam = hPathCurve->FirstParameter();
                double lastParam = hPathCurve->LastParameter();
                double paramStep = (lastParam - firstParam) / (sampleCount - 1);

                for (int i = 1; i <= sampleCount; ++i) {
                    double t = firstParam + (i - 1) * paramStep;
                    if (i == sampleCount) t = lastParam; // 防止末尾浮点精度溢出

                    gp_Vec T, N, B;
                    trihedron->D0(t, T, N, B); // 计算当前点的切向(T), 法向(N), 副法向(B)

                    gp_Pnt P = hPathCurve->Value(t); // 当前路径所在的物理中心点

                    // 按进度比例计算当前点应该扭转的角度
                    double currentTwist = twistRad * (t - firstParam) / (lastParam - firstParam);

                    // 绕着切线 (T) 旋转法向量 (N)
                    gp_Ax1 tangentAxis(P, gp_Dir(T));
                    gp_Dir rotatedNormal = gp_Dir(N).Rotated(tangentAxis, currentTwist);

                    // 沿着旋转后的法向量往外推一段距离 (比如 10.0 毫米)，得到引导线上的控制点
                    gp_Pnt guideP = P.Translated(gp_Vec(rotatedNormal) * 10.0);
                    guidePts->SetValue(i, guideP);
                }

                // 4. 将这 30 个控制点平滑插值为一根实体的 3D 样条曲线
                GeomAPI_Interpolate interp(guidePts, Standard_False, Precision::Confusion());
                interp.Perform();

                if (interp.IsDone()) {
                    TopoDS_Edge guideEdge = BRepBuilderAPI_MakeEdge(interp.Curve());
                    TopoDS_Wire guideWire = BRepBuilderAPI_MakeWire(guideEdge);

                    // 5. 告诉扫掠引擎：看着这根麻花一样的引导线来决定截面的方向！
                    // Standard_False 表示按参数化进行匹配同步
                    pipeShell.SetMode(guideWire, Standard_False);
                }
                else {
                    // 如果极其罕见地插值失败，退化为默认的无扭转模式
                    pipeShell.SetMode(!GetKeepOriginalOrientation());
                }
            }
            else {
                // 没有扭转参数时，使用标准的法向保持逻辑
                pipeShell.SetMode(!GetKeepOriginalOrientation());
            }


            // 应用缩放规律 (Scaling Law)
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
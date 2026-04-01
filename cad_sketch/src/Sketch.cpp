#include "cad_sketch/Sketch.h"
#include "cad_sketch/SketchLine.h"
#include "cad_sketch/SketchCircle.h"
#include "cad_sketch/SketchArc.h"
#include "cad_sketch/SketchCurve.h" 

#include <iostream>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>
#include <algorithm>
#include <fstream>
#include <TopTools_HSequenceOfShape.hxx>
#include <ShapeAnalysis_FreeBounds.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <Geom_BSplineCurve.hxx>
#include <BOPAlgo_Builder.hxx>
#include <BOPAlgo_BuilderFace.hxx>
#include <TopTools_ListOfShape.hxx>
#include <TopExp_Explorer.hxx>
#include <BRepTools.hxx>
#include <Precision.hxx>
#include <GProp_GProps.hxx>
#include <BRepGProp.hxx>
#include <BRepCheck_Wire.hxx>
#include <BRepCheck_Status.hxx> 


// 这是一个将局部 2D 点转为世界 3D 坐标并生成 OCC Edge 的内部辅助函数
static TopoDS_Shape CreateEdgeFromElement(const cad_sketch::SketchElementPtr& elem, const gp_Ax3& cs) {
    auto Sketch2DToWorld = [](const cad_sketch::SketchPointPtr& pt, const gp_Ax3& ax) {
        return ax.Location().Translated(gp_Vec(ax.XDirection()) * pt->GetX() + gp_Vec(ax.YDirection()) * pt->GetY());
        };

    if (auto line = std::dynamic_pointer_cast<cad_sketch::SketchLine>(elem)) {
        gp_Pnt p1 = Sketch2DToWorld(line->GetStartPoint(), cs);
        gp_Pnt p2 = Sketch2DToWorld(line->GetEndPoint(), cs);
        if (p1.Distance(p2) > Precision::Confusion()) {
            return BRepBuilderAPI_MakeEdge(p1, p2);
        }
    }
    else if (auto circle = std::dynamic_pointer_cast<cad_sketch::SketchCircle>(elem)) {
        gp_Pnt center = Sketch2DToWorld(circle->GetCenter(), cs);
        gp_Ax2 ax2(center, cs.Direction(), cs.XDirection());
        return BRepBuilderAPI_MakeEdge(gp_Circ(ax2, circle->GetRadius()));
    }
    else if (auto arc = std::dynamic_pointer_cast<cad_sketch::SketchArc>(elem)) {
        gp_Pnt center = Sketch2DToWorld(arc->GetCenter(), cs);
        gp_Ax2 ax2(center, cs.Direction(), cs.XDirection());
        return BRepBuilderAPI_MakeEdge(gp_Circ(ax2, arc->GetRadius()), arc->GetStartAngle(), arc->GetEndAngle());
    }
    else if (auto curve = std::dynamic_pointer_cast<cad_sketch::SketchCurve>(elem)) {
        const auto& rawPoints = curve->GetControlPoints();
        std::vector<gp_Pnt> validPoints;
        for (const auto& pt : rawPoints) {
            gp_Pnt worldPt = Sketch2DToWorld(pt, cs);
            if (validPoints.empty() || validPoints.back().Distance(worldPt) > 1e-5) {
                validPoints.push_back(worldPt);
            }
        }

        if (validPoints.size() >= 2) {
            Handle(TColgp_HArray1OfPnt) occPoints = new TColgp_HArray1OfPnt(1, validPoints.size());
            for (size_t i = 0; i < validPoints.size(); ++i) occPoints->SetValue(i + 1, validPoints[i]);

            try {
                GeomAPI_Interpolate interpolator(occPoints, Standard_False, Precision::Confusion());
                interpolator.Perform();
                if (interpolator.IsDone()) {
                    return BRepBuilderAPI_MakeEdge(interpolator.Curve());
                }
            }
            catch (...) {
                return TopoDS_Shape();
            }
        }
    }
    return TopoDS_Shape();
}



namespace cad_sketch {

Sketch::Sketch() : m_name("Sketch") {
}

Sketch::Sketch(const std::string& name) : m_name(name) {
}

const std::string& Sketch::GetName() const {
    return m_name;
}

void Sketch::SetName(const std::string& name) {
    m_name = name;
}

void Sketch::AddElement(const SketchElementPtr& element) {
    m_elements.push_back(element);
}

void Sketch::ClearElements() {
    m_elements.clear();
}

void Sketch::RemoveElement(const SketchElementPtr& element) {
    auto it = std::find(m_elements.begin(), m_elements.end(), element);
    if (it != m_elements.end()) {
        m_elements.erase(it);
    }
}

const std::vector<SketchElementPtr>& Sketch::GetElements() const {
    return m_elements;
}

SketchElementPtr Sketch::GetElementById(int id) const {
    for (const auto& element : m_elements) {
        if (element->GetId() == id) {
            return element;
        }
    }
    return nullptr;
}

void Sketch::AddConstraint(const ConstraintPtr& constraint) {
    m_constraints.push_back(constraint);
    m_solver.AddConstraint(constraint);
}

void Sketch::RemoveConstraint(const ConstraintPtr& constraint) {
    auto it = std::find(m_constraints.begin(), m_constraints.end(), constraint);
    if (it != m_constraints.end()) {
        m_constraints.erase(it);
        m_solver.RemoveConstraint(constraint);
    }
}

void Sketch::ClearConstraints() {
    m_constraints.clear();
    m_solver.ClearConstraints();
}

const std::vector<ConstraintPtr>& Sketch::GetConstraints() const {
    return m_constraints;
}

bool Sketch::SolveConstraints() {
    return m_solver.Solve();
}

bool Sketch::ValidateConstraints() const {
    return m_solver.ValidateConstraints();
}

void Sketch::SelectElement(const SketchElementPtr& element) {
    element->SetSelected(true);
}

void Sketch::DeselectElement(const SketchElementPtr& element) {
    element->SetSelected(false);
}

void Sketch::ClearSelection() {
    for (auto& element : m_elements) {
        element->SetSelected(false);
    }
}

std::vector<SketchElementPtr> Sketch::GetSelectedElements() const {
    std::vector<SketchElementPtr> selected;
    for (const auto& element : m_elements) {
        if (element->IsSelected()) {
            selected.push_back(element);
        }
    }
    return selected;
}

bool Sketch::IsEmpty() const {
    return m_elements.empty();
}

int Sketch::GetElementCount() const {
    return static_cast<int>(m_elements.size());
}

int Sketch::GetConstraintCount() const {
    return static_cast<int>(m_constraints.size());
}



void Sketch::UpdateProfiles(const gp_Ax3& cs) {
    m_profiles.clear();

    TopTools_ListOfShape edgeList;
    for (const auto& elem : m_elements) {
        TopoDS_Shape shape = CreateEdgeFromElement(elem, cs);
        if (!shape.IsNull() && shape.ShapeType() == TopAbs_EDGE) {
            edgeList.Append(shape);
        }
    }
    if (edgeList.IsEmpty()) return;

    // ----------------------------------------------------------------
    // BOPAlgo_Builder 分割 T 型交叉 (Split T-intersections)
    // ----------------------------------------------------------------
    BOPAlgo_Builder builder;
    TopTools_MapOfShape vertexMap; // 用于去重顶点

    for (TopTools_ListIteratorOfListOfShape it(edgeList); it.More(); it.Next()) {
        const TopoDS_Shape& edge = it.Value();
        builder.AddArgument(edge); // 加入边

        // 提取边的所有拓扑顶点，强制作为参数送入布尔引擎
        // 这将迫使引擎用 T型交叉的端点去打断被触碰的长边，建立真正的拓扑共享顶点
        for (TopExp_Explorer vExp(edge, TopAbs_VERTEX); vExp.More(); vExp.Next()) {
            const TopoDS_Shape& vertex = vExp.Current();
            if (vertexMap.Add(vertex)) { // 避免重复添加同一个顶点
                builder.AddArgument(vertex);
            }
        }
    }

    builder.Perform();

    if (builder.HasErrors()) return;

    // 收集所有分割后的边 (Collect all split edges)
    TopTools_ListOfShape splitEdges;
    TopoDS_Shape splitShape = builder.Shape();
    for (TopExp_Explorer exp(splitShape, TopAbs_EDGE); exp.More(); exp.Next()) {
        splitEdges.Append(exp.Current());
    }
    if (splitEdges.IsEmpty()) return;

    TopTools_MapOfShape usedSplitEdges;

    // 辅助函数：收集生成的面并精准过滤背景面 (Helper function to collect faces and filter background faces)
    auto collectFaces = [&](BOPAlgo_BuilderFace& fb) {
        const TopTools_ListOfShape& areas = fb.Areas();
        for (TopTools_ListIteratorOfListOfShape it(areas); it.More(); it.Next()) {
            TopoDS_Face face = TopoDS::Face(it.Value());

            // 核心修复：使用外环边界的方向 (Orientation) 来精准判断是否为无限大背景面 (Background Face)
            bool isBackgroundFace = true;
            for (TopExp_Explorer wireExp(face, TopAbs_WIRE); wireExp.More(); wireExp.Next()) {
                if (wireExp.Current().Orientation() == TopAbs_FORWARD) {
                    isBackgroundFace = false;
                    break;
                }
            }
            if (isBackgroundFace) continue;

            m_profiles.push_back(std::make_shared<SketchProfile>(face));
            for (TopExp_Explorer exp(face, TopAbs_EDGE); exp.More(); exp.Next()) {
                usedSplitEdges.Add(exp.Current());
            }
        }
        };

    // PASS 1 正向建面 
    BOPAlgo_BuilderFace fb1;
    fb1.SetShapes(splitEdges);
    gp_Pln pln1(cs); // 明确实例化基准面 
    BRepBuilderAPI_MakeFace mf1(pln1);
    if (mf1.IsDone()) fb1.SetFace(mf1.Face());

    // PASS 2 反向建面：捕捉被当作孔的顺时针面 
    BOPAlgo_BuilderFace fb2;
    fb2.SetShapes(splitEdges);
    gp_Pln pln2(cs.Location(), cs.Direction().Reversed()); // 明确实例化反向面 
    BRepBuilderAPI_MakeFace mf2(pln2);
    if (mf2.IsDone()) fb2.SetFace(mf2.Face());
    fb2.Perform();
    if (!fb2.HasErrors()) {
        const TopTools_ListOfShape& areas2 = fb2.Areas();
        for (TopTools_ListIteratorOfListOfShape it(areas2); it.More(); it.Next()) {
            TopoDS_Face face = TopoDS::Face(it.Value());

            bool isBackgroundFace = true;
            for (TopExp_Explorer wireExp(face, TopAbs_WIRE); wireExp.More(); wireExp.Next()) {
                if (wireExp.Current().Orientation() == TopAbs_FORWARD) {
                    isBackgroundFace = false;
                    break;
                }
            }
            if (isBackgroundFace) continue;

            TopExp_Explorer wireExp(face, TopAbs_WIRE);
            if (!wireExp.More()) continue;
            TopoDS_Wire wire = TopoDS::Wire(wireExp.Current());
            wire.Reverse();

            BRepBuilderAPI_MakeFace realFace(gp_Pln(cs), wire);
            if (!realFace.IsDone()) continue;

            m_profiles.push_back(std::make_shared<SketchProfile>(realFace.Face()));
            for (TopExp_Explorer exp(wire, TopAbs_EDGE); exp.More(); exp.Next()) {
                usedSplitEdges.Add(exp.Current());
            }
        }
    }

    // 处理真正未被 BuilderFace 覆盖的 splitEdges
    Handle(TopTools_HSequenceOfShape) remainingEdges = new TopTools_HSequenceOfShape();
    for (TopTools_ListIteratorOfListOfShape it(splitEdges); it.More(); it.Next()) {
        if (!usedSplitEdges.Contains(it.Value())) {
            remainingEdges->Append(it.Value());
        }
    }

    if (remainingEdges->Length() > 0) {
        Handle(TopTools_HSequenceOfShape) wires = new TopTools_HSequenceOfShape();
        ShapeAnalysis_FreeBounds::ConnectEdgesToWires(
            remainingEdges, 1e-5, Standard_False, wires);

        gp_Pln sketchPlane(cs);
        for (Standard_Integer i = 1; i <= wires->Length(); ++i) {
            TopoDS_Wire wire = TopoDS::Wire(wires->Value(i));
            if (!wire.Closed()) continue;

            // 严格验证端点真正首尾相接，拒绝容差粘合
            BRepCheck_Wire checker(wire);
            if (checker.Closed() != BRepCheck_NoError) continue; 

            BRepBuilderAPI_MakeFace makeFace(sketchPlane, wire, Standard_True);
            if (makeFace.IsDone()) {
                m_profiles.push_back(std::make_shared<SketchProfile>(makeFace.Face()));
            }
        }
    }
    std::ofstream logFile("E:\\sketch_log.txt", std::ios::app);
    if (logFile.is_open()) {
        logFile << "=======================================" << std::endl;
        logFile << "Total Profiles generated: " << m_profiles.size() << std::endl;
        logFile << "=======================================" << std::endl;
        logFile.close();
    }
}


} // namespace cad_sketch
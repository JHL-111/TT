#include "cad_sketch/Sketch.h"
#include "cad_sketch/SketchLine.h"
#include "cad_sketch/SketchCircle.h"
#include "cad_sketch/SketchArc.h"
#include "cad_sketch/SketchCurve.h" 

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

    TopTools_MapOfShape usedEdges; // 记录被成功建面的边

    // 广义布尔引擎,处理T型
    BOPAlgo_Builder builder;
    for (TopTools_ListIteratorOfListOfShape it(edgeList); it.More(); it.Next()) {
        builder.AddArgument(it.Value());
    }
    builder.SetFuzzyValue(1e-5);
    builder.Perform();

    if (!builder.HasErrors()) {
        TopTools_ListOfShape splitEdges;
        TopoDS_Shape splitShape = builder.Shape();
        for (TopExp_Explorer exp(splitShape, TopAbs_EDGE); exp.More(); exp.Next()) {
            splitEdges.Append(exp.Current());
        }

        if (!splitEdges.IsEmpty()) {
            // -------------------------------------------------------------
            // 第一遍 原始法向推导 (捕捉所有逆时针实体面)
            // -------------------------------------------------------------
            BOPAlgo_BuilderFace faceBuilder1;
            faceBuilder1.SetShapes(splitEdges);

            gp_Pln basePln(cs);
            BRepBuilderAPI_MakeFace baseFaceMaker1(basePln);
            if (baseFaceMaker1.IsDone()) {
                faceBuilder1.SetFace(baseFaceMaker1.Face());
            }
            faceBuilder1.Perform();

            if (!faceBuilder1.HasErrors()) {
                const TopTools_ListOfShape& areas1 = faceBuilder1.Areas();
                for (TopTools_ListIteratorOfListOfShape it(areas1); it.More(); it.Next()) {
                    TopoDS_Face face = TopoDS::Face(it.Value());

                    Standard_Real uMin, uMax, vMin, vMax;
                    BRepTools::UVBounds(face, uMin, uMax, vMin, vMax);
                    if (Precision::IsInfinite(uMin) || Precision::IsInfinite(uMax) ||
                        Precision::IsInfinite(vMin) || Precision::IsInfinite(vMax)) {
                        continue; // 丢弃宇宙背景面
                    }

                    m_profiles.push_back(std::make_shared<SketchProfile>(face));
                    for (TopExp_Explorer exp(face, TopAbs_EDGE); exp.More(); exp.Next()) {
                        usedEdges.Add(exp.Current());
                    }
                }
            }

            // -------------------------------------------------------------
            // 第二遍 反转法向推导 (捕捉被遗漏的顺时针实体面，被考虑成孔的那部分）
            // -------------------------------------------------------------
            BOPAlgo_BuilderFace faceBuilder2;
            faceBuilder2.SetShapes(splitEdges);

            // 构建反向坐标系
            gp_Dir reversedDir = cs.Direction().Reversed();
            gp_Pln reversedPln(cs.Location(), reversedDir);

            BRepBuilderAPI_MakeFace baseFaceMaker2(reversedPln);
            if (baseFaceMaker2.IsDone()) {
                faceBuilder2.SetFace(baseFaceMaker2.Face());
            }
            faceBuilder2.Perform();

            if (!faceBuilder2.HasErrors()) {
                const TopTools_ListOfShape& areas2 = faceBuilder2.Areas();
                for (TopTools_ListIteratorOfListOfShape it(areas2); it.More(); it.Next()) {
                    TopoDS_Face face = TopoDS::Face(it.Value());

                    Standard_Real uMin, uMax, vMin, vMax;
                    BRepTools::UVBounds(face, uMin, uMax, vMin, vMax);
                    if (Precision::IsInfinite(uMin) || Precision::IsInfinite(uMax) ||
                        Precision::IsInfinite(vMin) || Precision::IsInfinite(vMax)) {
                        continue;
                    }

                    // 1. 提取出这个面外围的闭合线框 (Wire)
                    TopExp_Explorer wireExp(face, TopAbs_WIRE);
                    if (wireExp.More()) {
                        TopoDS_Wire wire = TopoDS::Wire(wireExp.Current());

                        // 2. 将线框的环绕方向反转（把里世界的顺时针变成表世界的逆时针）
                        wire.Reverse();

                        // 3. 使用【原始的正向基准面 basePln】和【反转后的线框】，重新浇筑一个全新的物理面
                        gp_Pln originalBasePln(cs);
                        BRepBuilderAPI_MakeFace realFaceMaker(originalBasePln, wire);

                        if (realFaceMaker.IsDone()) {
                            // 现在这个面，无论是拓扑还是底层几何，都和 PASS 1 一模一样了！
                            m_profiles.push_back(std::make_shared<SketchProfile>(realFaceMaker.Face()));

                            // 标记使用过的边
                            for (TopExp_Explorer exp(wire, TopAbs_EDGE); exp.More(); exp.Next()) {
                                usedEdges.Add(exp.Current());
                            }
                        }
                    }
                }
            }
        }
    }

	// 处理无需要布尔分割的边
    Handle(TopTools_HSequenceOfShape) remainingEdges = new TopTools_HSequenceOfShape();
    for (TopTools_ListIteratorOfListOfShape it(edgeList); it.More(); it.Next()) {
        if (!usedEdges.Contains(it.Value())) {
            remainingEdges->Append(it.Value());
        }
    }

    if (remainingEdges->Length() > 0) {
        Handle(TopTools_HSequenceOfShape) wires = new TopTools_HSequenceOfShape();
        ShapeAnalysis_FreeBounds::ConnectEdgesToWires(remainingEdges, 1e-5, Standard_False, wires);

        gp_Pln sketchPlane(cs);
        for (Standard_Integer i = 1; i <= wires->Length(); ++i) {
            TopoDS_Wire wire = TopoDS::Wire(wires->Value(i));
            if (wire.Closed()) {
                BRepBuilderAPI_MakeFace makeFace(sketchPlane, wire, Standard_True);
                if (makeFace.IsDone()) {
                    m_profiles.push_back(std::make_shared<SketchProfile>(makeFace.Face()));
                }
            }
        }
    }
}


} // namespace cad_sketch
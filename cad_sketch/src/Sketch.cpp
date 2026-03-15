#include "cad_sketch/Sketch.h"
#include "cad_sketch/SketchLine.h"
#include "cad_sketch/SketchCircle.h"
#include "cad_sketch/SketchArc.h"

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
#include <gp_Circ.hxx>
#include <gp_Pln.hxx>
#include <algorithm>
#include <TopTools_HSequenceOfShape.hxx>
#include <ShapeAnalysis_FreeBounds.hxx>


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

TopoDS_Wire Sketch::GetProfileWire(const gp_Ax3& cs) const {
    BRepBuilderAPI_MakeWire wireMaker;
    bool hasEdges = false;

    // 获取草图基准面的原点和 X/Y 轴向量
    gp_Pnt origin = cs.Location();
    gp_Dir xDir = cs.XDirection();
    gp_Dir yDir = cs.YDirection();

    // 辅助 Lambda：将 2D 草图坐标转换为基于基准面的真实 3D 坐标
    auto LocalToWorld = [&](double x, double y) -> gp_Pnt {
        return origin.Translated(gp_Vec(xDir) * x + gp_Vec(yDir) * y);
        };

    for (const auto& element : m_elements) {
        if (element->GetType() == SketchElementType::Line) {
            auto line = std::dynamic_pointer_cast<SketchLine>(element);
            if (line && line->GetStartPoint() && line->GetEndPoint()) {
                gp_Pnt p1 = LocalToWorld(line->GetStartPoint()->GetX(), line->GetStartPoint()->GetY());
                gp_Pnt p2 = LocalToWorld(line->GetEndPoint()->GetX(), line->GetEndPoint()->GetY());
                if (!p1.IsEqual(p2, Precision::Confusion())) {
                    wireMaker.Add(BRepBuilderAPI_MakeEdge(p1, p2));
                    hasEdges = true;
                }
            }
        }
        else if (element->GetType() == SketchElementType::Circle) {
            auto circle = std::dynamic_pointer_cast<SketchCircle>(element);
            if (circle && circle->GetCenter()) {
                gp_Pnt center = LocalToWorld(circle->GetCenter()->GetX(), circle->GetCenter()->GetY());
                // 在斜面上生成圆：法线为 cs.Direction()
                gp_Ax2 ax2(center, cs.Direction(), cs.XDirection());
                wireMaker.Add(BRepBuilderAPI_MakeEdge(gp_Circ(ax2, circle->GetRadius())));
                hasEdges = true;
            }
        }
        else if (element->GetType() == SketchElementType::Arc) {
            auto arc = std::dynamic_pointer_cast<SketchArc>(element);
            if (arc && arc->GetCenter()) {
                gp_Pnt center = LocalToWorld(arc->GetCenter()->GetX(), arc->GetCenter()->GetY());
                gp_Ax2 ax2(center, cs.Direction(), cs.XDirection());
                wireMaker.Add(BRepBuilderAPI_MakeEdge(gp_Circ(ax2, arc->GetRadius()), arc->GetStartAngle(), arc->GetEndAngle()));
                hasEdges = true;
            }
        }
    }

    if (!hasEdges || !wireMaker.IsDone()) return TopoDS_Wire();
    return wireMaker.Wire();
}

TopoDS_Face Sketch::GetProfileFace(const gp_Ax3& cs) const {
    TopoDS_Wire wire = GetProfileWire(cs);
    if (wire.IsNull() || !wire.Closed()) return TopoDS_Face();

    // 强制根据真实 3D 空间内的线框生成面
    BRepBuilderAPI_MakeFace faceMaker(wire, true);
    if (faceMaker.IsDone()) return faceMaker.Face();
    return TopoDS_Face();
}

void Sketch::UpdateProfiles(const gp_Ax3& cs) {
    m_profiles.clear();
    if (m_elements.empty()) return;

    // 1. 收集当前草图里所有的边 (Edges)
    Handle(TopTools_HSequenceOfShape) edges = new TopTools_HSequenceOfShape();
    for (const auto& elem : m_elements) {
        TopoDS_Shape shape = CreateEdgeFromElement(elem, cs);
        if (!shape.IsNull() && shape.ShapeType() == TopAbs_EDGE) {
            edges->Append(shape);
        }
    }

    if (edges->IsEmpty()) return;

    // 2. 自动缝合乱序的边生成线框 (Wires)
    Handle(TopTools_HSequenceOfShape) wires = new TopTools_HSequenceOfShape();
    double tolerance = 1e-5;
    // 使用 ShapeAnalysis_FreeBounds 自动处理相连关系
    ShapeAnalysis_FreeBounds::ConnectEdgesToWires(edges, tolerance, Standard_False, wires);

    // 3. 构建当前草图的平面基准 (Planar surface)
    gp_Pln sketchPlane(cs.Location(), cs.Direction());

    // 4. 检查生成的线框是否闭合，并转换为面 (Faces)
    for (int i = 1; i <= wires->Length(); ++i) {
        TopoDS_Wire wire = TopoDS::Wire(wires->Value(i));

        if (wire.Closed()) {
            // 使用线框和草图平面生成面
            BRepBuilderAPI_MakeFace faceMaker(sketchPlane, wire);
            if (faceMaker.IsDone()) {
                m_profiles.push_back(std::make_shared<SketchProfile>(faceMaker.Face()));
            }
        }
    }
}

} // namespace cad_sketch
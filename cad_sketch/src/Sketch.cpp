#include "cad_sketch/Sketch.h"
#include "cad_sketch/SketchLine.h"
#include "cad_sketch/SketchCircle.h"
#include "cad_sketch/SketchArc.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <algorithm>

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

} // namespace cad_sketch
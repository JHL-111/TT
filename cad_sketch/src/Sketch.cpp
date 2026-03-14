#include "cad_sketch/Sketch.h"
#include "cad_sketch/SketchLine.h"
#include "cad_sketch/SketchCircle.h"
#include "cad_sketch/SketchArc.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <TopoDS_Wire.hxx>
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

TopoDS_Wire Sketch::GetProfileWire() const {
    BRepBuilderAPI_MakeWire wireMaker;
    bool hasEdges = false;

    // 遍历草图中的所有元素 (Iterate through all sketch elements)
    for (const auto& element : m_elements) {

        // 处理直线 (Handle Lines)
        if (element->GetType() == SketchElementType::Line) {
            auto line = std::dynamic_pointer_cast<SketchLine>(element);
            if (line && line->GetStartPoint() && line->GetEndPoint()) {
                // 将 2D 坐标转换为 3D 空间中 XOY 平面上的点 (Z = 0)
                gp_Pnt p1(line->GetStartPoint()->GetX(), line->GetStartPoint()->GetY(), 0.0);
                gp_Pnt p2(line->GetEndPoint()->GetX(), line->GetEndPoint()->GetY(), 0.0);

                // 确保两个点不重合 (Ensure points are not identical)
                if (!p1.IsEqual(p2, 1e-6)) {
                    TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(p1, p2);
                    wireMaker.Add(edge);
                    hasEdges = true;
                }
            }
        }
        // 处理完整圆 (Handle Circles)
        else if (element->GetType() == SketchElementType::Circle) {
            auto circle = std::dynamic_pointer_cast<SketchCircle>(element);
            if (circle && circle->GetCenter()) {
                gp_Pnt center(circle->GetCenter()->GetX(), circle->GetCenter()->GetY(), 0.0);
                // 定义一个位于 XOY 平面的局部坐标系 (Z轴为法线)
                gp_Ax2 ax2(center, gp_Dir(0, 0, 1));
                gp_Circ gpCirc(ax2, circle->GetRadius());

                TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(gpCirc);
                wireMaker.Add(edge);
                hasEdges = true;
            }
        }
        // 处理圆弧 (Handle Arcs)
        else if (element->GetType() == SketchElementType::Arc) {
            auto arc = std::dynamic_pointer_cast<SketchArc>(element);
            if (arc && arc->GetCenter()) {
                gp_Pnt center(arc->GetCenter()->GetX(), arc->GetCenter()->GetY(), 0.0);
                gp_Ax2 ax2(center, gp_Dir(0, 0, 1));
                gp_Circ gpCirc(ax2, arc->GetRadius());

                // OCCT 接收的弧度通常是从 X 轴正向逆时针计算的
                TopoDS_Edge edge = BRepBuilderAPI_MakeEdge(gpCirc, arc->GetStartAngle(), arc->GetEndAngle());
                wireMaker.Add(edge);
                hasEdges = true;
            }
        }
    }

    // 如果没有添加任何边，或者线框构造失败，返回一个空的 Wire
    if (!hasEdges || !wireMaker.IsDone()) {
        return TopoDS_Wire();
    }

    // 返回构造好的拓扑线框 (Return the constructed topological wire)
    return wireMaker.Wire();
}


} // namespace cad_sketch
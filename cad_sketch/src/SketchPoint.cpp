#include "cad_sketch/SketchPoint.h"
#include <sstream>
#include <gp_XYZ.hxx>

namespace cad_sketch {

SketchPoint::SketchPoint() : SketchElement(SketchElementType::Point) {
}

SketchPoint::SketchPoint(double x, double y) 
    : SketchElement(SketchElementType::Point), m_point(x, y, 0) {
}

SketchPoint::SketchPoint(const cad_core::Point& point)
    : SketchElement(SketchElementType::Point), m_point(point) {
}

const cad_core::Point& SketchPoint::GetPoint() const {
    return m_point;
}

void SketchPoint::SetPoint(const cad_core::Point& point) {
    m_point = point;
}

double SketchPoint::GetX() const {
    return m_point.X();
}

double SketchPoint::GetY() const {
    return m_point.Y();
}

void SketchPoint::SetX(double x) {
    m_point.SetX(x);
}

void SketchPoint::SetY(double y) {
    m_point.SetY(y);
}

void SketchPoint::SetXY(double x, double y) {
    m_point.SetXYZ(x, y, 0);
}

void SketchPoint::Translate(double dx, double dy) {
    m_point.SetX(m_point.X() + dx);
    m_point.SetY(m_point.Y() + dy);
}

void SketchPoint::Rotate(double cx, double cy, double angleRad) {
    // 使用 GetX() 和 GetY() 获取当前坐标
    double px = GetX();
    double py = GetY();

    // 计算旋转后的新坐标
    double nx = (px - cx) * std::cos(angleRad) - (py - cy) * std::sin(angleRad) + cx;
    double ny = (px - cx) * std::sin(angleRad) + (py - cy) * std::cos(angleRad) + cy;

    // 使用 SetX() 和 SetY() 写入新坐标 (如果你的类里是叫 SetX/SetY)
    SetX(nx);
    SetY(ny);
    // 注意：如果你的点类里没有 SetX/SetY，而是叫 SetCoordinate(nx, ny)，请替换为对应的方法
}

std::string SketchPoint::GetDescription() const {
    std::ostringstream oss;
    oss << "Point (" << GetX() << ", " << GetY() << ")";
    return oss.str();
}

} // namespace cad_sketch
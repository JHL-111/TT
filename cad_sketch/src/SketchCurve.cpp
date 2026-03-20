// cad_sketch/src/SketchCurve.cpp
#include "cad_sketch/SketchCurve.h"

namespace cad_sketch {

    SketchCurve::SketchCurve() : SketchElement(SketchElementType::Curve) {
    }

    void SketchCurve::Translate(double dx, double dy) {
        // 遍历平移所有的控制点 (Translate all control points)
        for (auto& pt : m_controlPoints) {
            pt->Translate(dx, dy);
        }
    }

    void SketchCurve::Rotate(double cx, double cy, double angleRad) {
        // 遍历旋转所有的控制点 (Rotate all control points)
        for (auto& pt : m_controlPoints) {
            pt->Rotate(cx, cy, angleRad);
        }
    }

    void SketchCurve::AddControlPoint(const SketchPointPtr& point) {
        if (point) {
            m_controlPoints.push_back(point);
        }
    }

    const std::vector<SketchPointPtr>& SketchCurve::GetControlPoints() const {
        return m_controlPoints;
    }

    std::string SketchCurve::GetDescription() const {
        return "SketchCurve (Points: " + std::to_string(m_controlPoints.size()) + ")";
    }

} // namespace cad_sketch
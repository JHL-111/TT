#include "cad_sketch/SketchArc.h"
#include <cmath>
#include <sstream>

namespace cad_sketch {

    SketchArc::SketchArc()
        : SketchElement(SketchElementType::Arc), m_radius(1.0), m_startAngle(0.0), m_endAngle(M_PI) {
        m_center = std::make_shared<SketchPoint>();
        m_startPoint = std::make_shared<SketchPoint>(); 
        m_endPoint = std::make_shared<SketchPoint>();  
        UpdateEndpoints(); 
    }

    SketchArc::SketchArc(const SketchPointPtr& center, double radius, double startAngle, double endAngle)
        : SketchElement(SketchElementType::Arc), m_center(center), m_radius(radius),
        m_startAngle(startAngle), m_endAngle(endAngle) {
        m_startPoint = std::make_shared<SketchPoint>();
        m_endPoint = std::make_shared<SketchPoint>();
        UpdateEndpoints();
    }

    SketchArc::SketchArc(double centerX, double centerY, double radius, double startAngle, double endAngle)
        : SketchElement(SketchElementType::Arc), m_radius(radius),
        m_startAngle(startAngle), m_endAngle(endAngle) {
        m_center = std::make_shared<SketchPoint>(centerX, centerY);
        m_startPoint = std::make_shared<SketchPoint>();
        m_endPoint = std::make_shared<SketchPoint>();
        UpdateEndpoints();
    }

    const SketchPointPtr& SketchArc::GetCenter() const {
        return m_center;
    }

    void SketchArc::SetCenter(const SketchPointPtr& center) {
        m_center = center;
        UpdateEndpoints();
    }

    double SketchArc::GetRadius() const {
        return m_radius;
    }

    void SketchArc::SetRadius(double radius) {
        m_radius = radius;
        UpdateEndpoints();
    }

    double SketchArc::GetStartAngle() const {
        return m_startAngle;
    }

    void SketchArc::SetStartAngle(double angle) {
        m_startAngle = angle;
		UpdateEndpoints();
    }

    double SketchArc::GetEndAngle() const {
        return m_endAngle;
    }

    void SketchArc::SetEndAngle(double angle) {
        m_endAngle = angle;
		UpdateEndpoints();
    }

    double SketchArc::GetSweepAngle() const {
        double sweep = m_endAngle - m_startAngle;
        while (sweep < 0) sweep += 2 * M_PI;
        while (sweep > 2 * M_PI) sweep -= 2 * M_PI;
        return sweep;
    }

    double SketchArc::GetLength() const {
        return m_radius * GetSweepAngle();
    }

    const SketchPointPtr& SketchArc::GetStartPoint() const {
        return m_startPoint; 
    }

    const SketchPointPtr& SketchArc::GetEndPoint() const {
        return m_endPoint;   
    }

    void SketchArc::Translate(double dx, double dy) {
        if (m_center) m_center->Translate(dx, dy);
        if (m_startPoint) m_startPoint->Translate(dx, dy); 
        if (m_endPoint) m_endPoint->Translate(dx, dy);     
    }

    void SketchArc::Rotate(double cx, double cy, double angleRad) {
        if (m_center) m_center->Rotate(cx, cy, angleRad);
        if (m_startPoint) m_startPoint->Rotate(cx, cy, angleRad); 
        if (m_endPoint) m_endPoint->Rotate(cx, cy, angleRad);     

        m_startAngle += angleRad;
        m_endAngle += angleRad;

        // Normalise angles to [0, 2π]
        if (m_startAngle < 0)          m_startAngle += 2 * M_PI;
        if (m_startAngle >= 2 * M_PI)  m_startAngle -= 2 * M_PI;
        if (m_endAngle < 0)            m_endAngle += 2 * M_PI;
        if (m_endAngle >= 2 * M_PI)    m_endAngle -= 2 * M_PI;
    }

    std::string SketchArc::GetDescription() const {
        std::ostringstream oss;
        oss << "Arc (Radius: " << m_radius << ", Sweep: " << GetSweepAngle() * 180.0 / M_PI << "°)";
        return oss.str();
    }

    void SketchArc::UpdateEndpoints() {
        if (!m_center || !m_startPoint || !m_endPoint) return;

        double sx = m_center->GetX() + m_radius * std::cos(m_startAngle);
        double sy = m_center->GetY() + m_radius * std::sin(m_startAngle);
        double ex = m_center->GetX() + m_radius * std::cos(m_endAngle);
        double ey = m_center->GetY() + m_radius * std::sin(m_endAngle);

        m_startPoint->Translate(sx - m_startPoint->GetX(), sy - m_startPoint->GetY());
        m_endPoint->Translate(ex - m_endPoint->GetX(), ey - m_endPoint->GetY());
    }

} // namespace cad_sketch
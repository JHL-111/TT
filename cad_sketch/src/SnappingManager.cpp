#include "cad_sketch/SnappingManager.h"
#include "cad_sketch/SketchLine.h"
#include "cad_sketch/SketchCircle.h"
#include "cad_sketch/SketchArc.h"
#include <cmath>
#include <algorithm>

namespace cad_sketch {

SnappingManager::SnappingManager() : m_snapTolerance(0.1), m_gridSize(0.5) {
    // 默认启用常见的捕捉类型
    m_enabledSnapTypes.push_back(SnapType::Endpoint);
    m_enabledSnapTypes.push_back(SnapType::Midpoint);
    m_enabledSnapTypes.push_back(SnapType::Center);
    m_enabledSnapTypes.push_back(SnapType::Nearest);
    m_enabledSnapTypes.push_back(SnapType::Grid);
}

void SnappingManager::SetSnapTolerance(double tolerance) {
    m_snapTolerance = tolerance;
}

double SnappingManager::GetSnapTolerance() const {
    return m_snapTolerance;
}

void SnappingManager::EnableSnapType(SnapType type) {
    if (!IsSnapTypeEnabled(type)) {
        m_enabledSnapTypes.push_back(type);
    }
}

void SnappingManager::DisableSnapType(SnapType type) {
    auto it = std::find(m_enabledSnapTypes.begin(), m_enabledSnapTypes.end(), type);
    if (it != m_enabledSnapTypes.end()) {
        m_enabledSnapTypes.erase(it);
    }
}

bool SnappingManager::IsSnapTypeEnabled(SnapType type) const {
    return std::find(m_enabledSnapTypes.begin(), m_enabledSnapTypes.end(), type) != m_enabledSnapTypes.end();
}

void SnappingManager::SetGridSize(double gridSize) {
    m_gridSize = gridSize;
}

double SnappingManager::GetGridSize() const {
    return m_gridSize;
}

SnapResult SnappingManager::FindSnapPoint(const cad_core::Point& inputPoint,
    const std::vector<SketchElementPtr>& elements) const {
    SnapResult bestResult;

    // 1. 优先尝试端点捕捉 (最高优先级)
    if (IsSnapTypeEnabled(SnapType::Endpoint)) {
        SnapResult endpointResult = SnapToEndpoints(inputPoint, elements);
        if (endpointResult.found) {
            return endpointResult; // 只要找到了端点，直接返回，不再被其他元素干扰
        }
    }

    // 2. 尝试中心点捕捉
    if (IsSnapTypeEnabled(SnapType::Center)) {
        SnapResult centerResult = SnapToCenters(inputPoint, elements);
        if (centerResult.found) {
            return centerResult;
        }
    }

    // 3. 尝试中点捕捉
    if (IsSnapTypeEnabled(SnapType::Midpoint)) {
        SnapResult midpointResult = SnapToMidpoints(inputPoint, elements);
        if (midpointResult.found) {
            return midpointResult;
        }
    }

    // 4. 尝试边缘/最近点捕捉 (比如连到线段中间)
    if (IsSnapTypeEnabled(SnapType::Nearest)) {
        SnapResult nearestResult = SnapToNearest(inputPoint, elements);
        if (nearestResult.found) {
            return nearestResult;
        }
    }

    // 5. 只有在没有任何几何图形捕捉到的情况下，才去捕捉网格！(最低优先级兜底)
    if (IsSnapTypeEnabled(SnapType::Grid)) {
        SnapResult gridResult = SnapToGrid(inputPoint);
        if (gridResult.found) {
            return gridResult;
        }
    }

    return bestResult;
}
SnapResult SnappingManager::SnapToGrid(const cad_core::Point& inputPoint) const {
    SnapResult result;
    
    double x = std::round(inputPoint.X() / m_gridSize) * m_gridSize;
    double y = std::round(inputPoint.Y() / m_gridSize) * m_gridSize;
    
    cad_core::Point snapPoint(x, y, 0);
    
    if (IsWithinTolerance(inputPoint, snapPoint)) {
        result.found = true;
        result.type = SnapType::Grid;
        result.snapPoint = snapPoint;
    }
    
    return result;
}

SnapResult SnappingManager::SnapToEndpoints(const cad_core::Point& inputPoint, 
                                          const std::vector<SketchElementPtr>& elements) const {
    SnapResult result;
    
    for (const auto& element : elements) {
        if (element->GetType() == SketchElementType::Point) {
        auto point = std::dynamic_pointer_cast<SketchPoint>(element);
            if (point) {
                // 判断鼠标是否在点的捕捉容差范围内
                if (IsWithinTolerance(inputPoint, point->GetPoint())) {
                    result.found = true;
                    result.type = SnapType::Endpoint; // 复用端点的吸附图标
                    result.snapPoint = point->GetPoint();
                    result.element = element;
                    return result;
                }
            }
        }
        else if (element->GetType() == SketchElementType::Line) {
                auto line = std::dynamic_pointer_cast<SketchLine>(element);
            if (line && line->GetStartPoint() && line->GetEndPoint()) {
                if (IsWithinTolerance(inputPoint, line->GetStartPoint()->GetPoint())) {
                    result.found = true;
                    result.type = SnapType::Endpoint;
                    result.snapPoint = line->GetStartPoint()->GetPoint();
                    result.element = element;
                    return result;
                }
                if (IsWithinTolerance(inputPoint, line->GetEndPoint()->GetPoint())) {
                    result.found = true;
                    result.type = SnapType::Endpoint;
                    result.snapPoint = line->GetEndPoint()->GetPoint();
                    result.element = element;
                    return result;
                }
            }
        }
        else if (element->GetType() == SketchElementType::Arc) {
            auto arc = std::dynamic_pointer_cast<SketchArc>(element);
            if (arc && arc->GetStartPoint() && arc->GetEndPoint()) {
                if (IsWithinTolerance(inputPoint, arc->GetStartPoint()->GetPoint())) {
                    result.found = true;
                    result.type = SnapType::Endpoint;
                    result.snapPoint = arc->GetStartPoint()->GetPoint();
                    result.element = element;
                    return result;
                }
                if (IsWithinTolerance(inputPoint, arc->GetEndPoint()->GetPoint())) {
                    result.found = true;
                    result.type = SnapType::Endpoint;
                    result.snapPoint = arc->GetEndPoint()->GetPoint();
                    result.element = element;
                    return result;
                }
            }
        }
    }
    
    return result;
}

SnapResult SnappingManager::SnapToMidpoints(const cad_core::Point& inputPoint, 
                                          const std::vector<SketchElementPtr>& elements) const {
    SnapResult result;
    
    for (const auto& element : elements) {
        if (element->GetType() == SketchElementType::Line) {
            auto line = std::dynamic_pointer_cast<SketchLine>(element);
            if (line && line->GetStartPoint() && line->GetEndPoint()) {
                double midX = (line->GetStartPoint()->GetX() + line->GetEndPoint()->GetX()) / 2.0;
                double midY = (line->GetStartPoint()->GetY() + line->GetEndPoint()->GetY()) / 2.0;
                cad_core::Point midPoint(midX, midY, 0);
                
                if (IsWithinTolerance(inputPoint, midPoint)) {
                    result.found = true;
                    result.type = SnapType::Midpoint;
                    result.snapPoint = midPoint;
                    result.element = element;
                    return result;
                }
            }
        }
        else if (element->GetType() == SketchElementType::Arc) {
            auto arc = std::dynamic_pointer_cast<SketchArc>(element);
            if (arc && arc->GetCenter()) {
                // 计算中点角度
                double midAngle = arc->GetStartAngle() + arc->GetSweepAngle() / 2.0;
                // 用极坐标算出真正的中点坐标
                double midX = arc->GetCenter()->GetX() + arc->GetRadius() * std::cos(midAngle);
                double midY = arc->GetCenter()->GetY() + arc->GetRadius() * std::sin(midAngle);
                cad_core::Point midPoint(midX, midY, 0);

                if (IsWithinTolerance(inputPoint, midPoint)) {
                    result.found = true;
                    result.type = SnapType::Midpoint;
                    result.snapPoint = midPoint;
                    result.element = element;
                    return result;
                }
            }
        }
    }
    
    return result;
}

SnapResult SnappingManager::SnapToCenters(const cad_core::Point& inputPoint, 
                                        const std::vector<SketchElementPtr>& elements) const {
    SnapResult result;
    
    for (const auto& element : elements) {
        if (element->GetType() == SketchElementType::Circle) {
            auto circle = std::dynamic_pointer_cast<SketchCircle>(element);
            if (circle && circle->GetCenter()) {
                if (IsWithinTolerance(inputPoint, circle->GetCenter()->GetPoint())) {
                    result.found = true;
                    result.type = SnapType::Center;
                    result.snapPoint = circle->GetCenter()->GetPoint();
                    result.element = element;
                    return result;
                }
            }
        } 
    }
    
    return result;
}

// 找到任意边线上的最近点
SnapResult SnappingManager::SnapToNearest(const cad_core::Point& inputPoint, const std::vector<SketchElementPtr>& elements) const {
    SnapResult result;
    double minDistance = m_snapTolerance;

    for (const auto& element : elements) {
        if (element->GetType() == SketchElementType::Line) {
            auto line = std::dynamic_pointer_cast<SketchLine>(element);
            if (line && line->GetStartPoint() && line->GetEndPoint()) {
                cad_core::Point closestPt;
                double dist = DistanceToLineSegment(inputPoint, line->GetStartPoint()->GetPoint(), line->GetEndPoint()->GetPoint(), closestPt);

                if (dist <= minDistance) {
                    minDistance = dist;
                    result.found = true;
                    result.type = SnapType::Nearest;
                    result.snapPoint = closestPt; // 吸附到线上的垂足
                    result.element = element;
                }
            }
        }
        else if (element->GetType() == SketchElementType::Arc) {
            auto arc = std::dynamic_pointer_cast<SketchArc>(element);
            if (arc && arc->GetCenter()) {
                cad_core::Point closestPt;
                double dist = DistanceToArc(inputPoint, arc->GetCenter()->GetPoint(), arc->GetRadius(), arc->GetStartAngle(), arc->GetSweepAngle(), closestPt);

                if (dist <= minDistance) {
                    minDistance = dist;
                    result.found = true;
                    result.type = SnapType::Nearest;
                    result.snapPoint = closestPt; // 吸附到圆弧上的绝对点
                    result.element = element;
                }
            }
        }
        else if (element->GetType() == SketchElementType::Circle) {
            auto circle = std::dynamic_pointer_cast<SketchCircle>(element);
            if (circle && circle->GetCenter()) {
                cad_core::Point closestPt;
                // 计算鼠标到圆边缘的最近点
                double dist = DistanceToCircle(inputPoint, circle->GetCenter()->GetPoint(), circle->GetRadius(), closestPt);

                if (dist <= minDistance) {
                    minDistance = dist;
                    result.found = true;
                    result.type = SnapType::Nearest;
                    result.snapPoint = closestPt; // 吸附到圆周上的绝对点
                    result.element = element;
                }
            }
        }
    }
    return result;
}

// Calculation of geometric distance from a point to a line segment (vector projection method)
double SnappingManager::DistanceToLineSegment(const cad_core::Point& p, const cad_core::Point& a, const cad_core::Point& b, cad_core::Point& closestPoint) const {
    double l2 = a.Distance(b) * a.Distance(b); 
    if (l2 == 0.0) {
        closestPoint = a;
        return p.Distance(a);
    }

    // Projection calculation, restricted within the range of 0.0 to 1.0, 
    // ensuring that the point lies within the line segment rather than on the extension line.
    double t = std::max(0.0, std::min(1.0, ((p.X() - a.X()) * (b.X() - a.X()) + (p.Y() - a.Y()) * (b.Y() - a.Y())) / l2));

    closestPoint = cad_core::Point(a.X() + t * (b.X() - a.X()), a.Y() + t * (b.Y() - a.Y()), 0);
    return p.Distance(closestPoint);
}

// Calculation of the geometric distance from a point to a circle
double SnappingManager::DistanceToCircle(const cad_core::Point& p, const cad_core::Point& center, double radius, cad_core::Point& closestPoint) const {
    double d = p.Distance(center); // Distance from the mouse to the center of the circle
    if (d == 0.0) {
        closestPoint = cad_core::Point(center.X() + radius, center.Y(), 0);
        return radius;
    }

    // Following the line from the center of the circle to the mouse,
    // extend it outward or contract it inward, and find the absolute point on the arc.
    closestPoint = cad_core::Point(
        center.X() + radius * (p.X() - center.X()) / d,
        center.Y() + radius * (p.Y() - center.Y()) / d,
        0
    );
    return std::abs(d - radius);
}

// Calculation of the geometric distance from a point to an arc(limited within the angle range)
double SnappingManager::DistanceToArc(const cad_core::Point& p, const cad_core::Point& center, double radius, double startAngle, double sweepAngle, cad_core::Point& closestPoint) const {
    // 1. Calculate the angle of the point where the mouse is located relative to the center of the circle.
    double dx = p.X() - center.X();
    double dy = p.Y() - center.Y();
    double currentAngle = std::atan2(dy, dx);
    if (currentAngle < 0) currentAngle += 2 * M_PI;

    // 2. Standardized initial angle
    double start = startAngle;
    while (start < 0) start += 2 * M_PI;
    while (start >= 2 * M_PI) start -= 2 * M_PI;
    double end = start + sweepAngle;

    // 3. Determine whether the mouse angle is within the sweeping sector of the arc 
    // (handle the cases where it crosses the 0 degree/360 degree boundary)
    bool inSweep = false;
    if (end <= 2 * M_PI) {
        inSweep = (currentAngle >= start && currentAngle <= end);
    }
    else {
        inSweep = (currentAngle >= start || currentAngle <= (end - 2 * M_PI));
    }

    if (inSweep) {
        // If within the sector 
        // the nearest point is the same as the circumference of the circle，it is the foot of the perpendicular on the edge of the circle.
        closestPoint = cad_core::Point(
            center.X() + radius * std::cos(currentAngle),
            center.Y() + radius * std::sin(currentAngle),
            0
        );
        return p.Distance(closestPoint);
    }
    else {
        // If not within the sector
        // the nearest point must be either the starting point or the ending point.
        cad_core::Point startPt(
            center.X() + radius * std::cos(startAngle),
            center.Y() + radius * std::sin(startAngle), 0
        );
        cad_core::Point endPt(
            center.X() + radius * std::cos(startAngle + sweepAngle),
            center.Y() + radius * std::sin(startAngle + sweepAngle), 0
        );

        double distStart = p.Distance(startPt);
        double distEnd = p.Distance(endPt);

        if (distStart < distEnd) {
            closestPoint = startPt;
            return distStart;
        }
        else {
            closestPoint = endPt;
            return distEnd;
        }
    }
}

bool SnappingManager::IsWithinTolerance(const cad_core::Point& p1, const cad_core::Point& p2) const {
    return p1.Distance(p2) <= m_snapTolerance;
}

} // namespace cad_sketch
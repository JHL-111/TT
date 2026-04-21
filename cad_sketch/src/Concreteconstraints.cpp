/**
 * @file ConcreteConstraints.cpp
 * @brief Complete implementation of all 10 geometric constraints
 *
 * Each constraint class implements:
 * - GetError():        constraint violation amount in the current state
 * - GetJacobianAt():  partial derivatives of the error function w.r.t. each variable
 * - GetInvolvedPoints(): sketch points involved (used for variable registration)
 */

#include "cad_sketch/ConcreteConstraints.h"
#include <sstream>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace cad_sketch {

    // =========================================================================
    // 1. HorizontalConstraint
    //    error = p2.y - p1.y
    //    ∂e/∂p1.y = -1,  ∂e/∂p2.y = +1
    // =========================================================================

    HorizontalConstraint::HorizontalConstraint(const SketchLinePtr& line)
        : Constraint(ConstraintType::Horizontal), m_line(line) {
        if (line) AddElement(line);
    }

    bool HorizontalConstraint::IsValid() const {
        return m_line && m_line->GetStartPoint() && m_line->GetEndPoint();
    }

    std::string HorizontalConstraint::GetDescription() const {
        return "Horizontal Constraint";
    }

    double HorizontalConstraint::GetError() const {
        if (!IsValid()) return 0.0;
        return m_line->GetEndPoint()->GetY() - m_line->GetStartPoint()->GetY();
    }

    std::vector<SketchPointPtr> HorizontalConstraint::GetInvolvedPoints() const {
        if (!IsValid()) return {};
        return { m_line->GetStartPoint(), m_line->GetEndPoint() };
    }

    std::vector<JacobianEntry> HorizontalConstraint::GetJacobianAt(int eqIndex) const {
        std::vector<JacobianEntry> entries;
        if (!IsValid()) return entries;

        int idx1 = GetPointVariableIndex(m_line->GetStartPoint()); // index of p1.x
        int idx2 = GetPointVariableIndex(m_line->GetEndPoint());   // index of p2.x
        if (idx1 < 0 || idx2 < 0) return entries;

        // error = p2.y - p1.y
        // ∂e/∂p1.y = -1  (p1.y variable index = idx1 + 1)
        // ∂e/∂p2.y = +1  (p2.y variable index = idx2 + 1)
        entries.push_back({ idx1 + 1, -1.0 });
        entries.push_back({ idx2 + 1,  1.0 });
        return entries;
    }

    // =========================================================================
    // 2. VerticalConstraint
    //    error = p2.x - p1.x
    // =========================================================================

    VerticalConstraint::VerticalConstraint(const SketchLinePtr& line)
        : Constraint(ConstraintType::Vertical), m_line(line) {
        if (line) AddElement(line);
    }

    bool VerticalConstraint::IsValid() const {
        return m_line && m_line->GetStartPoint() && m_line->GetEndPoint();
    }

    std::string VerticalConstraint::GetDescription() const {
        return "Vertical Constraint";
    }

    double VerticalConstraint::GetError() const {
        if (!IsValid()) return 0.0;
        return m_line->GetEndPoint()->GetX() - m_line->GetStartPoint()->GetX();
    }

    std::vector<SketchPointPtr> VerticalConstraint::GetInvolvedPoints() const {
        if (!IsValid()) return {};
        return { m_line->GetStartPoint(), m_line->GetEndPoint() };
    }

    std::vector<JacobianEntry> VerticalConstraint::GetJacobianAt(int eqIndex) const {
        std::vector<JacobianEntry> entries;
        if (!IsValid()) return entries;

        int idx1 = GetPointVariableIndex(m_line->GetStartPoint());
        int idx2 = GetPointVariableIndex(m_line->GetEndPoint());
        if (idx1 < 0 || idx2 < 0) return entries;

        // error = p2.x - p1.x
        entries.push_back({ idx1, -1.0 }); // ∂e/∂p1.x = -1
        entries.push_back({ idx2,  1.0 }); // ∂e/∂p2.x = +1
        return entries;
    }

    // =========================================================================
    // 3. CoincidentConstraint
    //    eq0: p1.x - p2.x = 0
    //    eq1: p1.y - p2.y = 0
    // =========================================================================

    CoincidentConstraint::CoincidentConstraint(const SketchPointPtr& p1, const SketchPointPtr& p2)
        : Constraint(ConstraintType::Coincident), m_p1(p1), m_p2(p2) {
        if (p1) AddElement(p1);
        if (p2) AddElement(p2);
    }

    bool CoincidentConstraint::IsValid() const {
        return m_p1 && m_p2;
    }

    std::string CoincidentConstraint::GetDescription() const {
        return "Coincident Constraint";
    }

    double CoincidentConstraint::GetError() const {
        return GetErrorAt(0);
    }

    double CoincidentConstraint::GetErrorAt(int eqIndex) const {
        if (!IsValid()) return 0.0;
        if (eqIndex == 0) return m_p1->GetX() - m_p2->GetX();
        if (eqIndex == 1) return m_p1->GetY() - m_p2->GetY();
        return 0.0;
    }

    std::vector<SketchPointPtr> CoincidentConstraint::GetInvolvedPoints() const {
        if (!IsValid()) return {};
        return { m_p1, m_p2 };
    }

    std::vector<JacobianEntry> CoincidentConstraint::GetJacobianAt(int eqIndex) const {
        std::vector<JacobianEntry> entries;
        if (!IsValid()) return entries;

        int idx1 = GetPointVariableIndex(m_p1);
        int idx2 = GetPointVariableIndex(m_p2);
        if (idx1 < 0 || idx2 < 0) return entries;

        if (eqIndex == 0) {
            // eq0: p1.x - p2.x
            entries.push_back({ idx1,      1.0 }); // ∂/∂p1.x
            entries.push_back({ idx2,     -1.0 }); // ∂/∂p2.x
        }
        else if (eqIndex == 1) {
            // eq1: p1.y - p2.y
            entries.push_back({ idx1 + 1,  1.0 }); // ∂/∂p1.y
            entries.push_back({ idx2 + 1, -1.0 }); // ∂/∂p2.y
        }
        return entries;
    }

    // =========================================================================
    // 4. DistanceConstraint
    //    error = (p1.x-p2.x)² + (p1.y-p2.y)² - d²
    //    Squared form avoids sqrt for better numerical stability
    // =========================================================================

    DistanceConstraint::DistanceConstraint(const SketchPointPtr& p1, const SketchPointPtr& p2, double targetDistance)
        : Constraint(ConstraintType::Distance), m_p1(p1), m_p2(p2), m_targetDistance(targetDistance) {
        if (p1) AddElement(p1);
        if (p2) AddElement(p2);
    }

    bool DistanceConstraint::IsValid() const {
        return m_p1 && m_p2 && m_targetDistance > 0.0;
    }

    std::string DistanceConstraint::GetDescription() const {
        std::ostringstream oss;
        oss << "Distance Constraint (d=" << m_targetDistance << ")";
        return oss.str();
    }

    double DistanceConstraint::GetError() const {
        if (!IsValid()) return 0.0;
        double dx = m_p1->GetX() - m_p2->GetX();
        double dy = m_p1->GetY() - m_p2->GetY();
        return dx * dx + dy * dy - m_targetDistance * m_targetDistance;
    }

    std::vector<SketchPointPtr> DistanceConstraint::GetInvolvedPoints() const {
        if (!IsValid()) return {};
        return { m_p1, m_p2 };
    }

    std::vector<JacobianEntry> DistanceConstraint::GetJacobianAt(int eqIndex) const {
        std::vector<JacobianEntry> entries;
        if (!IsValid()) return entries;

        int idx1 = GetPointVariableIndex(m_p1);
        int idx2 = GetPointVariableIndex(m_p2);
        if (idx1 < 0 || idx2 < 0) return entries;

        double dx = m_p1->GetX() - m_p2->GetX();
        double dy = m_p1->GetY() - m_p2->GetY();

        // error = dx² + dy² - d²
        // ∂e/∂p1.x = 2*dx,   ∂e/∂p1.y = 2*dy
        // ∂e/∂p2.x = -2*dx,  ∂e/∂p2.y = -2*dy
        entries.push_back({ idx1,      2.0 * dx });
        entries.push_back({ idx1 + 1,  2.0 * dy });
        entries.push_back({ idx2,     -2.0 * dx });
        entries.push_back({ idx2 + 1, -2.0 * dy });
        return entries;
    }

    // =========================================================================
    // 5. ParallelConstraint
    //    error = dx1 * dy2 - dy1 * dx2  (cross product)
    //    where dx1 = a2.x-a1.x, dy1 = a2.y-a1.y
    //          dx2 = b2.x-b1.x, dy2 = b2.y-b1.y
    // =========================================================================

    ParallelConstraint::ParallelConstraint(const SketchLinePtr& line1, const SketchLinePtr& line2)
        : Constraint(ConstraintType::Parallel), m_line1(line1), m_line2(line2) {
        if (line1) AddElement(line1);
        if (line2) AddElement(line2);
    }

    bool ParallelConstraint::IsValid() const {
        return m_line1 && m_line2 &&
            m_line1->GetStartPoint() && m_line1->GetEndPoint() &&
            m_line2->GetStartPoint() && m_line2->GetEndPoint();
    }

    std::string ParallelConstraint::GetDescription() const {
        return "Parallel Constraint";
    }

    double ParallelConstraint::GetError() const {
        if (!IsValid()) return 0.0;
        double dx1 = m_line1->GetEndPoint()->GetX() - m_line1->GetStartPoint()->GetX();
        double dy1 = m_line1->GetEndPoint()->GetY() - m_line1->GetStartPoint()->GetY();
        double dx2 = m_line2->GetEndPoint()->GetX() - m_line2->GetStartPoint()->GetX();
        double dy2 = m_line2->GetEndPoint()->GetY() - m_line2->GetStartPoint()->GetY();
        return dx1 * dy2 - dy1 * dx2;
    }

    std::vector<SketchPointPtr> ParallelConstraint::GetInvolvedPoints() const {
        if (!IsValid()) return {};
        return {
            m_line1->GetStartPoint(), m_line1->GetEndPoint(),
            m_line2->GetStartPoint(), m_line2->GetEndPoint()
        };
    }

    std::vector<JacobianEntry> ParallelConstraint::GetJacobianAt(int eqIndex) const {
        std::vector<JacobianEntry> entries;
        if (!IsValid()) return entries;

        auto a1 = m_line1->GetStartPoint();
        auto a2 = m_line1->GetEndPoint();
        auto b1 = m_line2->GetStartPoint();
        auto b2 = m_line2->GetEndPoint();

        int ia1 = GetPointVariableIndex(a1);
        int ia2 = GetPointVariableIndex(a2);
        int ib1 = GetPointVariableIndex(b1);
        int ib2 = GetPointVariableIndex(b2);
        if (ia1 < 0 || ia2 < 0 || ib1 < 0 || ib2 < 0) return entries;

        double dx1 = a2->GetX() - a1->GetX();
        double dy1 = a2->GetY() - a1->GetY();
        double dx2 = b2->GetX() - b1->GetX();
        double dy2 = b2->GetY() - b1->GetY();

        // error = dx1*dy2 - dy1*dx2
        // ∂e/∂a1.x = -dy2,  ∂e/∂a1.y = dx2
        // ∂e/∂a2.x = dy2,   ∂e/∂a2.y = -dx2
        // ∂e/∂b1.x = dy1,   ∂e/∂b1.y = -dx1
        // ∂e/∂b2.x = -dy1,  ∂e/∂b2.y = dx1
        entries.push_back({ ia1,     -dy2 });
        entries.push_back({ ia1 + 1,  dx2 });
        entries.push_back({ ia2,      dy2 });
        entries.push_back({ ia2 + 1, -dx2 });
        entries.push_back({ ib1,      dy1 });
        entries.push_back({ ib1 + 1, -dx1 });
        entries.push_back({ ib2,     -dy1 });
        entries.push_back({ ib2 + 1,  dx1 });
        return entries;
    }

    // =========================================================================
    // 6. PerpendicularConstraint
    //    error = dx1*dx2 + dy1*dy2  (dot product)
    // =========================================================================

    PerpendicularConstraint::PerpendicularConstraint(const SketchLinePtr& line1, const SketchLinePtr& line2)
        : Constraint(ConstraintType::Perpendicular), m_line1(line1), m_line2(line2) {
        if (line1) AddElement(line1);
        if (line2) AddElement(line2);
    }

    bool PerpendicularConstraint::IsValid() const {
        return m_line1 && m_line2 &&
            m_line1->GetStartPoint() && m_line1->GetEndPoint() &&
            m_line2->GetStartPoint() && m_line2->GetEndPoint();
    }

    std::string PerpendicularConstraint::GetDescription() const {
        return "Perpendicular Constraint";
    }

    double PerpendicularConstraint::GetError() const {
        if (!IsValid()) return 0.0;
        double dx1 = m_line1->GetEndPoint()->GetX() - m_line1->GetStartPoint()->GetX();
        double dy1 = m_line1->GetEndPoint()->GetY() - m_line1->GetStartPoint()->GetY();
        double dx2 = m_line2->GetEndPoint()->GetX() - m_line2->GetStartPoint()->GetX();
        double dy2 = m_line2->GetEndPoint()->GetY() - m_line2->GetStartPoint()->GetY();
        return dx1 * dx2 + dy1 * dy2;
    }

    std::vector<SketchPointPtr> PerpendicularConstraint::GetInvolvedPoints() const {
        if (!IsValid()) return {};
        return {
            m_line1->GetStartPoint(), m_line1->GetEndPoint(),
            m_line2->GetStartPoint(), m_line2->GetEndPoint()
        };
    }

    std::vector<JacobianEntry> PerpendicularConstraint::GetJacobianAt(int eqIndex) const {
        std::vector<JacobianEntry> entries;
        if (!IsValid()) return entries;

        auto a1 = m_line1->GetStartPoint();
        auto a2 = m_line1->GetEndPoint();
        auto b1 = m_line2->GetStartPoint();
        auto b2 = m_line2->GetEndPoint();

        int ia1 = GetPointVariableIndex(a1);
        int ia2 = GetPointVariableIndex(a2);
        int ib1 = GetPointVariableIndex(b1);
        int ib2 = GetPointVariableIndex(b2);
        if (ia1 < 0 || ia2 < 0 || ib1 < 0 || ib2 < 0) return entries;

        double dx1 = a2->GetX() - a1->GetX();
        double dy1 = a2->GetY() - a1->GetY();
        double dx2 = b2->GetX() - b1->GetX();
        double dy2 = b2->GetY() - b1->GetY();

        // error = dx1*dx2 + dy1*dy2
        // ∂e/∂a1.x = -dx2,  ∂e/∂a1.y = -dy2
        // ∂e/∂a2.x = dx2,   ∂e/∂a2.y = dy2
        // ∂e/∂b1.x = -dx1,  ∂e/∂b1.y = -dy1
        // ∂e/∂b2.x = dx1,   ∂e/∂b2.y = dy1
        entries.push_back({ ia1,     -dx2 });
        entries.push_back({ ia1 + 1, -dy2 });
        entries.push_back({ ia2,      dx2 });
        entries.push_back({ ia2 + 1,  dy2 });
        entries.push_back({ ib1,     -dx1 });
        entries.push_back({ ib1 + 1, -dy1 });
        entries.push_back({ ib2,      dx1 });
        entries.push_back({ ib2 + 1,  dy1 });
        return entries;
    }

    // =========================================================================
    // 7. AngleConstraint
    //    Uses cross/dot product form:
    //    error = cross - dot * tan(targetAngle)
    //    where cross = dx1*dy2 - dy1*dx2,  dot = dx1*dx2 + dy1*dy2
    //    This avoids the non-differentiability of atan2.
    //    When targetAngle ≈ π/2, tan becomes very large, but that is equivalent
    //    to a Perpendicular constraint.
    // =========================================================================

    AngleConstraint::AngleConstraint(const SketchLinePtr& line1, const SketchLinePtr& line2, double targetAngleRad)
        : Constraint(ConstraintType::Angle), m_line1(line1), m_line2(line2), m_targetAngle(targetAngleRad) {
        if (line1) AddElement(line1);
        if (line2) AddElement(line2);
    }

    bool AngleConstraint::IsValid() const {
        return m_line1 && m_line2 &&
            m_line1->GetStartPoint() && m_line1->GetEndPoint() &&
            m_line2->GetStartPoint() && m_line2->GetEndPoint();
    }

    std::string AngleConstraint::GetDescription() const {
        std::ostringstream oss;
        oss << "Angle Constraint (" << (m_targetAngle * 180.0 / M_PI) << " deg)";
        return oss.str();
    }

    double AngleConstraint::GetError() const {
        if (!IsValid()) return 0.0;
        double dx1 = m_line1->GetEndPoint()->GetX() - m_line1->GetStartPoint()->GetX();
        double dy1 = m_line1->GetEndPoint()->GetY() - m_line1->GetStartPoint()->GetY();
        double dx2 = m_line2->GetEndPoint()->GetX() - m_line2->GetStartPoint()->GetX();
        double dy2 = m_line2->GetEndPoint()->GetY() - m_line2->GetStartPoint()->GetY();

        double cross = dx1 * dy2 - dy1 * dx2;
        double dot = dx1 * dx2 + dy1 * dy2;

        // error = cross - dot * tan(targetAngle)
        double tanTarget = std::tan(m_targetAngle);
        return cross - dot * tanTarget;
    }

    std::vector<SketchPointPtr> AngleConstraint::GetInvolvedPoints() const {
        if (!IsValid()) return {};
        return {
            m_line1->GetStartPoint(), m_line1->GetEndPoint(),
            m_line2->GetStartPoint(), m_line2->GetEndPoint()
        };
    }

    std::vector<JacobianEntry> AngleConstraint::GetJacobianAt(int eqIndex) const {
        std::vector<JacobianEntry> entries;
        if (!IsValid()) return entries;

        auto a1 = m_line1->GetStartPoint();
        auto a2 = m_line1->GetEndPoint();
        auto b1 = m_line2->GetStartPoint();
        auto b2 = m_line2->GetEndPoint();

        int ia1 = GetPointVariableIndex(a1);
        int ia2 = GetPointVariableIndex(a2);
        int ib1 = GetPointVariableIndex(b1);
        int ib2 = GetPointVariableIndex(b2);
        if (ia1 < 0 || ia2 < 0 || ib1 < 0 || ib2 < 0) return entries;

        double dx1 = a2->GetX() - a1->GetX();
        double dy1 = a2->GetY() - a1->GetY();
        double dx2 = b2->GetX() - b1->GetX();
        double dy2 = b2->GetY() - b1->GetY();
        double t = std::tan(m_targetAngle);

        // error = (dx1*dy2 - dy1*dx2) - t*(dx1*dx2 + dy1*dy2)
        // Partial derivatives of cross are the same as Parallel;
        // partial derivatives of dot are the same as Perpendicular, multiplied by -t.

        // ∂cross/∂a1.x = -dy2,  ∂dot/∂a1.x = -dx2  → combined: -dy2 - t*(-dx2) = -dy2 + t*dx2
        entries.push_back({ ia1,     -dy2 + t * dx2 });
        entries.push_back({ ia1 + 1,  dx2 + t * dy2 });
        entries.push_back({ ia2,      dy2 - t * dx2 });
        entries.push_back({ ia2 + 1, -dx2 - t * dy2 });
        entries.push_back({ ib1,      dy1 + t * dx1 });
        entries.push_back({ ib1 + 1, -dx1 + t * dy1 });
        entries.push_back({ ib2,     -dy1 - t * dx1 });
        entries.push_back({ ib2 + 1,  dx1 - t * dy1 });
        return entries;
    }

    // =========================================================================
    // 8. EqualLengthConstraint
    //    error = |L1|² - |L2|²
    //         = (dx1²+dy1²) - (dx2²+dy2²)
    // =========================================================================

    EqualLengthConstraint::EqualLengthConstraint(const SketchLinePtr& line1, const SketchLinePtr& line2)
        : Constraint(ConstraintType::Equal), m_line1(line1), m_line2(line2) {
        if (line1) AddElement(line1);
        if (line2) AddElement(line2);
    }

    bool EqualLengthConstraint::IsValid() const {
        return m_line1 && m_line2 &&
            m_line1->GetStartPoint() && m_line1->GetEndPoint() &&
            m_line2->GetStartPoint() && m_line2->GetEndPoint();
    }

    std::string EqualLengthConstraint::GetDescription() const {
        return "Equal Length Constraint";
    }

    double EqualLengthConstraint::GetError() const {
        if (!IsValid()) return 0.0;
        double dx1 = m_line1->GetEndPoint()->GetX() - m_line1->GetStartPoint()->GetX();
        double dy1 = m_line1->GetEndPoint()->GetY() - m_line1->GetStartPoint()->GetY();
        double dx2 = m_line2->GetEndPoint()->GetX() - m_line2->GetStartPoint()->GetX();
        double dy2 = m_line2->GetEndPoint()->GetY() - m_line2->GetStartPoint()->GetY();
        return (dx1 * dx1 + dy1 * dy1) - (dx2 * dx2 + dy2 * dy2);
    }

    std::vector<SketchPointPtr> EqualLengthConstraint::GetInvolvedPoints() const {
        if (!IsValid()) return {};
        return {
            m_line1->GetStartPoint(), m_line1->GetEndPoint(),
            m_line2->GetStartPoint(), m_line2->GetEndPoint()
        };
    }

    std::vector<JacobianEntry> EqualLengthConstraint::GetJacobianAt(int eqIndex) const {
        std::vector<JacobianEntry> entries;
        if (!IsValid()) return entries;

        auto a1 = m_line1->GetStartPoint();
        auto a2 = m_line1->GetEndPoint();
        auto b1 = m_line2->GetStartPoint();
        auto b2 = m_line2->GetEndPoint();

        int ia1 = GetPointVariableIndex(a1);
        int ia2 = GetPointVariableIndex(a2);
        int ib1 = GetPointVariableIndex(b1);
        int ib2 = GetPointVariableIndex(b2);
        if (ia1 < 0 || ia2 < 0 || ib1 < 0 || ib2 < 0) return entries;

        double dx1 = a2->GetX() - a1->GetX();
        double dy1 = a2->GetY() - a1->GetY();
        double dx2 = b2->GetX() - b1->GetX();
        double dy2 = b2->GetY() - b1->GetY();

        // error = (dx1²+dy1²) - (dx2²+dy2²)
        entries.push_back({ ia1,     -2.0 * dx1 });
        entries.push_back({ ia1 + 1, -2.0 * dy1 });
        entries.push_back({ ia2,      2.0 * dx1 });
        entries.push_back({ ia2 + 1,  2.0 * dy1 });
        entries.push_back({ ib1,      2.0 * dx2 });
        entries.push_back({ ib1 + 1,  2.0 * dy2 });
        entries.push_back({ ib2,     -2.0 * dx2 });
        entries.push_back({ ib2 + 1, -2.0 * dy2 });
        return entries;
    }

    // =========================================================================
    // 9. FixedConstraint
    //    eq0: p.x - tx = 0
    //    eq1: p.y - ty = 0
    // =========================================================================

    FixedConstraint::FixedConstraint(const SketchPointPtr& point, double targetX, double targetY)
        : Constraint(ConstraintType::Fixed), m_point(point), m_targetX(targetX), m_targetY(targetY) {
        if (point) AddElement(point);
    }

    FixedConstraint::FixedConstraint(const SketchPointPtr& point)
        : Constraint(ConstraintType::Fixed), m_point(point),
        m_targetX(point ? point->GetX() : 0.0),
        m_targetY(point ? point->GetY() : 0.0) {
        if (point) AddElement(point);
    }

    bool FixedConstraint::IsValid() const {
        return m_point != nullptr;
    }

    std::string FixedConstraint::GetDescription() const {
        std::ostringstream oss;
        oss << "Fixed Constraint (" << m_targetX << ", " << m_targetY << ")";
        return oss.str();
    }

    double FixedConstraint::GetError() const {
        return GetErrorAt(0);
    }

    double FixedConstraint::GetErrorAt(int eqIndex) const {
        if (!IsValid()) return 0.0;
        if (eqIndex == 0) return m_point->GetX() - m_targetX;
        if (eqIndex == 1) return m_point->GetY() - m_targetY;
        return 0.0;
    }

    std::vector<SketchPointPtr> FixedConstraint::GetInvolvedPoints() const {
        if (!IsValid()) return {};
        return { m_point };
    }

    std::vector<JacobianEntry> FixedConstraint::GetJacobianAt(int eqIndex) const {
        std::vector<JacobianEntry> entries;
        if (!IsValid()) return entries;

        int idx = GetPointVariableIndex(m_point);
        if (idx < 0) return entries;

        if (eqIndex == 0) {
            entries.push_back({ idx, 1.0 });      // ∂(p.x - tx)/∂p.x = 1
        }
        else if (eqIndex == 1) {
            entries.push_back({ idx + 1, 1.0 });   // ∂(p.y - ty)/∂p.y = 1
        }
        return entries;
    }

    // =========================================================================
    // 10. RadiusConstraint — applied by direct assignment, not via Newton-Raphson
    // =========================================================================

    RadiusConstraint::RadiusConstraint(const SketchCirclePtr& circle, double targetRadius)
        : Constraint(ConstraintType::Radius), m_circle(circle), m_arc(nullptr), m_targetRadius(targetRadius) {
        if (circle) AddElement(circle);
    }

    RadiusConstraint::RadiusConstraint(const SketchArcPtr& arc, double targetRadius)
        : Constraint(ConstraintType::Radius), m_circle(nullptr), m_arc(arc), m_targetRadius(targetRadius) {
        if (arc) AddElement(arc);
    }

    bool RadiusConstraint::IsValid() const {
        return (m_circle || m_arc) && m_targetRadius > 0.0;
    }

    std::string RadiusConstraint::GetDescription() const {
        std::ostringstream oss;
        oss << "Radius Constraint (r=" << m_targetRadius << ")";
        return oss.str();
    }

    double RadiusConstraint::GetError() const {
        if (m_circle) return m_circle->GetRadius() - m_targetRadius;
        if (m_arc)    return m_arc->GetRadius() - m_targetRadius;
        return 0.0;
    }

    std::vector<SketchPointPtr> RadiusConstraint::GetInvolvedPoints() const {
        // Radius constraint does not move points; return empty
        return {};
    }

    std::vector<JacobianEntry> RadiusConstraint::GetJacobianAt(int eqIndex) const {
        // Does not participate in matrix solving
        return {};
    }

    void RadiusConstraint::ApplyDirectly() {
        if (m_circle && m_targetRadius > 0.0) {
            m_circle->SetRadius(m_targetRadius);
        }
        if (m_arc && m_targetRadius > 0.0) {
            m_arc->SetRadius(m_targetRadius);
        }
    }

} // namespace cad_sketch
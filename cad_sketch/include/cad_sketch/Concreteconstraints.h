/**
 * @file ConcreteConstraints.h
 * @brief Implementations of all concrete geometric constraints
 *
 * Contains 10 constraint types:
 *   1. HorizontalConstraint    - Line is horizontal (both endpoints share the same Y)
 *   2. VerticalConstraint      - Line is vertical (both endpoints share the same X)
 *   3. CoincidentConstraint    - Two points coincide
 *   4. DistanceConstraint      - Distance between two points equals a target value
 *   5. ParallelConstraint      - Two line segments are parallel
 *   6. PerpendicularConstraint - Two line segments are perpendicular
 *   7. AngleConstraint         - Angle between two line segments equals a target value
 *   8. EqualLengthConstraint   - Two line segments have equal length
 *   9. FixedConstraint         - A point is fixed at specified coordinates
 *  10. RadiusConstraint        - Circle/arc radius equals a target value
 *
 * Each constraint implements GetError / GetJacobianAt / GetInvolvedPoints
 * for use by the Newton-Raphson solver.
 */

#pragma once

#include "Constraint.h"
#include "SketchLine.h"
#include "SketchCircle.h"
#include "SketchArc.h"
#include <cmath>

namespace cad_sketch {

    // =========================================================================
    // 1. HorizontalConstraint — line is horizontal
    //    Equation: p2.y - p1.y = 0
    // =========================================================================
    class HorizontalConstraint : public Constraint {
    public:
        HorizontalConstraint(const SketchLinePtr& line);

        bool IsValid() const override;
        std::string GetDescription() const override;
        double GetError() const override;
        int GetEquationCount() const override { return 1; }
        std::vector<SketchPointPtr> GetInvolvedPoints() const override;
        std::vector<JacobianEntry> GetJacobianAt(int eqIndex) const override;

    private:
        SketchLinePtr m_line;
    };

    // =========================================================================
    // 2. VerticalConstraint — line is vertical
    //    Equation: p2.x - p1.x = 0
    // =========================================================================
    class VerticalConstraint : public Constraint {
    public:
        VerticalConstraint(const SketchLinePtr& line);

        bool IsValid() const override;
        std::string GetDescription() const override;
        double GetError() const override;
        int GetEquationCount() const override { return 1; }
        std::vector<SketchPointPtr> GetInvolvedPoints() const override;
        std::vector<JacobianEntry> GetJacobianAt(int eqIndex) const override;

    private:
        SketchLinePtr m_line;
    };

    // =========================================================================
    // 3. CoincidentConstraint — two points coincide
    //    Equations: p1.x - p2.x = 0,  p1.y - p2.y = 0  (two equations)
    // =========================================================================
    class CoincidentConstraint : public Constraint {
    public:
        CoincidentConstraint(const SketchPointPtr& p1, const SketchPointPtr& p2);

        bool IsValid() const override;
        std::string GetDescription() const override;
        double GetError() const override;
        double GetErrorAt(int eqIndex) const override;
        int GetEquationCount() const override { return 2; }
        std::vector<SketchPointPtr> GetInvolvedPoints() const override;
        std::vector<JacobianEntry> GetJacobianAt(int eqIndex) const override;

    private:
        SketchPointPtr m_p1;
        SketchPointPtr m_p2;
    };

    // =========================================================================
    // 4. DistanceConstraint — distance between two points equals targetDistance
    //    Equation: (p1.x-p2.x)² + (p1.y-p2.y)² - d² = 0
    // =========================================================================
    class DistanceConstraint : public Constraint {
    public:
        DistanceConstraint(const SketchPointPtr& p1, const SketchPointPtr& p2, double targetDistance);

        void SetTargetDistance(double d) { m_targetDistance = d; }
        double GetTargetDistance() const { return m_targetDistance; }

        bool IsValid() const override;
        std::string GetDescription() const override;
        double GetError() const override;
        int GetEquationCount() const override { return 1; }
        std::vector<SketchPointPtr> GetInvolvedPoints() const override;
        std::vector<JacobianEntry> GetJacobianAt(int eqIndex) const override;

    private:
        SketchPointPtr m_p1;
        SketchPointPtr m_p2;
        double m_targetDistance;
    };

    // =========================================================================
    // 5. ParallelConstraint — two line segments are parallel
    //    Equation: (a2.x-a1.x)*(b2.y-b1.y) - (a2.y-a1.y)*(b2.x-b1.x) = 0
    //    i.e. the cross product of the two direction vectors is zero
    // =========================================================================
    class ParallelConstraint : public Constraint {
    public:
        ParallelConstraint(const SketchLinePtr& line1, const SketchLinePtr& line2);

        bool IsValid() const override;
        std::string GetDescription() const override;
        double GetError() const override;
        int GetEquationCount() const override { return 1; }
        std::vector<SketchPointPtr> GetInvolvedPoints() const override;
        std::vector<JacobianEntry> GetJacobianAt(int eqIndex) const override;

    private:
        SketchLinePtr m_line1;
        SketchLinePtr m_line2;
    };

    // =========================================================================
    // 6. PerpendicularConstraint — two line segments are perpendicular
    //    Equation: (a2.x-a1.x)*(b2.x-b1.x) + (a2.y-a1.y)*(b2.y-b1.y) = 0
    //    i.e. the dot product of the two direction vectors is zero
    // =========================================================================
    class PerpendicularConstraint : public Constraint {
    public:
        PerpendicularConstraint(const SketchLinePtr& line1, const SketchLinePtr& line2);

        bool IsValid() const override;
        std::string GetDescription() const override;
        double GetError() const override;
        int GetEquationCount() const override { return 1; }
        std::vector<SketchPointPtr> GetInvolvedPoints() const override;
        std::vector<JacobianEntry> GetJacobianAt(int eqIndex) const override;

    private:
        SketchLinePtr m_line1;
        SketchLinePtr m_line2;
    };

    // =========================================================================
    // 7. AngleConstraint — angle between two segments equals targetAngle (radians)
    //    Equation: atan2(cross, dot) - targetAngle = 0
    //    where cross = dx1*dy2 - dy1*dx2,  dot = dx1*dx2 + dy1*dy2
    // =========================================================================
    class AngleConstraint : public Constraint {
    public:
        AngleConstraint(const SketchLinePtr& line1, const SketchLinePtr& line2, double targetAngleRad);

        void SetTargetAngle(double angleRad) { m_targetAngle = angleRad; }
        double GetTargetAngle() const { return m_targetAngle; }

        bool IsValid() const override;
        std::string GetDescription() const override;
        double GetError() const override;
        int GetEquationCount() const override { return 1; }
        std::vector<SketchPointPtr> GetInvolvedPoints() const override;
        std::vector<JacobianEntry> GetJacobianAt(int eqIndex) const override;

    private:
        SketchLinePtr m_line1;
        SketchLinePtr m_line2;
        double m_targetAngle; // radians
    };

    // =========================================================================
    // 8. EqualLengthConstraint — two line segments have equal length
    //    Equation: |L1|² - |L2|² = 0
    // =========================================================================
    class EqualLengthConstraint : public Constraint {
    public:
        EqualLengthConstraint(const SketchLinePtr& line1, const SketchLinePtr& line2);

        bool IsValid() const override;
        std::string GetDescription() const override;
        double GetError() const override;
        int GetEquationCount() const override { return 1; }
        std::vector<SketchPointPtr> GetInvolvedPoints() const override;
        std::vector<JacobianEntry> GetJacobianAt(int eqIndex) const override;

    private:
        SketchLinePtr m_line1;
        SketchLinePtr m_line2;
    };

    // =========================================================================
    // 9. FixedConstraint — fix a point at given coordinates
    //    Equations: p.x - tx = 0,  p.y - ty = 0  (two equations)
    // =========================================================================
    class FixedConstraint : public Constraint {
    public:
        FixedConstraint(const SketchPointPtr& point, double targetX, double targetY);
        FixedConstraint(const SketchPointPtr& point); // Fix at the current position

        void SetTarget(double x, double y) { m_targetX = x; m_targetY = y; }

        bool IsValid() const override;
        std::string GetDescription() const override;
        double GetError() const override;
        double GetErrorAt(int eqIndex) const override;
        int GetEquationCount() const override { return 2; }
        std::vector<SketchPointPtr> GetInvolvedPoints() const override;
        std::vector<JacobianEntry> GetJacobianAt(int eqIndex) const override;

    private:
        SketchPointPtr m_point;
        double m_targetX;
        double m_targetY;
    };

    // =========================================================================
    // 10. RadiusConstraint — circle/arc radius constraint
    //     This is a special constraint: it does not modify point coordinates;
    //     instead it directly sets the radius value.
    //     Equation: currentRadius - targetRadius = 0
    //     Note: because radius is not part of the solver's variable system
    //     (variables are point coordinates only), this constraint is applied
    //     by direct assignment and does not participate in Newton-Raphson iteration.
    // =========================================================================
    class RadiusConstraint : public Constraint {
    public:
        RadiusConstraint(const SketchCirclePtr& circle, double targetRadius);
        RadiusConstraint(const SketchArcPtr& arc, double targetRadius);

        void SetTargetRadius(double r) { m_targetRadius = r; }
        double GetTargetRadius() const { return m_targetRadius; }

        bool IsValid() const override;
        std::string GetDescription() const override;
        double GetError() const override;
        int GetEquationCount() const override { return 0; } // Does not participate in matrix solving
        std::vector<SketchPointPtr> GetInvolvedPoints() const override;
        std::vector<JacobianEntry> GetJacobianAt(int eqIndex) const override;

        /** Apply the radius value directly (called by the solver before iteration) */
        void ApplyDirectly();

    private:
        SketchCirclePtr m_circle;
        SketchArcPtr m_arc;
        double m_targetRadius;
    };

} // namespace cad_sketch
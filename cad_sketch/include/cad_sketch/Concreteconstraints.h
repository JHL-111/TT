/**
 * @file ConcreteConstraints.h
 * @brief 所有具体几何约束的实现
 *
 * 包含 10 种约束类型：
 *   1. HorizontalConstraint    - 线段水平（两端点 Y 相同）
 *   2. VerticalConstraint      - 线段竖直（两端点 X 相同）
 *   3. CoincidentConstraint    - 两点重合
 *   4. DistanceConstraint      - 两点之间距离 = 目标值
 *   5. ParallelConstraint      - 两条线段平行
 *   6. PerpendicularConstraint - 两条线段垂直
 *   7. AngleConstraint         - 两条线段夹角 = 目标值
 *   8. EqualLengthConstraint   - 两条线段等长
 *   9. FixedConstraint         - 点固定在指定坐标
 *  10. RadiusConstraint        - 圆/弧半径 = 目标值
 *
 * 每个约束都实现了 GetError / GetJacobianAt / GetInvolvedPoints
 * 供 Newton-Raphson 求解器使用。
 */

#pragma once

#include "Constraint.h"
#include "SketchLine.h"
#include "SketchCircle.h"
#include "SketchArc.h"
#include <cmath>

namespace cad_sketch {

    // =========================================================================
    // 1. HorizontalConstraint — 线段水平
    //    方程: p2.y - p1.y = 0
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
    // 2. VerticalConstraint — 线段竖直
    //    方程: p2.x - p1.x = 0
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
    // 3. CoincidentConstraint — 两点重合
    //    方程: p1.x - p2.x = 0, p1.y - p2.y = 0 （两个方程）
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
    // 4. DistanceConstraint — 两点距离 = targetDistance
    //    方程: (p1.x-p2.x)² + (p1.y-p2.y)² - d² = 0
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
    // 5. ParallelConstraint — 两线段平行
    //    方程: (a2.x-a1.x)*(b2.y-b1.y) - (a2.y-a1.y)*(b2.x-b1.x) = 0
    //    即两个方向向量的叉积为零
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
    // 6. PerpendicularConstraint — 两线段垂直
    //    方程: (a2.x-a1.x)*(b2.x-b1.x) + (a2.y-a1.y)*(b2.y-b1.y) = 0
    //    即两个方向向量的点积为零
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
    // 7. AngleConstraint — 两线段夹角 = targetAngle (弧度)
    //    方程: atan2(cross, dot) - targetAngle = 0
    //    其中 cross = dx1*dy2 - dy1*dx2, dot = dx1*dx2 + dy1*dy2
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
        double m_targetAngle; // 弧度
    };

    // =========================================================================
    // 8. EqualLengthConstraint — 两线段等长
    //    方程: |L1|² - |L2|² = 0
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
    // 9. FixedConstraint — 固定点坐标
    //    方程: p.x - tx = 0, p.y - ty = 0 （两个方程）
    // =========================================================================
    class FixedConstraint : public Constraint {
    public:
        FixedConstraint(const SketchPointPtr& point, double targetX, double targetY);
        FixedConstraint(const SketchPointPtr& point); // 固定在当前位置

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
    // 10. RadiusConstraint — 圆/弧半径约束
    //     这是一个特殊约束：它不修改点坐标，而是直接设置半径值
    //     方程: currentRadius - targetRadius = 0
    //     注意: 由于半径不是求解器变量系统的一部分（变量只有点坐标），
    //     这个约束通过直接赋值来实现，不参与 Newton-Raphson 迭代
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
        int GetEquationCount() const override { return 0; } // 不参与矩阵求解
        std::vector<SketchPointPtr> GetInvolvedPoints() const override;
        std::vector<JacobianEntry> GetJacobianAt(int eqIndex) const override;

        /** 直接应用半径值（求解器在迭代前调用） */
        void ApplyDirectly();

    private:
        SketchCirclePtr m_circle;
        SketchArcPtr m_arc;
        double m_targetRadius;
    };

} // namespace cad_sketch#pragma once

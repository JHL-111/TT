/**
 * @file ConstraintSolverTest.h
 * @brief 约束求解器的测试函数
 *
 * 使用方法：
 *   在 main.cpp 中 #include 这个文件，然后在 mainWindow.show() 之前调用 RunConstraintTests()
 *   测试结果会打印到控制台（qDebug 输出）
 *   确认全部通过后，删掉调用即可，不影响正式代码
 */

#pragma once

#include "cad_sketch/Sketch.h"
#include "cad_sketch/ConcreteConstraints.h"
#include <QDebug>
#include <cmath>

inline void RunConstraintTests() {
    using namespace cad_sketch;

    qDebug() << "========================================";
    qDebug() << "  Constraint Solver Tests";
    qDebug() << "========================================";

    int passed = 0;
    int failed = 0;

    // ---------------------------------------------------------------
    // Test 1: Horizontal Constraint
    // 一条斜线 (0,0)→(5,3)，加水平约束后两端点 Y 应该相同
    // ---------------------------------------------------------------
    {
        auto line = std::make_shared<SketchLine>(0.0, 0.0, 5.0, 3.0);
        Sketch sketch("Test_Horizontal");
        sketch.AddElement(line->GetStartPoint());
        sketch.AddElement(line->GetEndPoint());
        sketch.AddElement(line);
        sketch.AddConstraint(std::make_shared<HorizontalConstraint>(line));
        sketch.SolveConstraints();

        auto result = sketch.GetConstraintSolver()->GetLastResult();
        double dy = std::abs(line->GetEndPoint()->GetY() - line->GetStartPoint()->GetY());
        bool ok = result.converged && dy < 1e-4;

        qDebug() << "[Test1 Horizontal]" << (ok ? "PASS" : "FAIL")
            << "converged=" << result.converged
            << "iterations=" << result.iterations
            << "dy=" << dy
            << "p1=(" << line->GetStartPoint()->GetX() << "," << line->GetStartPoint()->GetY() << ")"
            << "p2=(" << line->GetEndPoint()->GetX() << "," << line->GetEndPoint()->GetY() << ")";
        ok ? passed++ : failed++;
    }

    // ---------------------------------------------------------------
    // Test 2: Vertical Constraint
    // 一条斜线 (0,0)→(5,3)，加竖直约束后两端点 X 应该相同
    // ---------------------------------------------------------------
    {
        auto line = std::make_shared<SketchLine>(0.0, 0.0, 5.0, 3.0);
        Sketch sketch("Test_Vertical");
        sketch.AddElement(line);
        sketch.AddConstraint(std::make_shared<VerticalConstraint>(line));
        sketch.SolveConstraints();

        auto result = sketch.GetConstraintSolver()->GetLastResult();
        double dx = std::abs(line->GetEndPoint()->GetX() - line->GetStartPoint()->GetX());
        bool ok = result.converged && dx < 1e-4;

        qDebug() << "[Test2 Vertical]" << (ok ? "PASS" : "FAIL")
            << "converged=" << result.converged
            << "dx=" << dx;
        ok ? passed++ : failed++;
    }

    // ---------------------------------------------------------------
    // Test 3: Coincident Constraint
    // 两个点 (1,2) 和 (4,6)，加重合约束后应该在同一位置
    // ---------------------------------------------------------------
    {
        auto p1 = std::make_shared<SketchPoint>(1.0, 2.0);
        auto p2 = std::make_shared<SketchPoint>(4.0, 6.0);
        Sketch sketch("Test_Coincident");
        sketch.AddElement(p1);
        sketch.AddElement(p2);
        sketch.AddConstraint(std::make_shared<CoincidentConstraint>(p1, p2));
        sketch.SolveConstraints();

        auto result = sketch.GetConstraintSolver()->GetLastResult();
        double dist = std::sqrt(std::pow(p1->GetX() - p2->GetX(), 2) + std::pow(p1->GetY() - p2->GetY(), 2));
        bool ok = result.converged && dist < 1e-4;

        qDebug() << "[Test3 Coincident]" << (ok ? "PASS" : "FAIL")
            << "converged=" << result.converged
            << "distance=" << dist
            << "p1=(" << p1->GetX() << "," << p1->GetY() << ")"
            << "p2=(" << p2->GetX() << "," << p2->GetY() << ")";
        ok ? passed++ : failed++;
    }

    // ---------------------------------------------------------------
    // Test 4: Distance Constraint
    // 两个点 (0,0) 和 (3,4)，约束距离=10
    // 当前距离=5，求解后应该变成 10
    // ---------------------------------------------------------------
    {
        auto p1 = std::make_shared<SketchPoint>(0.0, 0.0);
        auto p2 = std::make_shared<SketchPoint>(3.0, 4.0);
        Sketch sketch("Test_Distance");
        sketch.AddElement(p1);
        sketch.AddElement(p2);
        // 固定 p1 在原点
        sketch.AddConstraint(std::make_shared<FixedConstraint>(p1, 0.0, 0.0));
        // p1 到 p2 距离 = 10
        sketch.AddConstraint(std::make_shared<DistanceConstraint>(p1, p2, 10.0));
        sketch.SolveConstraints();

        auto result = sketch.GetConstraintSolver()->GetLastResult();
        double actualDist = std::sqrt(p2->GetX() * p2->GetX() + p2->GetY() * p2->GetY());
        bool ok = result.converged && std::abs(actualDist - 10.0) < 1e-3;

        qDebug() << "[Test4 Distance]" << (ok ? "PASS" : "FAIL")
            << "converged=" << result.converged
            << "target=10 actual=" << actualDist
            << "p2=(" << p2->GetX() << "," << p2->GetY() << ")";
        ok ? passed++ : failed++;
    }

    // ---------------------------------------------------------------
    // Test 5: Fixed Constraint
    // 点 (5,7) 固定到 (0,0)
    // ---------------------------------------------------------------
    {
        auto p = std::make_shared<SketchPoint>(5.0, 7.0);
        Sketch sketch("Test_Fixed");
        sketch.AddElement(p);
        sketch.AddConstraint(std::make_shared<FixedConstraint>(p, 0.0, 0.0));
        sketch.SolveConstraints();

        auto result = sketch.GetConstraintSolver()->GetLastResult();
        bool ok = result.converged && std::abs(p->GetX()) < 1e-6 && std::abs(p->GetY()) < 1e-6;

        qDebug() << "[Test5 Fixed]" << (ok ? "PASS" : "FAIL")
            << "converged=" << result.converged
            << "p=(" << p->GetX() << "," << p->GetY() << ")";
        ok ? passed++ : failed++;
    }

    // ---------------------------------------------------------------
    // Test 6: Parallel Constraint
    // lineA: (0,0)→(4,2)  lineB: (1,3)→(3,5)
    // 加平行约束后，两线段方向向量的叉积应该≈0
    // ---------------------------------------------------------------
    {
        auto lineA = std::make_shared<SketchLine>(0.0, 0.0, 4.0, 2.0);
        auto lineB = std::make_shared<SketchLine>(1.0, 3.0, 3.0, 5.0);
        Sketch sketch("Test_Parallel");
        sketch.AddElement(lineA);
        sketch.AddElement(lineB);
        sketch.AddConstraint(std::make_shared<ParallelConstraint>(lineA, lineB));
        sketch.SolveConstraints();

        auto result = sketch.GetConstraintSolver()->GetLastResult();
        double dx1 = lineA->GetEndPoint()->GetX() - lineA->GetStartPoint()->GetX();
        double dy1 = lineA->GetEndPoint()->GetY() - lineA->GetStartPoint()->GetY();
        double dx2 = lineB->GetEndPoint()->GetX() - lineB->GetStartPoint()->GetX();
        double dy2 = lineB->GetEndPoint()->GetY() - lineB->GetStartPoint()->GetY();
        double cross = std::abs(dx1 * dy2 - dy1 * dx2);
        bool ok = result.converged && cross < 1e-4;

        qDebug() << "[Test6 Parallel]" << (ok ? "PASS" : "FAIL")
            << "converged=" << result.converged
            << "cross_product=" << cross;
        ok ? passed++ : failed++;
    }

    // ---------------------------------------------------------------
    // Test 7: Perpendicular Constraint
    // lineA: (0,0)→(4,0)  lineB: (4,0)→(6,3)
    // 加垂直约束后，点积应该≈0
    // ---------------------------------------------------------------
    {
        auto lineA = std::make_shared<SketchLine>(0.0, 0.0, 4.0, 0.0);
        auto lineB = std::make_shared<SketchLine>(4.0, 0.0, 6.0, 3.0);
        Sketch sketch("Test_Perpendicular");
        sketch.AddElement(lineA);
        sketch.AddElement(lineB);
        // 固定 lineA 的起点，让系统有参考
        sketch.AddConstraint(std::make_shared<FixedConstraint>(lineA->GetStartPoint()));
        sketch.AddConstraint(std::make_shared<PerpendicularConstraint>(lineA, lineB));
        sketch.SolveConstraints();

        auto result = sketch.GetConstraintSolver()->GetLastResult();
        double dx1 = lineA->GetEndPoint()->GetX() - lineA->GetStartPoint()->GetX();
        double dy1 = lineA->GetEndPoint()->GetY() - lineA->GetStartPoint()->GetY();
        double dx2 = lineB->GetEndPoint()->GetX() - lineB->GetStartPoint()->GetX();
        double dy2 = lineB->GetEndPoint()->GetY() - lineB->GetStartPoint()->GetY();
        double dot = std::abs(dx1 * dx2 + dy1 * dy2);
        bool ok = result.converged && dot < 1e-3;

        qDebug() << "[Test7 Perpendicular]" << (ok ? "PASS" : "FAIL")
            << "converged=" << result.converged
            << "dot_product=" << dot;
        ok ? passed++ : failed++;
    }

    // ---------------------------------------------------------------
    // Test 8: Equal Length Constraint
    // lineA: (0,0)→(3,0) 长度3,  lineB: (0,0)→(0,5) 长度5
    // 加等长约束后两线段长度应该相同
    // ---------------------------------------------------------------
    {
        auto lineA = std::make_shared<SketchLine>(0.0, 0.0, 3.0, 0.0);
        auto lineB = std::make_shared<SketchLine>(0.0, 0.0, 0.0, 5.0);
        Sketch sketch("Test_EqualLength");
        sketch.AddElement(lineA);
        sketch.AddElement(lineB);
        sketch.AddConstraint(std::make_shared<EqualLengthConstraint>(lineA, lineB));
        sketch.SolveConstraints();

        auto result = sketch.GetConstraintSolver()->GetLastResult();
        double lenA = lineA->GetLength();
        double lenB = lineB->GetLength();
        bool ok = result.converged && std::abs(lenA - lenB) < 1e-3;

        qDebug() << "[Test8 EqualLength]" << (ok ? "PASS" : "FAIL")
            << "converged=" << result.converged
            << "lenA=" << lenA << "lenB=" << lenB;
        ok ? passed++ : failed++;
    }

    // ---------------------------------------------------------------
    // Test 9: Multi-Constraint (核心场景)
    // 起点固定在原点，lineA长度=5，lineA终点与lineB起点重合，两线垂直
    // ---------------------------------------------------------------
    {
        auto lineA = std::make_shared<SketchLine>(0.0, 0.0, 4.0, 3.0);
        auto lineB = std::make_shared<SketchLine>(4.0, 3.0, 7.0, 1.0);
        Sketch sketch("Test_Multi");
        sketch.AddElement(lineA);
        sketch.AddElement(lineB);

        sketch.AddConstraint(std::make_shared<FixedConstraint>(lineA->GetStartPoint(), 0.0, 0.0));
        sketch.AddConstraint(std::make_shared<DistanceConstraint>(
            lineA->GetStartPoint(), lineA->GetEndPoint(), 5.0));
        sketch.AddConstraint(std::make_shared<CoincidentConstraint>(
            lineA->GetEndPoint(), lineB->GetStartPoint()));
        sketch.AddConstraint(std::make_shared<PerpendicularConstraint>(lineA, lineB));

        sketch.SolveConstraints();

        auto result = sketch.GetConstraintSolver()->GetLastResult();

        // 验证
        double originDist = std::sqrt(
            std::pow(lineA->GetStartPoint()->GetX(), 2) +
            std::pow(lineA->GetStartPoint()->GetY(), 2));
        double lenA = lineA->GetLength();
        double coincDist = std::sqrt(
            std::pow(lineA->GetEndPoint()->GetX() - lineB->GetStartPoint()->GetX(), 2) +
            std::pow(lineA->GetEndPoint()->GetY() - lineB->GetStartPoint()->GetY(), 2));
        double dx1 = lineA->GetEndPoint()->GetX() - lineA->GetStartPoint()->GetX();
        double dy1 = lineA->GetEndPoint()->GetY() - lineA->GetStartPoint()->GetY();
        double dx2 = lineB->GetEndPoint()->GetX() - lineB->GetStartPoint()->GetX();
        double dy2 = lineB->GetEndPoint()->GetY() - lineB->GetStartPoint()->GetY();
        double dot = std::abs(dx1 * dx2 + dy1 * dy2);

        bool ok = result.converged &&
            originDist < 1e-4 &&
            std::abs(lenA - 5.0) < 1e-3 &&
            coincDist < 1e-4 &&
            dot < 1e-3;

        qDebug() << "[Test9 MultiConstraint]" << (ok ? "PASS" : "FAIL")
            << "converged=" << result.converged
            << "iterations=" << result.iterations;
        qDebug() << "  origin_dist=" << originDist
            << "lineA_len=" << lenA << "(target=5)"
            << "coincident_dist=" << coincDist
            << "dot_product=" << dot;
        ok ? passed++ : failed++;
    }

    // ---------------------------------------------------------------
    // Test 10: Radius Constraint
    // 圆半径从 3.0 变成 7.5
    // ---------------------------------------------------------------
    {
        auto center = std::make_shared<SketchPoint>(0.0, 0.0);
        auto circle = std::make_shared<SketchCircle>(center, 3.0);
        Sketch sketch("Test_Radius");
        sketch.AddElement(circle);

        auto rc = std::make_shared<RadiusConstraint>(circle, 7.5);
        sketch.AddConstraint(rc);
        sketch.SolveConstraints();

        bool ok = std::abs(circle->GetRadius() - 7.5) < 1e-6;

        qDebug() << "[Test10 Radius]" << (ok ? "PASS" : "FAIL")
            << "radius=" << circle->GetRadius() << "(target=7.5)";
        ok ? passed++ : failed++;
    }

    // ---------------------------------------------------------------
    // 总结
    // ---------------------------------------------------------------
    qDebug() << "========================================";
    qDebug() << "  Results:" << passed << "passed," << failed << "failed,"
        << (passed + failed) << "total";
    qDebug() << "========================================";
}
#pragma once

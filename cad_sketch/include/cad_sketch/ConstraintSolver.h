/**
 * @file ConstraintSolver.h
 * @brief 基于 Newton-Raphson 的几何约束求解器
 *
 * 求解流程：
 * 1. CollectVariables() — 扫描所有约束，注册涉及的点坐标为自由变量
 * 2. 组装误差向量 F 和雅可比矩阵 J
 * 3. 解法方程 (J^T·J)·Δx = -J^T·F（最小二乘形式）
 * 4. 更新变量 x += Δx
 * 5. 重复直到 ||F|| < tolerance
 *
 * 矩阵运算使用 Eigen 库（header-only，需要 find_package(Eigen3)）
 */

#pragma once

#include "Constraint.h"
#include "SketchPoint.h"
#include <vector>
#include <map>
#include <memory>

namespace cad_sketch {

    /**
     * @brief 求解器状态信息
     */
    struct SolveResult {
        bool converged;         // 是否收敛
        int iterations;         // 迭代次数
        double finalError;      // 最终误差
        int variableCount;      // 变量数量
        int equationCount;      // 方程数量
    };

    class ConstraintSolver {
    public:
        ConstraintSolver();
        ~ConstraintSolver() = default;

        // ---------- 约束管理 ----------
        void AddConstraint(const ConstraintPtr& constraint);
        void RemoveConstraint(const ConstraintPtr& constraint);
        void ClearConstraints();
        const std::vector<ConstraintPtr>& GetConstraints() const;

        // ---------- 求解 ----------

        /** 执行完整的求解流程（变量注册 + 迭代求解） */
        bool Solve();

        /** 获取上次求解的详细结果 */
        SolveResult GetLastResult() const { return m_lastResult; }

        bool ValidateConstraints() const;

        // ---------- 参数设置 ----------
        void SetTolerance(double tolerance);
        double GetTolerance() const;
        void SetMaxIterations(int maxIterations);
        int GetMaxIterations() const;

        /**
         * 设置阻尼系数（0 < damping <= 1）
         * 1.0 = 标准 Newton-Raphson
         * < 1.0 = 阻尼 Newton（更稳定但收敛更慢）
         */
        void SetDamping(double damping);
        double GetDamping() const;

    private:
        std::vector<ConstraintPtr> m_constraints;
        double m_tolerance;
        int m_maxIterations;
        double m_damping;
        SolveResult m_lastResult;

        // ---------- 变量系统 ----------

        struct Variable {
            SketchPoint* point;  // 所属的点
            bool isX;            // true = X 坐标, false = Y 坐标
            int index;           // 在全局变量向量中的索引
        };

        std::vector<Variable> m_variables;
        std::map<SketchPoint*, int> m_pointToVarIndex; // point -> 其 x 变量的索引

        /**
         * 扫描所有约束，收集涉及的点，注册为变量
         * 每个点注册 2 个变量（x, y），自动去重
         */
        void CollectVariables();

        /**
         * 将变量映射信息同步到每个约束对象
         */
        void SyncVariableMapToConstraints();

        /**
         * 应用直接约束（如 RadiusConstraint）
         */
        void ApplyDirectConstraints();

        // ---------- Newton-Raphson 核心 ----------

        /**
         * 执行 Newton-Raphson 迭代求解
         * 在 CollectVariables() 之后调用
         */
        bool IterativeSolve();

        /**
         * 计算系统总误差 ||F||
         */
        double CalculateSystemError() const;

        /**
         * 统计所有约束产生的总方程数
         */
        int GetTotalEquationCount() const;

        /**
         * 从变量向量读取当前值 / 写回点坐标
         */
        void ReadVariables(std::vector<double>& x) const;
        void WriteVariables(const std::vector<double>& x);
    };

} // namespace cad_sketch
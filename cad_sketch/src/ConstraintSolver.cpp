/**
 * @file ConstraintSolver.cpp
 * @brief Newton-Raphson 约束求解器的完整实现
 *
 * 核心算法：
 *   在每次迭代中：
 *   1. 组装误差向量 F (M×1) 和雅可比矩阵 J (M×N)
 *   2. 构造法方程 (J^T·J)·Δx = -J^T·F
 *   3. 用 Eigen 的 LDLT 分解求解 Δx
 *   4. 阻尼更新 x += damping * Δx
 *   5. 检查收敛条件 ||F|| < tolerance
 *
 * 其中 M = 方程总数, N = 变量总数（每个点 2 个：x, y）
 */

#include "cad_sketch/ConstraintSolver.h"
#include "cad_sketch/ConcreteConstraints.h"

#include <Eigen/Dense>
#include <cmath>
#include <algorithm>
#include <set>

namespace cad_sketch {

    ConstraintSolver::ConstraintSolver()
        : m_tolerance(1e-6)
        , m_maxIterations(100)
        , m_damping(1.0)
        , m_lastResult{ false, 0, 0.0, 0, 0 } {
    }

    // =====================================================================
    // 约束管理
    // =====================================================================

    void ConstraintSolver::AddConstraint(const ConstraintPtr& constraint) {
        m_constraints.push_back(constraint);
    }

    void ConstraintSolver::RemoveConstraint(const ConstraintPtr& constraint) {
        auto it = std::find(m_constraints.begin(), m_constraints.end(), constraint);
        if (it != m_constraints.end()) {
            m_constraints.erase(it);
        }
    }

    void ConstraintSolver::ClearConstraints() {
        m_constraints.clear();
    }

    const std::vector<ConstraintPtr>& ConstraintSolver::GetConstraints() const {
        return m_constraints;
    }

    // =====================================================================
    // 参数设置
    // =====================================================================

    void ConstraintSolver::SetTolerance(double tolerance) { m_tolerance = tolerance; }
    double ConstraintSolver::GetTolerance() const { return m_tolerance; }
    void ConstraintSolver::SetMaxIterations(int max) { m_maxIterations = max; }
    int ConstraintSolver::GetMaxIterations() const { return m_maxIterations; }
    void ConstraintSolver::SetDamping(double d) { m_damping = std::max(0.01, std::min(1.0, d)); }
    double ConstraintSolver::GetDamping() const { return m_damping; }

    bool ConstraintSolver::ValidateConstraints() const {
        for (const auto& c : m_constraints) {
            if (c->IsActive() && !c->IsValid()) return false;
        }
        return true;
    }

    // =====================================================================
    // 主求解入口
    // =====================================================================

    bool ConstraintSolver::Solve() {
        m_lastResult = { false, 0, 0.0, 0, 0 };

        if (m_constraints.empty()) {
            m_lastResult.converged = true;
            return true;
        }

        // 1. 先应用直接约束（如 RadiusConstraint）
        ApplyDirectConstraints();

        // 2. 收集变量
        CollectVariables();

        int N = static_cast<int>(m_variables.size());
        int M = GetTotalEquationCount();

        m_lastResult.variableCount = N;
        m_lastResult.equationCount = M;

        if (N == 0 || M == 0) {
            m_lastResult.converged = true;
            return true;
        }

        // 3. 同步变量映射到约束
        SyncVariableMapToConstraints();

        // 4. 迭代求解
        bool result = IterativeSolve();
        m_lastResult.converged = result;
        return result;
    }

    // =====================================================================
    // 变量注册系统
    // =====================================================================

    void ConstraintSolver::CollectVariables() {
        m_variables.clear();
        m_pointToVarIndex.clear();

        // 用 set 去重（同一个点可能被多个约束引用）
        std::set<SketchPoint*> registeredPoints;

        for (const auto& constraint : m_constraints) {
            if (!constraint->IsActive()) continue;
            if (constraint->GetEquationCount() == 0) continue; // 跳过直接约束

            auto points = constraint->GetInvolvedPoints();
            for (const auto& pt : points) {
                if (!pt) continue;
                SketchPoint* rawPtr = pt.get();

                if (registeredPoints.find(rawPtr) == registeredPoints.end()) {
                    registeredPoints.insert(rawPtr);

                    int baseIndex = static_cast<int>(m_variables.size());
                    m_pointToVarIndex[rawPtr] = baseIndex;

                    // 注册 x 和 y 两个变量
                    Variable varX;
                    varX.point = rawPtr;
                    varX.isX = true;
                    varX.index = baseIndex;
                    m_variables.push_back(varX);

                    Variable varY;
                    varY.point = rawPtr;
                    varY.isX = false;
                    varY.index = baseIndex + 1;
                    m_variables.push_back(varY);
                }
            }
        }
    }

    void ConstraintSolver::SyncVariableMapToConstraints() {
        for (auto& constraint : m_constraints) {
            if (constraint->IsActive()) {
                constraint->SetVariableMap(m_pointToVarIndex);
            }
        }
    }

    void ConstraintSolver::ApplyDirectConstraints() {
        for (auto& constraint : m_constraints) {
            if (!constraint->IsActive()) continue;
            // 检查是否是 RadiusConstraint（通过 dynamic_cast）
            auto radiusConstraint = std::dynamic_pointer_cast<RadiusConstraint>(constraint);
            if (radiusConstraint) {
                radiusConstraint->ApplyDirectly();
            }
        }
    }

    // =====================================================================
    // Newton-Raphson 迭代核心
    // =====================================================================

    bool ConstraintSolver::IterativeSolve() {
        int N = static_cast<int>(m_variables.size());   // 变量数
        int M = GetTotalEquationCount();                 // 方程数

        if (N == 0 || M == 0) return true;

        for (int iteration = 0; iteration < m_maxIterations; ++iteration) {
            m_lastResult.iterations = iteration + 1;

            // ----- Step 1: 组装 F 向量和 J 矩阵 -----
            Eigen::VectorXd F(M);
            Eigen::MatrixXd J = Eigen::MatrixXd::Zero(M, N);

            F.setZero();

            int eqRow = 0;
            for (const auto& constraint : m_constraints) {
                if (!constraint->IsActive()) continue;

                int eqCount = constraint->GetEquationCount();
                if (eqCount == 0) continue;

                for (int eq = 0; eq < eqCount; ++eq) {
                    // 填误差值
                    F(eqRow) = constraint->GetErrorAt(eq);

                    // 填雅可比矩阵的一行
                    auto jacobianEntries = constraint->GetJacobianAt(eq);
                    for (const auto& entry : jacobianEntries) {
                        if (entry.variableIndex >= 0 && entry.variableIndex < N) {
                            J(eqRow, entry.variableIndex) = entry.derivative;
                        }
                    }

                    eqRow++;
                }
            }

            // ----- Step 2: 检查收敛 -----
            double error = F.norm();
            m_lastResult.finalError = error;

            if (error < m_tolerance) {
                return true; // 收敛！
            }

            // ----- Step 3: 解法方程 (J^T·J)·Δx = -J^T·F -----
            // 法方程形式适用于超定/欠定系统
            Eigen::MatrixXd JtJ = J.transpose() * J;
            Eigen::VectorXd JtF = J.transpose() * F;

            // 添加微小的正则化项，防止奇异矩阵（Levenberg-Marquardt 思想）
            double lambda = 1e-8 * JtJ.diagonal().maxCoeff();
            if (lambda < 1e-12) lambda = 1e-12;
            JtJ.diagonal().array() += lambda;

            // LDLT 分解求解
            Eigen::VectorXd dx = JtJ.ldlt().solve(-JtF);

            // 检查求解是否有效
            if (dx.hasNaN()) {
                return false; // 数值问题，求解失败
            }

            // ----- Step 4: 阻尼更新变量 -----
            for (int i = 0; i < N; ++i) {
                if (m_variables[i].isX) {
                    double newVal = m_variables[i].point->GetX() + m_damping * dx(i);
                    m_variables[i].point->SetX(newVal);
                }
                else {
                    double newVal = m_variables[i].point->GetY() + m_damping * dx(i);
                    m_variables[i].point->SetY(newVal);
                }
            }
        }

        // 超过最大迭代次数仍未收敛
        m_lastResult.finalError = CalculateSystemError();
        return false;
    }

    // =====================================================================
    // 辅助函数
    // =====================================================================

    double ConstraintSolver::CalculateSystemError() const {
        double totalError = 0.0;
        for (const auto& constraint : m_constraints) {
            if (!constraint->IsActive()) continue;
            int eqCount = constraint->GetEquationCount();
            for (int eq = 0; eq < eqCount; ++eq) {
                double e = constraint->GetErrorAt(eq);
                totalError += e * e;
            }
        }
        return std::sqrt(totalError);
    }

    int ConstraintSolver::GetTotalEquationCount() const {
        int total = 0;
        for (const auto& constraint : m_constraints) {
            if (constraint->IsActive()) {
                total += constraint->GetEquationCount();
            }
        }
        return total;
    }

    void ConstraintSolver::ReadVariables(std::vector<double>& x) const {
        x.resize(m_variables.size());
        for (size_t i = 0; i < m_variables.size(); ++i) {
            if (m_variables[i].isX) {
                x[i] = m_variables[i].point->GetX();
            }
            else {
                x[i] = m_variables[i].point->GetY();
            }
        }
    }

    void ConstraintSolver::WriteVariables(const std::vector<double>& x) {
        for (size_t i = 0; i < m_variables.size() && i < x.size(); ++i) {
            if (m_variables[i].isX) {
                m_variables[i].point->SetX(x[i]);
            }
            else {
                m_variables[i].point->SetY(x[i]);
            }
        }
    }

} // namespace cad_sketch
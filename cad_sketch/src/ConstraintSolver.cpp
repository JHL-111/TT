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

        // ★ 关键修复：求解前保存所有点的坐标快照
        std::vector<double> savedCoords;
        ReadVariables(savedCoords);

        // 4. 迭代求解
        bool result = IterativeSolve();
        m_lastResult.converged = result;

        // ★ 关键修复：求解失败时回滚到原始坐标，防止线条消失
        if (!result) {
            WriteVariables(savedCoords);
        }

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
    // Newton-Raphson Iterative
    // =====================================================================

    bool ConstraintSolver::IterativeSolve() {
        int N = static_cast<int>(m_variables.size());   // Number of variables
        int M = GetTotalEquationCount();                 // Number of Equations

        if (N == 0 || M == 0) return true;

        double prevError = std::numeric_limits<double>::max();
        int divergeCount = 0;  // Consecutive divergence counting

        for (int iteration = 0; iteration < m_maxIterations; ++iteration) {
            m_lastResult.iterations = iteration + 1;

            // ----- Step 1: Assemble the F vector and the J matrix  -----
            Eigen::VectorXd F(M);
            Eigen::MatrixXd J = Eigen::MatrixXd::Zero(M, N);

            F.setZero();

            int eqRow = 0;
            for (const auto& constraint : m_constraints) {
                if (!constraint->IsActive()) continue;

                int eqCount = constraint->GetEquationCount();
                if (eqCount == 0) continue;

                for (int eq = 0; eq < eqCount; ++eq) {
                    // Fill in the error value
                    F(eqRow) = constraint->GetErrorAt(eq);

                    // Fill in a row of the Jacobian matrix
                    auto jacobianEntries = constraint->GetJacobianAt(eq);
                    for (const auto& entry : jacobianEntries) {
                        if (entry.variableIndex >= 0 && entry.variableIndex < N) {
                            J(eqRow, entry.variableIndex) = entry.derivative;
                        }
                    }

                    eqRow++;
                }
            }

            // ----- Step 2: Check convergence -----
            double error = F.norm();
            m_lastResult.finalError = error;

            if (error < m_tolerance) {
                return true; 
            }

            // Check whether the detection error is continuously increasing (diverging)
            if (error > prevError * 1.1) {
                divergeCount++;
                if (divergeCount >= 5) {
                    // If there are 5 consecutive increases in errors, 
                    // it is determined as a constraint conflict / divergence.
                    return false;
                }
            }
            else {
                divergeCount = 0;
            }
            prevError = error;

            // ----- Step 3: solve equation (J^T·J)·Δx = -J^T·F -----
            // The form of the mathematical equation is applicable to over-determined/under-determined systems
            Eigen::MatrixXd JtJ = J.transpose() * J;
            Eigen::VectorXd JtF = J.transpose() * F;

            // Add a small regularization term to prevent the occurrence of singular matrices 
            // (following the Levenberg-Marquardt concept)
            double lambda = 1e-8 * JtJ.diagonal().maxCoeff();
            if (lambda < 1e-12) lambda = 1e-12;
            JtJ.diagonal().array() += lambda;

            // LDLT Decomposition and compute
            Eigen::VectorXd dx = JtJ.ldlt().solve(-JtF);

            // check if valid
            if (dx.hasNaN()) {
                return false; 
            }

            // Limit the maximum displacement of a single step
            double maxStep = dx.lpNorm<Eigen::Infinity>();
            if (maxStep > 1e6) {
                // If the step size is too large, 
                // it indicates that the system may be diverging or encountering constraint conflicts, 
                // and the process should be terminated prematurely.
                return false;
            }

            // ----- Step 4: update variable -----
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

        // Failed to converge even after exceeding the maximum number of iterations
        m_lastResult.finalError = CalculateSystemError();
        return false;
    }

    // =====================================================================
    // auxiliary function
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
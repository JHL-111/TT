/**
 * @file ConstraintSolver.cpp
 * @brief Complete implementation of the Newton-Raphson constraint solver
 *
 * Core algorithm — each iteration:
 *   1. Assemble the error vector F (M×1) and the Jacobian matrix J (M×N)
 *   2. Form the normal equations (J^T·J)·Δx = -J^T·F
 *   3. Solve for Δx using Eigen's LDLT decomposition
 *   4. Damped update: x += damping * Δx
 *   5. Check convergence: ||F|| < tolerance
 *
 * where M = total number of equations,
 *       N = total number of variables (2 per point: x and y)
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
    // Constraint management
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
    // Parameter settings
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
    // Main solve entry point
    // =====================================================================

    bool ConstraintSolver::Solve() {
        m_lastResult = { false, 0, 0.0, 0, 0 };

        if (m_constraints.empty()) {
            m_lastResult.converged = true;
            return true;
        }

        // 1. Apply direct constraints first (e.g. RadiusConstraint)
        ApplyDirectConstraints();

        // 2. Collect variables
        CollectVariables();

        int N = static_cast<int>(m_variables.size());
        int M = GetTotalEquationCount();

        m_lastResult.variableCount = N;
        m_lastResult.equationCount = M;

        if (N == 0 || M == 0) {
            m_lastResult.converged = true;
            return true;
        }

        // 3. Sync variable map to all constraints
        SyncVariableMapToConstraints();

        // Key fix: save a snapshot of all point coordinates before solving
        std::vector<double> savedCoords;
        ReadVariables(savedCoords);

        // 4. Iterative solving
        bool result = IterativeSolve();
        m_lastResult.converged = result;

        // Key fix: roll back to original coordinates on failure to prevent lines disappearing
        if (!result) {
            WriteVariables(savedCoords);
        }

        return result;
    }

    // =====================================================================
    // Variable registration system
    // =====================================================================

    void ConstraintSolver::CollectVariables() {
        m_variables.clear();
        m_pointToVarIndex.clear();

        // Use a set to deduplicate (the same point may be referenced by multiple constraints)
        std::set<SketchPoint*> registeredPoints;

        for (const auto& constraint : m_constraints) {
            if (!constraint->IsActive()) continue;
            if (constraint->GetEquationCount() == 0) continue; // Skip direct constraints

            auto points = constraint->GetInvolvedPoints();
            for (const auto& pt : points) {
                if (!pt) continue;
                SketchPoint* rawPtr = pt.get();

                if (registeredPoints.find(rawPtr) == registeredPoints.end()) {
                    registeredPoints.insert(rawPtr);

                    int baseIndex = static_cast<int>(m_variables.size());
                    m_pointToVarIndex[rawPtr] = baseIndex;

                    // Register x and y as two separate variables
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
            // Check for RadiusConstraint via dynamic_cast
            auto radiusConstraint = std::dynamic_pointer_cast<RadiusConstraint>(constraint);
            if (radiusConstraint) {
                radiusConstraint->ApplyDirectly();
            }
        }
    }

    // =====================================================================
    // Newton-Raphson iterative solver
    // =====================================================================

    bool ConstraintSolver::IterativeSolve() {
        int N = static_cast<int>(m_variables.size());   // Number of variables
        int M = GetTotalEquationCount();                 // Number of equations

        if (N == 0 || M == 0) return true;

        double prevError = std::numeric_limits<double>::max();
        int divergeCount = 0;  // Consecutive divergence counter

        for (int iteration = 0; iteration < m_maxIterations; ++iteration) {
            m_lastResult.iterations = iteration + 1;

            // ----- Step 1: Assemble the F vector and the J matrix -----
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

            // Check whether the error is consistently increasing (diverging)
            if (error > prevError * 1.1) {
                divergeCount++;
                if (divergeCount >= 5) {
                    // 5 consecutive increases: determined to be a constraint
                    // conflict or divergence
                    return false;
                }
            }
            else {
                divergeCount = 0;
            }
            prevError = error;

            // ----- Step 3: Solve (J^T·J)·Δx = -J^T·F -----
            // This least-squares form handles over- and under-determined systems
            Eigen::MatrixXd JtJ = J.transpose() * J;
            Eigen::VectorXd JtF = J.transpose() * F;

            // Add a small regularisation term to prevent singular matrices
            // (Levenberg-Marquardt concept)
            double lambda = 1e-8 * JtJ.diagonal().maxCoeff();
            if (lambda < 1e-12) lambda = 1e-12;
            JtJ.diagonal().array() += lambda;

            // LDLT decomposition and solve
            Eigen::VectorXd dx = JtJ.ldlt().solve(-JtF);

            // Check for NaN
            if (dx.hasNaN()) {
                return false;
            }

            // Limit the maximum step size
            double maxStep = dx.lpNorm<Eigen::Infinity>();
            if (maxStep > 1e6) {
                // Step size too large: the system may be diverging or there is
                // a constraint conflict; terminate early
                return false;
            }

            // ----- Step 4: Update variables -----
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

        // Failed to converge within the maximum number of iterations
        m_lastResult.finalError = CalculateSystemError();
        return false;
    }

    // =====================================================================
    // Helper functions
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
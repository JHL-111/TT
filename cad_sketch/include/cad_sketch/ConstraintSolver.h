/**
 * @file ConstraintSolver.h
 * @brief Newton-Raphson based geometric constraint solver
 *
 * Solve procedure:
 * 1. CollectVariables() — scan all constraints and register involved point
 *    coordinates as free variables
 * 2. Assemble the error vector F and the Jacobian matrix J
 * 3. Solve the normal equations (J^T·J)·Δx = -J^T·F  (least-squares form)
 * 4. Update variables: x += Δx
 * 5. Repeat until ||F|| < tolerance
 *
 * Matrix operations use the Eigen library (header-only; requires find_package(Eigen3)).
 */

#pragma once

#include "Constraint.h"
#include "SketchPoint.h"
#include <vector>
#include <map>
#include <memory>

namespace cad_sketch {

    /**
     * @brief Solver status information
     */
    struct SolveResult {
        bool converged;         // Whether the solver converged
        int iterations;         // Number of iterations performed
        double finalError;      // Final residual error
        int variableCount;      // Number of variables
        int equationCount;      // Number of equations
    };

    class ConstraintSolver {
    public:
        ConstraintSolver();
        ~ConstraintSolver() = default;

        // ---------- Constraint management ----------
        void AddConstraint(const ConstraintPtr& constraint);
        void RemoveConstraint(const ConstraintPtr& constraint);
        void ClearConstraints();
        const std::vector<ConstraintPtr>& GetConstraints() const;

        // ---------- Solving ----------

        /** Execute the full solve procedure (variable registration + iterative solving) */
        bool Solve();

        /** Get detailed results from the last solve */
        SolveResult GetLastResult() const { return m_lastResult; }

        bool ValidateConstraints() const;

        // ---------- Parameter settings ----------
        void SetTolerance(double tolerance);
        double GetTolerance() const;
        void SetMaxIterations(int maxIterations);
        int GetMaxIterations() const;

        /**
         * Set the damping coefficient (0 < damping <= 1).
         * 1.0 = standard Newton-Raphson
         * < 1.0 = damped Newton (more stable but slower convergence)
         */
        void SetDamping(double damping);
        double GetDamping() const;

    private:
        std::vector<ConstraintPtr> m_constraints;
        double m_tolerance;
        int m_maxIterations;
        double m_damping;
        SolveResult m_lastResult;

        // ---------- Variable system ----------

        struct Variable {
            SketchPoint* point;  // The point this variable belongs to
            bool isX;            // true = X coordinate,  false = Y coordinate
            int index;           // Index in the global variable vector
        };

        std::vector<Variable> m_variables;
        std::map<SketchPoint*, int> m_pointToVarIndex; // point -> index of its x variable

        /**
         * Scan all constraints, collect involved points, and register them as variables.
         * Each point registers 2 variables (x, y); duplicates are automatically removed.
         */
        void CollectVariables();

        /**
         * Propagate the variable map to every constraint object.
         */
        void SyncVariableMapToConstraints();

        /**
         * Apply direct constraints (e.g. RadiusConstraint).
         */
        void ApplyDirectConstraints();

        // ---------- Newton-Raphson core ----------

        /**
         * Execute Newton-Raphson iterative solving.
         * Must be called after CollectVariables().
         */
        bool IterativeSolve();

        /**
         * Calculate the total system error ||F||.
         */
        double CalculateSystemError() const;

        /**
         * Count the total number of equations across all constraints.
         */
        int GetTotalEquationCount() const;

        /**
         * Read current values from the variable vector / write back to point coordinates.
         */
        void ReadVariables(std::vector<double>& x) const;
        void WriteVariables(const std::vector<double>& x);
    };

} // namespace cad_sketch
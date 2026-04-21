/**
 * @file Constraint.h
 * @brief Geometric constraint system - the core of parametric sketch modelling
 *
 * The constraint base class defines the common interface for all geometric constraints.
 * Each constraint subclass must provide: an error function, Jacobian entries, and an
 * equation count. This information is used by ConstraintSolver during Newton-Raphson
 * iterative solving.
 */

#pragma once

#include "SketchElement.h"
#include "SketchPoint.h"
#include <memory>
#include <vector>
#include <string>
#include <map>

namespace cad_sketch {

    enum class ConstraintType {
        Horizontal,
        Vertical,
        Parallel,
        Perpendicular,
        Coincident,
        Distance,
        Angle,
        Radius,
        Diameter,
        Equal,
        Fixed,
        Tangent,
        Symmetric,
        Midpoint
    };

    /**
     * @brief Jacobian entry — records the partial derivative of one equation
     *        with respect to one variable.
     *
     * Used by the solver when assembling the Jacobian matrix J:
     * J[equationIndex][variableIndex] = derivative
     */
    struct JacobianEntry {
        int variableIndex;   // Index of the variable in the global variable vector
        double derivative;   // Partial derivative value: ∂error/∂variable
    };

    class Constraint {
    public:
        Constraint(ConstraintType type);
        virtual ~Constraint() = default;

        ConstraintType GetType() const;
        int GetId() const;
        void SetId(int id);

        void AddElement(const SketchElementPtr& element);
        const std::vector<SketchElementPtr>& GetElements() const;

        bool IsActive() const;
        void SetActive(bool active);

        // ---------- Original interface (kept for compatibility) ----------

        /** Check whether the elements referenced by this constraint are valid */
        virtual bool IsValid() const = 0;

        /** Return a human-readable description of the constraint (for the UI) */
        virtual std::string GetDescription() const = 0;

        /** Return the error value of the first equation (kept for backward compatibility) */
        virtual double GetError() const = 0;

        // ---------- New interface required by the Newton-Raphson solver ----------

        /**
         * Number of equations produced by this constraint.
         * e.g. Horizontal produces 1 equation; Coincident produces 2.
         */
        virtual int GetEquationCount() const = 0;

        /**
         * Return the error value of equation eqIndex.
         * Default implementation: calls GetError() when eqIndex == 0.
         */
        virtual double GetErrorAt(int eqIndex) const { return GetError(); }

        /**
         * Collect all SketchPoint pointers involved in this constraint.
         * The solver uses these to register variables (each point has x and y variables).
         */
        virtual std::vector<SketchPointPtr> GetInvolvedPoints() const = 0;

        /**
         * Return the partial derivatives of equation eqIndex with respect to all variables.
         * variableIndex values are assigned by the solver after variable registration.
         *
         * Subclasses look up their points' global variable indices via m_variableMap.
         */
        virtual std::vector<JacobianEntry> GetJacobianAt(int eqIndex) const = 0;

        /**
         * Called by the solver after variable registration to inform the constraint
         * of the global variable indices for its points.
         * key: SketchPoint pointer,  value: index of that point's x coordinate
         * in the global variable vector (y index = x index + 1)
         */
        void SetVariableMap(const std::map<SketchPoint*, int>& varMap);

        /** Get the x-variable index for a given point; y index = x index + 1 */
        int GetPointVariableIndex(const SketchPointPtr& point) const;

    protected:
        ConstraintType m_type;
        int m_id;
        std::vector<SketchElementPtr> m_elements;
        bool m_active;

        /** Map from point pointer to global variable index (set by the solver) */
        std::map<SketchPoint*, int> m_variableMap;

        static int s_nextId;
    };

    using ConstraintPtr = std::shared_ptr<Constraint>;

} // namespace cad_sketch
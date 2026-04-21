/**
 * @file Sketch.h
 * @brief Sketch class
 *
 */

#pragma once

#include "SketchElement.h"
#include "SketchPoint.h"
#include "SketchLine.h"
#include "SketchCircle.h"
#include "SketchArc.h"
#include "Constraint.h"
#include "ConstraintSolver.h"
#include "cad_sketch/SketchProfile.h"
#include <vector>
#include <memory>
#include <string>
#include <gp_Ax3.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Face.hxx>


namespace cad_sketch {

    /**
     * @class Sketch
     * @brief Sketch class - the digital drawing board for 2D design
     *
     * A sketch is the foundation of 3D modelling.
     */
    class Sketch {
    public:
        /** Default constructor */
        Sketch();

        /**
         * @param name Sketch name
         */
        Sketch(const std::string& name);

        /** Destructor */
        ~Sketch() = default;

        /**
         * Get the sketch name
         * @return The name of the sketch
         */
        const std::string& GetName() const;

        /**
         * Set the sketch name
         * @param name New name
         */
        void SetName(const std::string& name);

        // ========== Element management ==========

        /**
         * Add an element
         * @param element The element to add (point, line, circle, etc.)
         */
        void AddElement(const SketchElementPtr& element);

        /** Remove all elements */
        void ClearElements();

        /** Remove a specific element (used for undo) */
        void RemoveElement(const SketchElementPtr& element);

        /**
         * Get all elements
         * @return Const reference to the element list
         */
        const std::vector<SketchElementPtr>& GetElements() const;

        /**
         * Find an element by ID
         * @param id Unique identifier of the element
         * @return The found element, or nullptr if not found
         */
        SketchElementPtr GetElementById(int id) const;

        // ========== Constraint management ==========

        /**
         * Add a constraint - establish a geometric relationship between elements
         * @param constraint The constraint (parallel, perpendicular, equal, etc.)
         */
        void AddConstraint(const ConstraintPtr& constraint);

        /**
         * Remove a constraint - release a geometric relationship between elements
         * @param constraint The constraint to remove
         */
        void RemoveConstraint(const ConstraintPtr& constraint);

        /** Remove all constraints */
        void ClearConstraints();

        /**
         * Get all constraints
         * @return Const reference to the constraint list
         */
        const std::vector<ConstraintPtr>& GetConstraints() const;

        // ========== Solver operations ==========

        /**
         * Solve constraints - adjust element positions to satisfy all constraints
         * @return true if solving succeeded, false if constraints conflict
         */
        bool SolveConstraints();

        /**
         * Validate constraints - check whether the current constraint system is consistent
         * @return true if the system is valid, false if there are conflicts
         */
        bool ValidateConstraints() const;

        /** Get detailed results from the constraint solver */
        ConstraintSolver* GetConstraintSolver() { return &m_solver; }

        // ========== Selection management ==========

        /**
         * Select an element
         * @param element The element to select
         */
        void SelectElement(const SketchElementPtr& element);

        /**
         * Deselect an element
         * @param element The element to deselect
         */
        void DeselectElement(const SketchElementPtr& element);

        /** Clear the current selection */
        void ClearSelection();

        /**
         * Get the currently selected elements
         * @return List of selected elements
         */
        std::vector<SketchElementPtr> GetSelectedElements() const;

        // ========== Utility methods ==========

        /**
         * Check whether the sketch is empty
         * @return true if empty, false if it contains elements
         */
        bool IsEmpty() const;

        /**
         * Get element count
         * @return Total number of elements
         */
        int GetElementCount() const;

        /**
         * Get constraint count
         * @return Total number of constraints
         */
        int GetConstraintCount() const;

        // ========== 3D geometry generation ==========

        /** Get all closed profiles computed so far */
        std::vector<SketchProfilePtr> GetProfiles() const { return m_profiles; }

        /** Core algorithm: detect and update profiles */
        void UpdateProfiles(const gp_Ax3& cs);

        // --- Reference plane and coordinate system management ---
        void SetBaseFace(const TopoDS_Face& face) { m_baseFace = face; }
        TopoDS_Face GetBaseFace() const { return m_baseFace; }

        void SetBaseCS(const gp_Ax3& cs) { m_baseCS = cs; }
        gp_Ax3 GetBaseCS() const { return m_baseCS; }

    private:
        /** Sketch name */
        std::string m_name;

        /** Collection of sketch elements */
        std::vector<SketchElementPtr> m_elements;

        /** Collection of constraints */
        std::vector<ConstraintPtr> m_constraints;

        /** Constraint solver */
        ConstraintSolver m_solver;

        /** Collection of closed profiles */
        std::vector<SketchProfilePtr> m_profiles;

        TopoDS_Face m_baseFace;
        gp_Ax3 m_baseCS;
    };

    /** Smart pointer type alias for Sketch */
    using SketchPtr = std::shared_ptr<Sketch>;

} // namespace cad_sketch
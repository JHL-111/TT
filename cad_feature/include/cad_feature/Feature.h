/**
 * @file Feature.h
 * @brief Feature system
 *
 * This file defines the core structure of the CAD feature system:
 * extrude, revolve, sweep, loft, and more.
 *
 */

#pragma once

#include "cad_core/Shape.h"    // Geometry shape base
#include "cad_core/ICommand.h" // Command interface - gives features undo/redo capability
#include <memory>              // Smart pointers
#include <string>              // Strings - carrier for feature names and parameters
#include <map>                 // Map container - dictionary from parameter name to value

namespace cad_feature {

    /**
     * @enum FeatureType
     * @brief Feature type enumeration
     */
    enum class FeatureType {
        Extrude,      // Extrude
        Revolve,      // Revolve
        Sweep,        // Sweep
        Loft,         // Loft
        Fillet,       // Fillet
        Chamfer,      // Chamfer
        Cut,          // Cut
        Union,        // Union
        Intersection  // Intersection
    };

    /**
     * @enum FeatureState
     * @brief Feature state enumeration - tracks the lifecycle of a feature
     */
    enum class FeatureState {
        Created,     // Created
        Previewing,  // Previewing
        Executed,    // Executed
        Failed       // Failed
    };

    /**
     * @class Feature
     * @brief Feature base class - the abstract foundation for all modelling operations
     *
     * This abstract base class defines the capabilities every feature must provide:
     * managing parameters, generating shapes, and tracking state.
     */
    class Feature {
    public:
        /**
         * Constructor - creates a new feature
         * @param type Feature type
         * @param name Feature name
         */
        Feature(FeatureType type, const std::string& name);

        /** Virtual destructor */
        virtual ~Feature() = default;

        /**
         * Get feature type
         * @return Feature type enum value
         */
        FeatureType GetType() const;

        /**
         * Get feature name
         * @return Const reference to the feature name string
         */
        const std::string& GetName() const;

        /**
         * Set feature name
         * @param name New name
         */
        void SetName(const std::string& name);

        /**
         * Get feature ID
         * @return The feature's unique ID
         */
        int GetId() const;

        /**
         * Set feature ID
         * @param id New ID value
         */
        void SetId(int id);

        /**
         * Get feature state
         * @return Current feature state
         */
        FeatureState GetState() const;

        /**
         * Set feature state
         * @param state New state value
         */
        void SetState(FeatureState state);

        /**
         * Check whether the feature is active
         * @return true if active, false if suppressed
         */
        bool IsActive() const;

        /**
         * Set the feature active state
         * @param active true to activate, false to suppress
         */
        void SetActive(bool active);

        // ========== Parameter management ==========

        /**
         * Set a parameter value
         * @param name Parameter name, e.g. "distance", "angle"
         * @param value Parameter value
         */
        void SetParameter(const std::string& name, double value);

        /**
         * Get a parameter value
         * @param name Parameter name
         * @return Current value of the parameter
         */
        double GetParameter(const std::string& name) const;

        /**
         * Check whether a parameter exists
         * @param name Parameter name
         * @return true if the parameter exists, false otherwise
         */
        bool HasParameter(const std::string& name) const;

        // ========== Shape operations ==========

        /**
         * Create the shape - the core capability of a feature.
         * Pure virtual: every concrete feature must implement its own version.
         * @return The generated geometry shape
         */
        virtual cad_core::ShapePtr CreateShape() const = 0;

        /**
         * Create a preview shape - defaults to the same as CreateShape()
         * @return Preview geometry shape
         */
        virtual cad_core::ShapePtr CreatePreviewShape() const;

        /**
         * Validate parameters - checks that all parameter values are sensible
         * @return true if parameters are valid, false otherwise
         */
        virtual bool ValidateParameters() const = 0;

        // ========== Command interface - enables undo/redo ==========

        /**
         * Create a command object - wraps the feature operation into an undoable command
         * @return The corresponding command object
         */
        virtual std::shared_ptr<cad_core::ICommand> CreateCommand() const = 0;

        // ===== Result shape accessors =====
        void SetResultShape(const cad_core::ShapePtr& shape) { m_resultShape = shape; }
        cad_core::ShapePtr GetResultShape() const { return m_resultShape; }

    protected:
        /** Feature type */
        FeatureType m_type;

        /** Feature name */
        std::string m_name;

        /** Feature ID */
        int m_id;

        /** Feature state */
        FeatureState m_state;

        /** Active flag */
        bool m_active;

        /** Parameter map */
        std::map<std::string, double> m_parameters;

        /** Static ID counter */
        static int s_nextId;

        /** The final 3D solid produced by this feature */
        cad_core::ShapePtr m_resultShape = nullptr;
    };

    /** Smart pointer type alias for Feature */
    using FeaturePtr = std::shared_ptr<Feature>;

} // namespace cad_feature
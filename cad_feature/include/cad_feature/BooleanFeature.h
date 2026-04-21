#pragma once

#include "cad_feature/Feature.h"
#include "cad_core/Shape.h"
#include <vector>

namespace cad_feature {

    // Boolean operation type enum
    enum class BooleanType {
        Union,          // Union
        Intersection,   // Intersection
        Difference      // Difference
    };

    class BooleanFeature : public Feature {
    public:
        BooleanFeature(const std::string& name);
        virtual ~BooleanFeature() = default;

        // Set/get the operation type
        void SetOperationType(BooleanType type);
        BooleanType GetOperationType() const;

        // Set/get target bodies (Target Bodies)
        void SetTargets(const std::vector<cad_core::ShapePtr>& targets);
        const std::vector<cad_core::ShapePtr>& GetTargets() const;

        // Set/get tool bodies (Tool Bodies)
        void SetTools(const std::vector<cad_core::ShapePtr>& tools);
        const std::vector<cad_core::ShapePtr>& GetTools() const;

        // Override base class virtual functions
        cad_core::ShapePtr CreateShape() const override;
        bool ValidateParameters() const override;
        std::shared_ptr<cad_core::ICommand> CreateCommand() const override;

    private:
        BooleanType m_boolType;
        std::vector<cad_core::ShapePtr> m_targets; // Target bodies
        std::vector<cad_core::ShapePtr> m_tools;   // Tool bodies
    };

    using BooleanFeaturePtr = std::shared_ptr<BooleanFeature>;

} // namespace cad_feature
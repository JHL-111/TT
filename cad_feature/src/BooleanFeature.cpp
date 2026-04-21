#include "cad_feature/BooleanFeature.h"
#include "cad_core/BooleanOperations.h"

namespace cad_feature {

    BooleanFeature::BooleanFeature(const std::string& name)
        : Feature(FeatureType::Union, name), m_boolType(BooleanType::Union) {
    }

    void BooleanFeature::SetOperationType(BooleanType type) {
        m_boolType = type;
        // Synchronously update the parent feature type identifier
        if (type == BooleanType::Union) m_type = FeatureType::Union;
        else if (type == BooleanType::Intersection) m_type = FeatureType::Intersection;
        else if (type == BooleanType::Difference) m_type = FeatureType::Cut;
    }

    BooleanType BooleanFeature::GetOperationType() const { return m_boolType; }

    void BooleanFeature::SetTargets(const std::vector<cad_core::ShapePtr>& targets) {
        m_targets = targets;
    }

    const std::vector<cad_core::ShapePtr>& BooleanFeature::GetTargets() const { return m_targets; }

    void BooleanFeature::SetTools(const std::vector<cad_core::ShapePtr>& tools) {
        m_tools = tools;
    }

    const std::vector<cad_core::ShapePtr>& BooleanFeature::GetTools() const { return m_tools; }

    bool BooleanFeature::ValidateParameters() const {
        if (m_boolType == BooleanType::Union) {
            return (m_targets.size() + m_tools.size()) >= 2; // Union requires at least 2 shapes
        }
        else {
            return !m_targets.empty() && !m_tools.empty(); // Intersection and difference both need a target and a tool
        }
    }

    std::shared_ptr<cad_core::ICommand> BooleanFeature::CreateCommand() const {
        return nullptr; // Not yet implemented
    }

    // Generate the resulting 3D solid from the boolean operation
    cad_core::ShapePtr BooleanFeature::CreateShape() const {
        if (!ValidateParameters()) return nullptr;

        cad_core::ShapePtr result = nullptr;

        if (m_boolType == BooleanType::Union) {
            std::vector<cad_core::ShapePtr> allShapes = m_targets;
            allShapes.insert(allShapes.end(), m_tools.begin(), m_tools.end());
            result = cad_core::BooleanOperations::Union(allShapes);
        }
        else if (m_boolType == BooleanType::Intersection) {
            result = m_targets[0];
            for (size_t i = 1; i < m_targets.size(); ++i) {
                if (result) result = cad_core::BooleanOperations::Intersection({ result, m_targets[i] });
            }
            for (const auto& tool : m_tools) {
                if (result) result = cad_core::BooleanOperations::Intersection({ result, tool });
            }
        }
        else if (m_boolType == BooleanType::Difference) {
            result = m_targets[0];
            for (const auto& tool : m_tools) {
                if (result) result = cad_core::BooleanOperations::Difference(result, tool);
            }
        }

        return result;
    }

} // namespace cad_feature
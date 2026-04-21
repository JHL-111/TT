#pragma once

#include "Feature.h"
#include "cad_core/Shape.h"
#include <vector>

namespace cad_feature {

    class LoftFeature : public Feature {
    public:
        LoftFeature();
        LoftFeature(const std::string& name);
        virtual ~LoftFeature() = default;

        // Section operations
        void AddSection(const cad_core::ShapePtr& section);
        void ClearSections();
        const std::vector<cad_core::ShapePtr>& GetSections() const;
        int GetSectionCount() const;

        // Loft parameters
        void SetSolid(bool solid);
        bool GetSolid() const;

        // Inherited from Feature
        cad_core::ShapePtr CreateShape() const override;
        bool ValidateParameters() const override;
        std::shared_ptr<cad_core::ICommand> CreateCommand() const override;

    private:
        std::vector<cad_core::ShapePtr> m_sections;

        bool AreSectionsValid() const;
        cad_core::ShapePtr LoftSections() const;
    };

    using LoftFeaturePtr = std::shared_ptr<LoftFeature>;

} // namespace cad_feature
#pragma once

#include "Feature.h"
#include "cad_core/Shape.h"

namespace cad_feature {

    class ExtrudeFeature : public Feature {
    public:
        ExtrudeFeature();
        ExtrudeFeature(const std::string& name);
        virtual ~ExtrudeFeature() = default;

        // Supports any Shape as the extrusion profile
        void SetProfileShape(const cad_core::ShapePtr& profile);
        const cad_core::ShapePtr& GetProfileShape() const;

        // Extrude parameters
        void SetDistance(double distance);
        double GetDistance() const;

        // Feature interface
        cad_core::ShapePtr CreateShape() const override;
        bool ValidateParameters() const override;
        std::shared_ptr<cad_core::ICommand> CreateCommand() const override;

    private:
        cad_core::ShapePtr m_profileShape; // Stores the input profile face
    };

    using ExtrudeFeaturePtr = std::shared_ptr<ExtrudeFeature>;

} // namespace cad_feature
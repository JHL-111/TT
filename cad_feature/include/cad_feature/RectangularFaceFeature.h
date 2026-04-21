#pragma once

#include "cad_feature/Feature.h"
#include "cad_core/ShapeFactory.h"

namespace cad_feature {

    class RectangularFaceFeature : public Feature {
    public:
        RectangularFaceFeature(const std::string& name);
        virtual ~RectangularFaceFeature() = default;

        // Set/get parameters
        void SetWidth(double width);
        double GetWidth() const;

        void SetHeight(double height);
        double GetHeight() const;

        // Override base class virtual functions
        cad_core::ShapePtr CreateShape() const override;
        bool ValidateParameters() const override;
        std::shared_ptr<cad_core::ICommand> CreateCommand() const override;
    };

    using RectangularFaceFeaturePtr = std::shared_ptr<RectangularFaceFeature>;

} // namespace cad_feature
#include "cad_feature/RectangularFaceFeature.h"

namespace cad_feature {

    RectangularFaceFeature::RectangularFaceFeature(const std::string& name)
        : Feature(FeatureType::Extrude, name) {
        SetParameter("width", 10.0);
        SetParameter("height", 10.0);
    }

    void RectangularFaceFeature::SetWidth(double width) {
        SetParameter("width", width);
    }

    double RectangularFaceFeature::GetWidth() const {
        return GetParameter("width");
    }

    void RectangularFaceFeature::SetHeight(double height) {
        SetParameter("height", height);
    }

    double RectangularFaceFeature::GetHeight() const {
        return GetParameter("height");
    }

    bool RectangularFaceFeature::ValidateParameters() const {
        return GetWidth() > 0.0 && GetHeight() > 0.0;
    }

    std::shared_ptr<cad_core::ICommand> RectangularFaceFeature::CreateCommand() const {
        return nullptr;
    }

    cad_core::ShapePtr RectangularFaceFeature::CreateShape() const {
        if (!ValidateParameters()) return nullptr;

        return cad_core::ShapeFactory::CreateRectangleFace(GetWidth(), GetHeight());
    }

} // namespace cad_feature
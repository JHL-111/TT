#pragma once

#include "cad_feature/Feature.h"
#include "cad_core/ShapeFactory.h"

namespace cad_feature {

    class RectangularFaceFeature : public Feature {
    public:
        RectangularFaceFeature(const std::string& name);
        virtual ~RectangularFaceFeature() = default;

        // 设置与获取参数 (Parameters)
        void SetWidth(double width);
        double GetWidth() const;

        void SetHeight(double height);
        double GetHeight() const;

        // 覆盖基类接口
        cad_core::ShapePtr CreateShape() const override;
        bool ValidateParameters() const override;
        std::shared_ptr<cad_core::ICommand> CreateCommand() const override;
    };

    using RectangularFaceFeaturePtr = std::shared_ptr<RectangularFaceFeature>;

} // namespace cad_feature#pragma once

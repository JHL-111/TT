#pragma once

#include "Feature.h"
#include "cad_core/Shape.h" 

namespace cad_feature {

    class ExtrudeFeature : public Feature {
    public:
        ExtrudeFeature();
        ExtrudeFeature(const std::string& name);
        virtual ~ExtrudeFeature() = default;

        //支持任意 Shape 作为拉伸轮廓】
        void SetProfileShape(const cad_core::ShapePtr& profile);
        const cad_core::ShapePtr& GetProfileShape() const;

        // Extrude parameters
        void SetDistance(double distance);
        double GetDistance() const;

        void SetDirection(double x, double y, double z);
        void GetDirection(double& x, double& y, double& z) const;

        void SetTaperAngle(double angle);
        double GetTaperAngle() const;

        void SetMidplane(bool midplane);
        bool GetMidplane() const;

        // Feature interface
        cad_core::ShapePtr CreateShape() const override;
        bool ValidateParameters() const override;
        std::shared_ptr<cad_core::ICommand> CreateCommand() const override;

    private:
        cad_core::ShapePtr m_profileShape; // 存储作为输入的轮廓面
    };

    using ExtrudeFeaturePtr = std::shared_ptr<ExtrudeFeature>;

} // namespace cad_feature
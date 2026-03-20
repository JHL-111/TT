#pragma once

#include "SketchElement.h"
#include "SketchPoint.h"
#include <vector>

namespace cad_sketch {

    class SketchCurve : public SketchElement {
    public:
        SketchCurve();
        virtual ~SketchCurve() = default;

        // 实现基类的纯虚函数 (Pure Virtual Functions)
        void Translate(double dx, double dy) override;
        void Rotate(double cx, double cy, double angleRad) override;

        // 曲线特有接口：添加控制点 (Control Points)
        void AddControlPoint(const SketchPointPtr& point);
        const std::vector<SketchPointPtr>& GetControlPoints() const;

        std::string GetDescription() const override;

    private:
        std::vector<SketchPointPtr> m_controlPoints; // 控制点集合
    };

    using SketchCurvePtr = std::shared_ptr<SketchCurve>;

} // namespace cad_sketch
#pragma once

#include "SketchElement.h"
#include "SketchPoint.h"
#include <vector>

namespace cad_sketch {

    class SketchCurve : public SketchElement {
    public:
        SketchCurve();
        virtual ~SketchCurve() = default;

        // Implement the base class pure virtual functions
        void Translate(double dx, double dy) override;
        void Rotate(double cx, double cy, double angleRad) override;

        // Control point interface
        void AddControlPoint(const SketchPointPtr& point);
        const std::vector<SketchPointPtr>& GetControlPoints() const;

        std::string GetDescription() const override;

    private:
        std::vector<SketchPointPtr> m_controlPoints; // Control point collection
    };

    using SketchCurvePtr = std::shared_ptr<SketchCurve>;

} // namespace cad_sketch
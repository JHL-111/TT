#pragma once

#include "SketchElement.h"
#include "SketchPoint.h"
#include <vector>
#include <memory>

namespace cad_sketch {

    enum class SnapType {
        None,
        Endpoint,
        Midpoint,
        Center,
        Nearest,
        Intersection,
        Perpendicular,
        Tangent,
        Grid
    };

    struct SnapResult {
        bool found;
        SnapType type;
        cad_core::Point snapPoint;
        SketchElementPtr element;

        SnapResult() : found(false), type(SnapType::None), element(nullptr) {}
    };

    class SnappingManager {
    public:
        SnappingManager();
        ~SnappingManager() = default;

        void SetSnapTolerance(double tolerance);
        double GetSnapTolerance() const;

        void EnableSnapType(SnapType type);
        void DisableSnapType(SnapType type);
        bool IsSnapTypeEnabled(SnapType type) const;

        void SetGridSize(double gridSize);
        double GetGridSize() const;

        SnapResult FindSnapPoint(const cad_core::Point& inputPoint,
            const std::vector<SketchElementPtr>& elements) const;

        SnapResult SnapToGrid(const cad_core::Point& inputPoint) const;
        SnapResult SnapToEndpoints(const cad_core::Point& inputPoint,
            const std::vector<SketchElementPtr>& elements) const;
        SnapResult SnapToMidpoints(const cad_core::Point& inputPoint,
            const std::vector<SketchElementPtr>& elements) const;
        SnapResult SnapToCenters(const cad_core::Point& inputPoint,
            const std::vector<SketchElementPtr>& elements) const;
        SnapResult SnapToNearest(const cad_core::Point& inputPoint,
            const std::vector<SketchElementPtr>& elements) const;

    private:
        double m_snapTolerance;
        double m_gridSize;
        std::vector<SnapType> m_enabledSnapTypes;

        bool IsWithinTolerance(const cad_core::Point& p1, const cad_core::Point& p2) const;

        // Internal helper functions for computing geometric distances
        double DistanceToLineSegment(const cad_core::Point& p, const cad_core::Point& a, const cad_core::Point& b, cad_core::Point& closestPoint) const;
        double DistanceToCircle(const cad_core::Point& p, const cad_core::Point& center, double radius, cad_core::Point& closestPoint) const;
        double DistanceToArc(const cad_core::Point& p, const cad_core::Point& center, double radius, double startAngle, double sweepAngle, cad_core::Point& closestPoint) const;
    };

} // namespace cad_sketch
#pragma once

#include "cad_core/Shape.h"
#include <vector>

namespace cad_core {

    class BooleanOperations {
    public:
        // Boolean operation types
        enum class BooleanType {
            Union,        // Union
            Intersection, // Intersection
            Difference    // Difference
        };

        // Boolean operations
        static ShapePtr Union(const ShapePtr& shape1, const ShapePtr& shape2);
        static ShapePtr Union(const std::vector<ShapePtr>& shapes);

        static ShapePtr Intersection(const ShapePtr& shape1, const ShapePtr& shape2);
        static ShapePtr Intersection(const std::vector<ShapePtr>& shapes);

        static ShapePtr Difference(const ShapePtr& shape1, const ShapePtr& shape2);

        // General boolean operation
        static ShapePtr BooleanOperation(const ShapePtr& shape1, const ShapePtr& shape2, BooleanType type);
        static ShapePtr BooleanOperation(const std::vector<ShapePtr>& shapes, BooleanType type);

        // Validate whether a shape is valid
        static bool IsValidShape(const ShapePtr& shape);

        // Repair a shape
        static ShapePtr FixShape(const ShapePtr& shape);

        // Simplify a shape
        static ShapePtr SimplifyShape(const ShapePtr& shape);

    private:
        // Private helper methods
        static ShapePtr PerformUnion(const ShapePtr& shape1, const ShapePtr& shape2);
        static ShapePtr PerformIntersection(const ShapePtr& shape1, const ShapePtr& shape2);
        static ShapePtr PerformDifference(const ShapePtr& shape1, const ShapePtr& shape2);

        // Shape validation and repair
        static bool ValidateInputs(const ShapePtr& shape1, const ShapePtr& shape2);
        static ShapePtr PostProcessResult(const TopoDS_Shape& result);
    };

} // namespace cad_core
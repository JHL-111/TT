#pragma once
#include "cad_core/Shape.h"
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <vector>

namespace cad_core {

    class FilletChamferOperations {
    public:
        // Fillet operations
        static ShapePtr CreateFillet(const ShapePtr& shape, const std::vector<TopoDS_Edge>& edges, double radius);
        static ShapePtr CreateFillet(const ShapePtr& shape, const TopoDS_Edge& edge, double radius);

        // Chamfer operations
        static ShapePtr CreateChamfer(const ShapePtr& shape, const std::vector<TopoDS_Edge>& edges, double distance1, double distance2 = -1.0);
        static ShapePtr CreateChamfer(const ShapePtr& shape, const TopoDS_Edge& edge, double distance);


        // Get edges and faces of a shape
        static std::vector<TopoDS_Edge> GetEdges(const ShapePtr& shape);
        static std::vector<TopoDS_Face> GetFaces(const ShapePtr& shape);

        // Edge validation
        static bool IsValidEdgeForFillet(const ShapePtr& shape, const TopoDS_Edge& edge);
        static bool IsValidEdgeForChamfer(const ShapePtr& shape, const TopoDS_Edge& edge);

        // Get faces adjacent to an edge
        static std::vector<TopoDS_Face> GetAdjacentFaces(const ShapePtr& shape, const TopoDS_Edge& edge);

        // Compute suggested fillet radius / chamfer distance
        static double GetSuggestedFilletRadius(const ShapePtr& shape, const TopoDS_Edge& edge);
        static double GetSuggestedChamferDistance(const ShapePtr& shape, const TopoDS_Edge& edge);

    private:
        // Private helper methods
        static ShapePtr PerformFillet(const ShapePtr& shape, const std::vector<TopoDS_Edge>& edges, double radius);
        static ShapePtr PerformChamfer(const ShapePtr& shape, const std::vector<TopoDS_Edge>& edges, double distance);
        static ShapePtr PostProcessResult(const TopoDS_Shape& result);

        // Edge analysis
        static bool AnalyzeEdge(const ShapePtr& shape, const TopoDS_Edge& edge, double& minRadius, double& maxRadius);
        static double GetEdgeLength(const TopoDS_Edge& edge);
        static double GetMinimumRadius(const ShapePtr& shape, const TopoDS_Edge& edge);
    };

} // namespace cad_core
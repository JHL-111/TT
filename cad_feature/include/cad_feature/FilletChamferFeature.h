#pragma once

#include "cad_feature/Feature.h"
#include "cad_core/Shape.h"
#include <TopoDS_Edge.hxx>
#include <vector>

namespace cad_feature {

    // Operation type enum
    enum class FCType {
        Fillet, // Fillet (round edge)
        Chamfer // Chamfer (cut edge)
    };

    class FilletChamferFeature : public Feature {
    public:
        FilletChamferFeature(const std::string& name);
        virtual ~FilletChamferFeature() = default;

        // Set/get operation type
        void SetOperationType(FCType type);
        FCType GetOperationType() const;

        // Set/get the base solid shape (Base Shape)
        void SetBaseShape(const cad_core::ShapePtr& baseShape);
        const cad_core::ShapePtr& GetBaseShape() const;

        // Set/get the selected edges (Selected Edges)
        void SetEdges(const std::vector<TopoDS_Edge>& edges);
        const std::vector<TopoDS_Edge>& GetEdges() const;

        // Set/get fillet/chamfer parameters
        void SetRadius(double radius);       // Radius for Fillet
        double GetRadius() const;

        void SetDistance1(double distance1); // Distance 1 for Chamfer
        double GetDistance1() const;

        void SetDistance2(double distance2); // Distance 2 for Chamfer
        double GetDistance2() const;

        // Override base class virtual functions
        cad_core::ShapePtr CreateShape() const override;
        bool ValidateParameters() const override;
        std::shared_ptr<cad_core::ICommand> CreateCommand() const override;

    private:
        FCType m_fcType;
        cad_core::ShapePtr m_baseShape;
        std::vector<TopoDS_Edge> m_edges;
    };

    using FilletChamferFeaturePtr = std::shared_ptr<FilletChamferFeature>;

} // namespace cad_feature
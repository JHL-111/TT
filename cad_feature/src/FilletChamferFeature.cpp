#include "cad_feature/FilletChamferFeature.h"
#include "cad_core/FilletChamferOperations.h"

namespace cad_feature {

    FilletChamferFeature::FilletChamferFeature(const std::string& name)
        : Feature(FeatureType::Fillet, name), m_fcType(FCType::Fillet) {
        SetParameter("radius", 1.0);
        SetParameter("distance1", 1.0);
        SetParameter("distance2", 1.0);
    }

    void FilletChamferFeature::SetOperationType(FCType type) {
        m_fcType = type;
        if (type == FCType::Fillet) m_type = FeatureType::Fillet;
        else m_type = FeatureType::Chamfer;
    }

    FCType FilletChamferFeature::GetOperationType() const { return m_fcType; }
    void FilletChamferFeature::SetBaseShape(const cad_core::ShapePtr& baseShape) { m_baseShape = baseShape; }
    const cad_core::ShapePtr& FilletChamferFeature::GetBaseShape() const { return m_baseShape; }
    void FilletChamferFeature::SetEdges(const std::vector<TopoDS_Edge>& edges) { m_edges = edges; }
    const std::vector<TopoDS_Edge>& FilletChamferFeature::GetEdges() const { return m_edges; }

    void FilletChamferFeature::SetRadius(double radius) { SetParameter("radius", radius); }
    double FilletChamferFeature::GetRadius() const { return GetParameter("radius"); }
    void FilletChamferFeature::SetDistance1(double distance1) { SetParameter("distance1", distance1); }
    double FilletChamferFeature::GetDistance1() const { return GetParameter("distance1"); }
    void FilletChamferFeature::SetDistance2(double distance2) { SetParameter("distance2", distance2); }
    double FilletChamferFeature::GetDistance2() const { return GetParameter("distance2"); }

    bool FilletChamferFeature::ValidateParameters() const {
        if (!m_baseShape || m_edges.empty()) return false;
        if (m_fcType == FCType::Fillet && GetRadius() <= 0) return false;
        if (m_fcType == FCType::Chamfer && (GetDistance1() <= 0 || GetDistance2() <= 0)) return false;
        return true;
    }

    std::shared_ptr<cad_core::ICommand> FilletChamferFeature::CreateCommand() const { return nullptr; }

    cad_core::ShapePtr FilletChamferFeature::CreateShape() const {
        if (!ValidateParameters()) return nullptr;

        if (m_fcType == FCType::Fillet) {
            return cad_core::FilletChamferOperations::CreateFillet(m_baseShape, m_edges, GetRadius());
        }
        else {
            return cad_core::FilletChamferOperations::CreateChamfer(m_baseShape, m_edges, GetDistance1(), GetDistance2());
        }
    }

} // namespace cad_feature
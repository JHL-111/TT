#include "cad_core/CreateSphereCommand.h"
#include "cad_core/ShapeFactory.h"
#include "cad_core/OCAFManager.h"

namespace cad_core {

    CreateSphereCommand::CreateSphereCommand(const Point& center, double radius, OCAFManager* ocaf)
        : m_center(center), m_radius(radius), m_useCenter(true), m_executed(false), m_ocaf(ocaf) {
    }

    CreateSphereCommand::CreateSphereCommand(double radius, OCAFManager* ocaf)
        : m_center(), m_radius(radius), m_useCenter(false), m_executed(false), m_ocaf(ocaf) {
    }

    bool CreateSphereCommand::Execute() {
        if (m_executed) {
            return true;
        }

        m_ocaf->StartTransaction("Create Sphere");

        if (m_useCenter) {
            m_createdShape = ShapeFactory::CreateSphere(m_center, m_radius);
        }
        else {
            m_createdShape = ShapeFactory::CreateSphere(m_radius);
        }

        if (!m_createdShape) {
            m_ocaf->AbortTransaction();
            return false;
        }

        if (!m_ocaf->AddShape(m_createdShape, "Sphere")) {
            m_ocaf->AbortTransaction();
            return false;
        }

        m_ocaf->CommitTransaction();
        m_executed = true;
        return true;
    }

    bool CreateSphereCommand::Undo() {
        if (!m_executed) {
            return false;
        }

        m_ocaf->Undo();
        m_executed = false;
        return true;
    }

    bool CreateSphereCommand::Redo() {
        if (m_executed) {
            return true;
        }

        m_ocaf->Redo();
        m_executed = true;
        return true;
    }

    const char* CreateSphereCommand::GetName() const {
        return "Create Sphere";
    }

    ShapePtr CreateSphereCommand::GetCreatedShape() const {
        return m_createdShape;
    }

} // namespace cad_core
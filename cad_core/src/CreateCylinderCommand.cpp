#include "cad_core/CreateCylinderCommand.h"
#include "cad_core/ShapeFactory.h"
#include "cad_core/OCAFManager.h"

namespace cad_core {

    CreateCylinderCommand::CreateCylinderCommand(const Point& center, double radius, double height, OCAFManager* ocaf)
        : m_center(center), m_radius(radius), m_height(height), m_useCenter(true), m_executed(false), m_ocaf(ocaf) {
    }

    CreateCylinderCommand::CreateCylinderCommand(double radius, double height, OCAFManager* ocaf)
        : m_center(), m_radius(radius), m_height(height), m_useCenter(false), m_executed(false), m_ocaf(ocaf) {
    }

    bool CreateCylinderCommand::Execute() {
        if (m_executed) {
            return true;
        }

        m_ocaf->StartTransaction("Create Cylinder");

        if (m_useCenter) {
            m_createdShape = ShapeFactory::CreateCylinder(m_center, m_radius, m_height);
        }
        else {
            m_createdShape = ShapeFactory::CreateCylinder(m_radius, m_height);
        }

        if (!m_createdShape) {
            m_ocaf->AbortTransaction();
            return false;
        }

        if (!m_ocaf->AddShape(m_createdShape, "Cylinder")) {
            m_ocaf->AbortTransaction();
            return false;
        }

        m_ocaf->CommitTransaction();
        m_executed = true;
        return true;
    }

    bool CreateCylinderCommand::Undo() {
        if (!m_executed) {
            return false;
        }

        m_ocaf->Undo();
        m_executed = false;
        return true;
    }

    bool CreateCylinderCommand::Redo() {
        if (m_executed) {
            return true;
        }

        m_ocaf->Redo();
        m_executed = true;
        return true;
    }

    const char* CreateCylinderCommand::GetName() const {
        return "Create Cylinder";
    }

    ShapePtr CreateCylinderCommand::GetCreatedShape() const {
        return m_createdShape;
    }

} // namespace cad_core
#include "cad_core/CreateBoxCommand.h"
#include "cad_core/ShapeFactory.h"
#include "cad_core/OCAFManager.h"   // 新增

namespace cad_core {

    CreateBoxCommand::CreateBoxCommand(const Point& corner1, const Point& corner2, OCAFManager* ocaf)
        : m_corner1(corner1), m_corner2(corner2), m_width(0), m_height(0), m_depth(0),
        m_useCorners(true), m_executed(false), m_ocaf(ocaf) {
    }

    CreateBoxCommand::CreateBoxCommand(double width, double height, double depth, OCAFManager* ocaf)
        : m_corner1(), m_corner2(), m_width(width), m_height(height), m_depth(depth),
        m_useCorners(false), m_executed(false), m_ocaf(ocaf) {
    }

    bool CreateBoxCommand::Execute() {
        if (m_executed) {
            return true;
        }

        m_ocaf->StartTransaction("Create Box");

        if (m_useCorners) {
            m_createdShape = ShapeFactory::CreateBox(m_corner1, m_corner2);
        }
        else {
            m_createdShape = ShapeFactory::CreateBox(m_width, m_height, m_depth);
        }

        if (!m_createdShape) {
            m_ocaf->AbortTransaction();
            return false;
        }

        if (!m_ocaf->AddShape(m_createdShape, "Box")) {
            m_ocaf->AbortTransaction();
            return false;
        }

        m_ocaf->CommitTransaction();
        m_executed = true;
        return true;
    }

    bool CreateBoxCommand::Undo() {
        if (!m_executed) {
            return false;
        }

        m_ocaf->Undo();
        m_executed = false;
        return true;
    }

    bool CreateBoxCommand::Redo() {
        if (m_executed) {
            return true;
        }

        m_ocaf->Redo();
        m_executed = true;
        return true;
    }

    const char* CreateBoxCommand::GetName() const {
        return "Create Box";
    }

    ShapePtr CreateBoxCommand::GetCreatedShape() const {
        return m_createdShape;
    }

} // namespace cad_core
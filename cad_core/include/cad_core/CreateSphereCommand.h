#pragma once

#include "ICommand.h"
#include "Shape.h"
#include "Point.h"

namespace cad_core {

class OCAFManager;

class CreateSphereCommand : public ICommand {
public:
    CreateSphereCommand(const Point& center, double radius, OCAFManager* ocaf);
    CreateSphereCommand(double radius, OCAFManager* ocaf);
    virtual ~CreateSphereCommand() = default;

    bool Execute() override;
    bool Undo() override;
    bool Redo() override;
    const char* GetName() const override;

    ShapePtr GetCreatedShape() const;

private:
    Point m_center;
    double m_radius;
    bool m_useCenter;
    ShapePtr m_createdShape;
    bool m_executed;
    OCAFManager* m_ocaf;
};

} // namespace cad_core
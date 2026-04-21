#include "cad_core/ShapeFactory.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <gp_Pln.hxx>

namespace cad_core {

    ShapePtr ShapeFactory::CreateRectangleFace(double width, double height) {
        try {
            // 1. Define an XY plane at the origin
            gp_Pln xyPlane = gp::XOY();

            // 2. Compute the bounding extents of the rectangle (centred at the origin)
            double halfWidth = width / 2.0;
            double halfHeight = height / 2.0;

            // 3. Build the face directly from the plane and its bounds
            BRepBuilderAPI_MakeFace faceMaker(xyPlane, -halfWidth, halfWidth, -halfHeight, halfHeight);

            if (faceMaker.IsDone()) {
                return std::make_shared<Shape>(faceMaker.Shape());
            }
        }
        catch (...) {
            // Exception handling
        }
        return nullptr;
    }

    ShapePtr ShapeFactory::CreateBox(const Point& corner1, const Point& corner2) {
        try {
            BRepPrimAPI_MakeBox boxMaker(corner1.GetOCCTPoint(), corner2.GetOCCTPoint());
            TopoDS_Shape shape = boxMaker.Shape();
            if (boxMaker.IsDone() && !shape.IsNull()) {
                return std::make_shared<Shape>(shape);
            }
        }
        catch (...) {
            // Handle OCCT exceptions
        }
        return nullptr;
    }

    ShapePtr ShapeFactory::CreateBox(double width, double height, double depth) {
        try {
            // Ensure all dimensions are positive
            if (width <= 0 || height <= 0 || depth <= 0) {
                return nullptr;
            }

            BRepPrimAPI_MakeBox boxMaker(width, height, depth);
            TopoDS_Shape shape = boxMaker.Shape();
            if (boxMaker.IsDone() && !shape.IsNull()) {
                return std::make_shared<Shape>(shape);
            }
        }
        catch (...) {
            // Handle OCCT exceptions
        }
        return nullptr;
    }

    ShapePtr ShapeFactory::CreateCylinder(const Point& center, double radius, double height) {
        try {
            // Ensure all dimensions are positive
            if (radius <= 0 || height <= 0) {
                return nullptr;
            }

            gp_Ax2 axis(center.GetOCCTPoint(), gp_Dir(0, 0, 1));
            BRepPrimAPI_MakeCylinder cylMaker(axis, radius, height);
            TopoDS_Shape shape = cylMaker.Shape();
            if (cylMaker.IsDone() && !shape.IsNull()) {
                return std::make_shared<Shape>(shape);
            }
        }
        catch (...) {
            // Handle OCCT exceptions
        }
        return nullptr;
    }

    ShapePtr ShapeFactory::CreateCylinder(double radius, double height) {
        return CreateCylinder(Point(0, 0, 0), radius, height);
    }

    ShapePtr ShapeFactory::CreateSphere(const Point& center, double radius) {
        try {
            // Ensure the radius is positive
            if (radius <= 0) {
                return nullptr;
            }

            BRepPrimAPI_MakeSphere sphereMaker(center.GetOCCTPoint(), radius);
            TopoDS_Shape shape = sphereMaker.Shape();
            if (sphereMaker.IsDone() && !shape.IsNull()) {
                return std::make_shared<Shape>(shape);
            }
        }
        catch (...) {
            // Handle OCCT exceptions
        }
        return nullptr;
    }

    ShapePtr ShapeFactory::CreateSphere(double radius) {
        return CreateSphere(Point(0, 0, 0), radius);
    }

} // namespace cad_core
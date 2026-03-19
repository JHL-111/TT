#pragma once

#include "Shape.h"
#include "Point.h"

namespace cad_core {

class ShapeFactory {
public:
    // 创建一个基于 XY 平面的矩形面 
    static ShapePtr CreateRectangleFace(double width, double height);

    static ShapePtr CreateBox(const Point& corner1, const Point& corner2);
    static ShapePtr CreateBox(double width, double height, double depth);
    
    static ShapePtr CreateCylinder(const Point& center, double radius, double height);
    static ShapePtr CreateCylinder(double radius, double height);
    
    static ShapePtr CreateSphere(const Point& center, double radius);
    static ShapePtr CreateSphere(double radius);
    
private:
    ShapeFactory() = default;
};

} // namespace cad_core
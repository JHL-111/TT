/**
 * @file Shape.cpp
 * @brief Shape类的实现
 * 
 * 这里实现了所有Shape类的功能，主要是调用OpenCASCADE的API
 */

#include "cad_core/Shape.h"
#include <GProp_GProps.hxx>  // 几何属性计算 - OpenCASCADE的瑞士军刀
#include <BRepGProp.hxx>     // 边界表示几何属性 - 专门处理实体几何
#include <TopAbs.hxx>
#include <gp_Pnt.hxx>
namespace cad_core {

/**
 * 默认构造函数 - 创建一个空形状
 * 就像准备一个空盒子，等待装入美妙的几何体
 */
Shape::Shape() {

}

/**
 * 从OpenCASCADE形状构造
 * 这是我们最常用的构造方式，把原生形状"包装"起来
 * @param shape 要包装的OpenCASCADE形状
 */
Shape::Shape(const TopoDS_Shape& shape) : m_shape(shape) {
    // 直接拷贝构造，简单粗暴但有效
    // TODO: 可能需要添加形状有效性检查
}

/**
 * 获取内部的OpenCASCADE形状
 * 当我们需要与OpenCASCADE API直接交互时就用这个
 * @return 内部形状的常量引用
 */
const TopoDS_Shape& Shape::GetOCCTShape() const {
    return m_shape;
}

/**
 * 设置内部形状
 * 给我们的"盒子"换个"内容物"
 * @param shape 新的形状
 */
void Shape::SetOCCTShape(const TopoDS_Shape& shape) {
    m_shape = shape;

}

/**
 * 检查形状是否有效
 * 空的形状就像空气，看不见摸不着
 * @return true表示有效，false表示空的
 */
bool Shape::IsValid() const {
    return !m_shape.IsNull();
}

/**
 * 计算体积
 * 使用OpenCASCADE的几何属性计算功能
 * @return 体积值，如果形状无效则返回0
 */
double Shape::Volume() const {
    if (!IsValid()) {
        // 空形状的体积当然是0，这很科学！
        return 0.0;
    }
    
    // OpenCASCADE的几何属性计算器
    GProp_GProps props;
    
    // 计算体积属性，BRepGProp就是专门干这个的
    BRepGProp::VolumeProperties(m_shape, props);
    
    // Mass()实际返回的是体积，别被名字骗了
    return props.Mass();
    
}

/**
 * 计算表面积
 * 想知道要刷多少漆就用这个函数！
 * @return 表面积值，如果形状无效则返回0
 */
double Shape::Area() const {
    if (!IsValid()) {
        // 没有形状就没有表面，这是常识
        return 0.0;
    }
    
    // 又是熟悉的几何属性计算器
    GProp_GProps props;
    
    // 计算表面积属性
    BRepGProp::SurfaceProperties(m_shape, props);
    
    // 这里的Mass()返回的才是真正的表面积
    return props.Mass();
    

}

/**
 * 计算质心/重心
 * 自动识别形状的维度（3D体积、2D面、1D线）并采取对应的计算策略
 */
gp_Pnt Shape::GetCentroid() const {
    if (!IsValid()) {
        return gp_Pnt(0.0, 0.0, 0.0);
    }

    GProp_GProps props;
    TopAbs_ShapeEnum type = m_shape.ShapeType();

    // 依据形状的拓扑维度，调用对应的属性计算器
    if (type == TopAbs_SOLID || type == TopAbs_COMPSOLID) {
        BRepGProp::VolumeProperties(m_shape, props);
    }
    else if (type == TopAbs_SHELL || type == TopAbs_FACE) {
        BRepGProp::SurfaceProperties(m_shape, props);
    }
    else if (type == TopAbs_WIRE || type == TopAbs_EDGE) {
        BRepGProp::LinearProperties(m_shape, props);
    }
    else {
        // 如果是顶点(Vertex)或不支持的类型，降级返回原点
        return gp_Pnt(0.0, 0.0, 0.0);
    }

    return props.CentreOfMass();
}

} // namespace cad_core
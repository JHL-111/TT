/**
 * 这个类封装了OpenCASCADE的TopoDS_Shape
 */

#pragma once

#include <TopoDS_Shape.hxx>
#include <memory>
#include <gp_Pnt.hxx>

namespace cad_core {

/**
 * @class Shape
 * @brief 几何形状的包装类
 * 
 */
class Shape {
public:
    //默认构造函数 
    Shape();
    
    /** 
     * 从OpenCASCADE形状构造
     * @param shape OpenCASCADE的原生形状
     */
    explicit Shape(const TopoDS_Shape& shape);
    
    /** 虚析构函数 
    virtual ~Shape() = default;

    /** 
     * 获取底层的OpenCASCADE形状 
     * @return
     */
    const TopoDS_Shape& GetOCCTShape() const;
    
    /** 
     * 设置底层形状
     * @param shape 新的OpenCASCADE形状
     */
    void SetOCCTShape(const TopoDS_Shape& shape);
    
    /** 
     * 检查形状是否有效
     * @return true如果形状有效，false如果是个"空壳"
     */
    bool IsValid() const;
    
    /** 
     * 计算体积 
     * @return 体积值，单位取决于你的建模单位
     * TODO: 添加单位处理和错误检查
     */
    double Volume() const;
    
    /** 
     * 计算表面积 
     * @return 表面积值
     * TODO: 对于非封闭形状可能需要特殊处理
     */
    double Area() const;


    /** * 计算几何质心/重心 
     * 自动根据形状类型(实体/面/线)选择正确的计算方法
     * @return 质心坐标点
     */
    gp_Pnt GetCentroid() const;

private:
    /** 存储实际的OpenCASCADE形状 */
    TopoDS_Shape m_shape;
};

/** 智能指针类型别名 */
using ShapePtr = std::shared_ptr<Shape>;

} // namespace cad_core
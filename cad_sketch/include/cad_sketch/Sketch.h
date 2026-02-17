/**
 * @file Sketch.h
 * @brief 草图类 - CAD的"画板"，所有伟大设计的起点！
 * 
 * 这个类是我们2D草图系统的核心，就像艺术家的画板一样，
 * 承载着各种几何元素（点、线、圆、弧等）和约束关系。
 * 
 * 想象一下在纸上画设计图的感觉，只不过我们的"纸"是数字化的，
 * 而且还能自动保证几何关系的正确性！这就是约束求解器的魅力 ✨
 * 
 * TODO: 添加草图的撤销/重做功能
 * TODO: 实现草图的导入/导出功能
 * TODO: 支持草图模板和参数化草图
 * TODO: 添加草图性能统计和优化建议
 */

#pragma once

#include "SketchElement.h"   // 草图元素基类 - 万物之源
#include "SketchPoint.h"     // 点元素 - 最基本的几何要素
#include "SketchLine.h"      // 线元素 - 连接两点的最短路径
#include "SketchCircle.h"    // 圆元素 - 完美的对称之美
#include "SketchArc.h"       // 弧元素 - 圆的一部分，但同样精彩
#include "Constraint.h"      // 约束基类 - 几何关系的守护者
#include "ConstraintSolver.h" // 约束求解器 - 让几何关系保持和谐的魔法师
#include <vector>            // 动态数组 - 容器界的万金油
#include <memory>            // 智能指针 - 内存管理的得力助手
#include <string>            // 字符串 - 人机交流的桥梁

namespace cad_sketch {

/**
 * @class Sketch
 * @brief 草图类 - 2D设计的数字画板
 * 
 * 这个类管理着一个草图中的所有元素和约束关系，
 * 就像一个严格但仁慈的老师，既允许创造自由，
 * 又确保一切都符合几何逻辑 📐
 * 
 * 草图是3D建模的基础，就像房子的地基一样重要！
 */
class Sketch {
public:
    /** 默认构造函数 - 创建一个空白的画板，等待艺术家的创作 */
    Sketch();
    
    /** 
     * 有名字的构造函数 - 给我们的画板起个响亮的名字
     * @param name 草图名称，就像画作的标题一样重要
     */
    Sketch(const std::string& name);
    
    /** 析构函数 - 默认就好，智能指针会帮我们收拾残局 */
    ~Sketch() = default;

    /** 
     * 获取草图名称 - 看看这幅"作品"叫什么
     * @return 草图的名称
     */
    const std::string& GetName() const;
    
    /** 
     * 设置草图名称 - 重新给作品命名
     * @param name 新的名称，要起得有意义哦
     */
    void SetName(const std::string& name);
    
    // ========== 元素管理 - 画板上的"演员"们 ==========
    
    /** 
     * 添加元素 - 在画板上添加新的几何图形
     * @param element 要添加的元素，可以是点、线、圆等
     */
    void AddElement(const SketchElementPtr& element);
    
    /** 
     * 移除元素 - 从画板上擦掉不需要的图形
     * @param element 要移除的元素
     */
    void RemoveElement(const SketchElementPtr& element);
    
    /** 清空所有元素 - 一键清空画板，重新开始 */
    void ClearElements();
    
    /** 
     * 获取所有元素 - 看看画板上都有什么
     * @return 元素列表的常量引用
     */
    const std::vector<SketchElementPtr>& GetElements() const;
    
    /** 
     * 根据ID查找元素 - 在众多元素中找到特定的那一个
     * @param id 元素的唯一标识符
     * @return 找到的元素，如果没找到则返回nullptr
     */
    SketchElementPtr GetElementById(int id) const;
    
    // ========== 约束管理 - 几何关系的"法官" ==========
    
    /** 
     * 添加约束 - 为元素之间建立几何关系
     * @param constraint 约束条件，比如平行、垂直、相等等
     */
    void AddConstraint(const ConstraintPtr& constraint);
    
    /** 
     * 移除约束 - 解除元素间的某种几何关系
     * @param constraint 要移除的约束
     */
    void RemoveConstraint(const ConstraintPtr& constraint);
    
    /** 清空所有约束 - 让所有元素重获"自由" */
    void ClearConstraints();
    
    /** 
     * 获取所有约束 - 查看元素间都有哪些"规则"
     * @return 约束列表的常量引用
     */
    const std::vector<ConstraintPtr>& GetConstraints() const;
    
    // ========== 求解器操作 - 数学魔法的施展 ==========
    
    /** 
     * 求解约束 - 让约束求解器发挥魔法，调整元素位置
     * @return true表示求解成功，false表示约束冲突无法解决
     */
    bool SolveConstraints();
    
    /** 
     * 验证约束 - 检查当前的约束系统是否合理
     * @return true表示约束系统没问题，false表示有冲突
     */
    bool ValidateConstraints() const;
    
    // ========== 选择管理 - 告诉程序用户关注什么 ==========
    
    /** 
     * 选择元素 - 把某个元素标记为"重点关注对象"
     * @param element 要选择的元素
     */
    void SelectElement(const SketchElementPtr& element);
    
    /** 
     * 取消选择元素 - 不再关注某个元素
     * @param element 要取消选择的元素
     */
    void DeselectElement(const SketchElementPtr& element);
    
    /** 清空选择 - 不关注任何元素，回到"佛系"状态 */
    void ClearSelection();
    
    /** 
     * 获取选中的元素 - 看看用户现在关注哪些元素
     * @return 当前选中的元素列表
     */
    std::vector<SketchElementPtr> GetSelectedElements() const;
    
    // ========== 实用工具方法 - 便民小助手 ==========
    
    /** 
     * 检查是否为空 - 看看画板上是不是还是一片空白
     * @return true表示空草图，false表示有内容
     */
    bool IsEmpty() const;
    
    /** 
     * 获取元素数量 - 数数画板上有多少个图形
     * @return 元素的总数
     */
    int GetElementCount() const;
    
    /** 
     * 获取约束数量 - 数数有多少条几何规则
     * @return 约束的总数
     */
    int GetConstraintCount() const;

private:
    /** 草图名称 - 这幅"作品"的标题 */
    std::string m_name;
    
    /** 草图元素集合 - 画板上的所有"演员" */
    std::vector<SketchElementPtr> m_elements;
    
    /** 约束集合 - 维护秩序的"规则条文" */
    std::vector<ConstraintPtr> m_constraints;
    
    /** 约束求解器 - 负责调解元素关系的"和事佬" */
    ConstraintSolver m_solver;
};

/** 草图智能指针类型别名 - 让内存管理变得轻松愉快 */
using SketchPtr = std::shared_ptr<Sketch>;

} // namespace cad_sketch
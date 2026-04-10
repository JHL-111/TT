/**
 * @file Sketch.h
 * @brief 草图类 
 * 
 */

#pragma once

#include "SketchElement.h"   
#include "SketchPoint.h"     
#include "SketchLine.h"      
#include "SketchCircle.h"   
#include "SketchArc.h"       
#include "Constraint.h"      
#include "ConstraintSolver.h"  
#include "cad_sketch/SketchProfile.h"
#include <vector>            
#include <memory>             
#include <string>           
#include <gp_Ax3.hxx>
#include <TopoDS_Wire.hxx>
#include <TopoDS_Face.hxx>


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
    /** 默认构造函数 - 创建一个空白的画板*/
    Sketch();
    
    /** 
     * @param name 草图名称
     */
    Sketch(const std::string& name);
    
    /** 析构函数  */
    ~Sketch() = default;

    /** 
     * 获取草图名称
     * @return 草图的名称
     */
    const std::string& GetName() const;
    
    /** 
     * 设置草图名称
     * @param name 新的名称
     */
    void SetName(const std::string& name);
    
    // ========== 元素管理 ==========
    
    /** 
     * 添加元素 - 在画板上添加新的几何图形
     * @param element 要添加的元素，可以是点、线、圆等
     */
    void AddElement(const SketchElementPtr& element);
    
    /** 清空所有元素  */
    void ClearElements();

    /** 用于撤回  */
    void RemoveElement(const SketchElementPtr& element);

    /** 
     * 获取所有元素 
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
    
    // ========== 求解器操作 ==========
    
    /** 
     * 求解约束 调整元素位置
     * @return true表示求解成功，false表示约束冲突无法解决
     */
    bool SolveConstraints();
    
    /** 
     * 验证约束 - 检查当前的约束系统是否合理
     * @return true表示约束系统没问题，false表示有冲突
     */
    bool ValidateConstraints() const;
    
	/** 获取约束求解器的详细结果 */
    ConstraintSolver* GetConstraintSolver() { return &m_solver; }

    // ========== 选择管理 ==========
    
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
    
    /** 清空选择 - 不关注任何元素 */
    void ClearSelection();
    
    /** 
     * 获取选中的元素 
     * @return 当前选中的元素列表
     */
    std::vector<SketchElementPtr> GetSelectedElements() const;
    
    // ========== 实用工具方法 ==========

    /** 
     * 检查是否为空 
     * @return true表示空草图，false表示有内容
     */
    bool IsEmpty() const;
    
    /** 
     * 获取元素数量 
     * @return 元素的总数
     */
    int GetElementCount() const;
    
    /** 
     * 获取约束数量
     * @return 约束的总数
     */
    int GetConstraintCount() const;

    // ========== 3D 几何生成  ==========

    // 获取当前计算出的所有闭合轮廓 (Profiles)
    std::vector<SketchProfilePtr> GetProfiles() const { return m_profiles; }

    // 核心算法：检测并更新轮廓
    void UpdateProfiles(const gp_Ax3& cs);


    // --- 基准面与坐标系管理 ---
    void SetBaseFace(const TopoDS_Face& face) { m_baseFace = face; }
    TopoDS_Face GetBaseFace() const { return m_baseFace; }

    void SetBaseCS(const gp_Ax3& cs) { m_baseCS = cs; }
    gp_Ax3 GetBaseCS() const { return m_baseCS; }

private:
    /** 草图名称 */
    std::string m_name;
    
    /** 草图元素集合  */
    std::vector<SketchElementPtr> m_elements;
    
    /** 约束集合 */
    std::vector<ConstraintPtr> m_constraints;
    
    /** 约束求解器 */
    ConstraintSolver m_solver;

    /** 闭合轮廓集合 */
    std::vector<SketchProfilePtr> m_profiles;


    TopoDS_Face m_baseFace;
    gp_Ax3 m_baseCS;
};

/** 草图智能指针类型别名 */
using SketchPtr = std::shared_ptr<Sketch>;

} // namespace cad_sketch
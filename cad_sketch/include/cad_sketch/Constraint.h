/**
 * @file Constraint.h
 * @brief 几何约束系统 - 草图参数化建模的核心
 *
 * 约束基类定义了所有几何约束的公共接口。
 * 每个约束子类需要提供：误差函数、雅可比矩阵条目、方程数量。
 * 这些信息被 ConstraintSolver 用于 Newton-Raphson 迭代求解。
 */

#pragma once

#include "SketchElement.h"
#include "SketchPoint.h"
#include <memory>
#include <vector>
#include <string>
#include <map>

namespace cad_sketch {

    enum class ConstraintType {
        Horizontal,
        Vertical,
        Parallel,
        Perpendicular,
        Coincident,
        Distance,
        Angle,
        Radius,
        Diameter,
        Equal,
        Fixed,
        Tangent,
        Symmetric,
        Midpoint
    };

    /**
     * @brief 雅可比矩阵条目 —— 记录某个方程对某个变量的偏导数
     *
     * 求解器在组装雅可比矩阵 J 时使用：J[equationIndex][variableIndex] = derivative
     */
    struct JacobianEntry {
        int variableIndex;   // 变量在全局变量向量中的索引
        double derivative;   // 偏导数值 ∂error/∂variable
    };

    class Constraint {
    public:
        Constraint(ConstraintType type);
        virtual ~Constraint() = default;

        ConstraintType GetType() const;
        int GetId() const;
        void SetId(int id);

        void AddElement(const SketchElementPtr& element);
        const std::vector<SketchElementPtr>& GetElements() const;

        bool IsActive() const;
        void SetActive(bool active);

        // ---------- 原有接口（保持兼容） ----------

        /** 检查约束引用的元素是否合法 */
        virtual bool IsValid() const = 0;

        /** 返回约束的文字描述（UI 显示用） */
        virtual std::string GetDescription() const = 0;

        /** 返回第一个方程的误差值（保持向后兼容） */
        virtual double GetError() const = 0;

        // ---------- Newton-Raphson 求解器需要的新接口 ----------

        /**
         * 这个约束产生几个方程
         * 例如：Horizontal 产生 1 个方程，Coincident 产生 2 个方程
         */
        virtual int GetEquationCount() const = 0;

        /**
         * 返回第 eqIndex 个方程的误差值
         * 默认实现：eqIndex=0 时调用 GetError()
         */
        virtual double GetErrorAt(int eqIndex) const { return GetError(); }

        /**
         * 收集这个约束涉及的所有 SketchPoint 指针
         * 求解器会用这些指针注册变量（每个点有 x, y 两个变量）
         */
        virtual std::vector<SketchPointPtr> GetInvolvedPoints() const = 0;

        /**
         * 返回第 eqIndex 个方程对所有变量的偏导数
         * variableIndex 由求解器在变量注册后分配
         *
         * 子类需要通过 m_variableMap 查找自己的点对应的全局变量索引
         */
        virtual std::vector<JacobianEntry> GetJacobianAt(int eqIndex) const = 0;

        /**
         * 求解器在变量注册后调用此函数，告诉约束它的点对应的全局变量索引
         * key: SketchPoint 指针, value: 该点 x 坐标在全局变量向量中的索引（y = x+1）
         */
        void SetVariableMap(const std::map<SketchPoint*, int>& varMap);

        /** 获取某个点的 x 变量索引，y = x+1 */
        int GetPointVariableIndex(const SketchPointPtr& point) const;

    protected:
        ConstraintType m_type;
        int m_id;
        std::vector<SketchElementPtr> m_elements;
        bool m_active;

        /** 点指针 -> 全局变量索引的映射（由求解器设置） */
        std::map<SketchPoint*, int> m_variableMap;

        static int s_nextId;
    };

    using ConstraintPtr = std::shared_ptr<Constraint>;

} // namespace cad_sketch
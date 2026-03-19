#pragma once

#include "cad_feature/Feature.h"
#include "cad_core/Shape.h"
#include <vector>

namespace cad_feature {

    // 定义布尔操作类型枚举
    enum class BooleanType {
        Union,          // 求和 (并集)
        Intersection,   // 求交 (交集)
        Difference      // 求差 (差集)
    };

    class BooleanFeature : public Feature {
    public:
        BooleanFeature(const std::string& name);
        virtual ~BooleanFeature() = default;

        // 设置与获取布尔类型
        void SetOperationType(BooleanType type);
        BooleanType GetOperationType() const;

        // 设置与获取目标体 (Target Bodies)
        void SetTargets(const std::vector<cad_core::ShapePtr>& targets);
        const std::vector<cad_core::ShapePtr>& GetTargets() const;

        // 设置与获取工具体 (Tool Bodies)
        void SetTools(const std::vector<cad_core::ShapePtr>& tools);
        const std::vector<cad_core::ShapePtr>& GetTools() const;

        // 覆盖基类的虚函数
        cad_core::ShapePtr CreateShape() const override;
        bool ValidateParameters() const override;
        std::shared_ptr<cad_core::ICommand> CreateCommand() const override;

    private:
        BooleanType m_boolType;
        std::vector<cad_core::ShapePtr> m_targets; // 目标体数组
        std::vector<cad_core::ShapePtr> m_tools;   // 工具体数组
    };

    using BooleanFeaturePtr = std::shared_ptr<BooleanFeature>;

} // namespace cad_feature
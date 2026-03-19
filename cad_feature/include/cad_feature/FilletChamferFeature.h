#pragma once

#include "cad_feature/Feature.h"
#include "cad_core/Shape.h"
#include <TopoDS_Edge.hxx>
#include <vector>

namespace cad_feature {

    // 定义操作类型枚举 (Operation Type)
    enum class FCType {
        Fillet, // 圆角
        Chamfer // 倒角
    };

    class FilletChamferFeature : public Feature {
    public:
        FilletChamferFeature(const std::string& name);
        virtual ~FilletChamferFeature() = default;

        // 设置操作类型
        void SetOperationType(FCType type);
        FCType GetOperationType() const;

        // 设置被操作的基础实体 (Base Shape)
        void SetBaseShape(const cad_core::ShapePtr& baseShape);
        const cad_core::ShapePtr& GetBaseShape() const;

        // 设置选中的边 (Selected Edges)
        void SetEdges(const std::vector<TopoDS_Edge>& edges);
        const std::vector<TopoDS_Edge>& GetEdges() const;

        // 设置圆角/倒角参数 (Parameters)
        void SetRadius(double radius);       // 用于 Fillet
        double GetRadius() const;

        void SetDistance1(double distance1); // 用于 Chamfer
        double GetDistance1() const;

        void SetDistance2(double distance2); // 用于 Chamfer
        double GetDistance2() const;

        // 覆盖基类的虚函数
        cad_core::ShapePtr CreateShape() const override;
        bool ValidateParameters() const override;
        std::shared_ptr<cad_core::ICommand> CreateCommand() const override;

    private:
        FCType m_fcType;
        cad_core::ShapePtr m_baseShape;
        std::vector<TopoDS_Edge> m_edges;
    };

    using FilletChamferFeaturePtr = std::shared_ptr<FilletChamferFeature>;

} // namespace cad_feature#pragma once

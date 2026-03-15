#pragma once
#include <TopoDS_Face.hxx>
#include <memory>
#include <vector>
#include "cad_sketch/SketchElement.h"

namespace cad_sketch {

    class SketchProfile {
    public:
        // 构造函数只做声明
        SketchProfile(const TopoDS_Face& face);
        ~SketchProfile() = default;

        // 获取底层的 OCC 面 (Face)
        TopoDS_Face GetFace() const;

        // 预留接口：添加边界元素 (Boundary Elements)
        void AddBoundaryElement(const SketchElementPtr& element);
        std::vector<SketchElementPtr> GetBoundaryElements() const;

    private:
        TopoDS_Face m_face; // 闭合轮廓生成的平面
        std::vector<SketchElementPtr> m_boundaryElements; // 构成该轮廓的草图元素集合
    };

    using SketchProfilePtr = std::shared_ptr<SketchProfile>;

} // namespace cad_sketch

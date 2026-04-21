#pragma once
#include <TopoDS_Face.hxx>
#include <memory>
#include <vector>
#include "cad_sketch/SketchElement.h"

namespace cad_sketch {

    class SketchProfile {
    public:
        // Constructor takes a face only
        SketchProfile(const TopoDS_Face& face);
        ~SketchProfile() = default;

        // Get the underlying OCC Face
        TopoDS_Face GetFace() const;

        // Reserved interface: add boundary elements
        void AddBoundaryElement(const SketchElementPtr& element);
        std::vector<SketchElementPtr> GetBoundaryElements() const;

    private:
        TopoDS_Face m_face;                                   // Planar face formed by the closed contour
        std::vector<SketchElementPtr> m_boundaryElements;     // Sketch elements that make up this profile
    };

    using SketchProfilePtr = std::shared_ptr<SketchProfile>;

} // namespace cad_sketch
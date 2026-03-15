#include "cad_sketch/SketchProfile.h"

namespace cad_sketch {

    SketchProfile::SketchProfile(const TopoDS_Face& face)
        : m_face(face) {
       
    }

    TopoDS_Face SketchProfile::GetFace() const {
        return m_face;
    }

    void SketchProfile::AddBoundaryElement(const SketchElementPtr& element) {
        if (element) {
            m_boundaryElements.push_back(element);
        }
    }

    std::vector<SketchElementPtr> SketchProfile::GetBoundaryElements() const {
        return m_boundaryElements;
    }

} // namespace cad_sketch
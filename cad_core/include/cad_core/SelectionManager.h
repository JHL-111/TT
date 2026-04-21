#pragma once

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <V3d_View.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <vector>
#include <memory>

#include "cad_core/Shape.h"

namespace cad_core {

    enum class SelectionMode {
        Shape = 0,    // Entire shape
        Face = 4,     // Face
        Edge = 2,     // Edge
        Vertex = 1    // Vertex
    };

    struct SelectionInfo {
        ShapePtr shape;
        TopoDS_Shape subShape;
        TopAbs_ShapeEnum shapeType;
        int index;

        SelectionInfo() : index(-1), shapeType(TopAbs_SHAPE) {}
        SelectionInfo(ShapePtr s, const TopoDS_Shape& sub, TopAbs_ShapeEnum type, int idx)
            : shape(s), subShape(sub), shapeType(type), index(idx) {
        }
    };

    class SelectionManager {
    public:
        SelectionManager();
        ~SelectionManager();

        // Set interactive context and view
        void SetContext(Handle(AIS_InteractiveContext) context);
        void SetView(Handle(V3d_View) view);

        // Selection mode
        void SetSelectionMode(SelectionMode mode);
        SelectionMode GetSelectionMode() const { return m_currentMode; }

        // Selection operations
        void StartSelection(int x, int y);
        void UpdateSelection(int x, int y);
        void EndSelection(int x, int y);

        // Multi-selection
        void StartMultiSelection(int x, int y);
        void AddToSelection(int x, int y);
        void RemoveFromSelection(int x, int y);

        // Get selection results
        std::vector<SelectionInfo> GetSelectedShapes() const;
        std::vector<SelectionInfo> GetSelectedFaces() const;
        std::vector<SelectionInfo> GetSelectedEdges() const;
        std::vector<SelectionInfo> GetSelectedVertices() const;

        // Clear selection
        void ClearSelection();

        // Check whether anything is selected
        bool HasSelection() const;
        size_t GetSelectionCount() const;

        // Highlight
        void HighlightShape(const Handle(AIS_Shape)& shape, bool highlight = true);
        void HighlightAll(bool highlight = true);

        // Selection filters
        void EnableShapeSelection(bool enable = true);
        void EnableFaceSelection(bool enable = true);
        void EnableEdgeSelection(bool enable = true);
        void EnableVertexSelection(bool enable = true);

    private:
        Handle(AIS_InteractiveContext) m_context;
        Handle(V3d_View) m_view;
        SelectionMode m_currentMode;
        std::vector<SelectionInfo> m_selectedItems;

        // Private methods
        void UpdateSelectionMode();
        SelectionInfo CreateSelectionInfo(const Handle(AIS_Shape)& aisShape, int subShapeIndex = -1);
        TopoDS_Shape GetSubShape(const TopoDS_Shape& shape, TopAbs_ShapeEnum type, int index);
        int GetSubShapeIndex(const TopoDS_Shape& shape, const TopoDS_Shape& subShape, TopAbs_ShapeEnum type);
    };

} // namespace cad_core
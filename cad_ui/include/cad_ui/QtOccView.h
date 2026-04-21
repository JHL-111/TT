#pragma once

#include <QWidget>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QTimer>
#include <map>
#include <memory>
#include <vector>
#include <TopoDS_Face.hxx>
#include <gp_Ax3.hxx>
#include <V3d_View.hxx>
#include <V3d_Viewer.hxx>
#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>
#include <AIS_ViewController.hxx>
#include <Graphic3d_GraphicDriver.hxx>
#include <AIS_InteractiveObject.hxx>
#include <StdSelect_BRepOwner.hxx>

#include "cad_feature/Feature.h"
#include "cad_core/Shape.h"
#include "cad_core/SelectionManager.h"
#include "cad_sketch/Sketch.h"
#include "cad_sketch/SketchLine.h"
#include "cad_sketch/SnappingManager.h"

namespace cad_ui {

    enum class SweepInteractionMode {
        None,
        SelectingProfile,
        PreviewingPathPlane  // Rotating the sweep path reference plane to follow the mouse
    };

    class QtOccView : public QWidget, protected AIS_ViewController {
        Q_OBJECT

    public:
        explicit QtOccView(QWidget* parent = nullptr);
        ~QtOccView() = default;

        // Viewer initialisation
        bool InitViewer();

        // View operations
        void FitAll();
        void ZoomIn();
        void ZoomOut();
        void Pan(int dx, int dy);
        void Rotate(int dx, int dy);

        // View modes
        void SetViewMode(const QString& mode);
        void SetProjectionMode(bool orthographic);

        // Shape display
        void DisplayShape(const cad_core::ShapePtr& shape);
        void RemoveShape(const cad_core::ShapePtr& shape);
        void RemoveSketch(const std::shared_ptr<cad_sketch::Sketch>& sketch);
        void ClearShapes();
        void RedrawAll();

        // Toggle shape visibility
        void SetShapeVisibility(const cad_core::ShapePtr& shape, bool visible);
        virtual QPaintEngine* paintEngine() const;

        // Background and appearance
        void SetBackgroundColor(const QColor& color);
        void SetBackgroundGradient(const QColor& color1, const QColor& color2);

        // Selection
        void SetSelectionMode(int mode);
        void SetSelectionMode(cad_core::SelectionMode mode);
        void ClearSelection();
        void SelectShape(const cad_core::ShapePtr& shape);
        // Enable or disable automatic multi-selection mode (no Shift key required)
        void SetMultiSelectionMode(bool multi);

        // Edge selection for operations
        void ClearEdgeSelection();
        std::vector<TopoDS_Edge> GetSelectedTopoEdges() const { return m_selectedEdges; }
        std::map<cad_core::ShapePtr, std::vector<TopoDS_Edge>> GetSelectedEdgesByShape() const;
        void HighlightEdge(const TopoDS_Edge& edge);
        void HighlightVertex(const TopoDS_Vertex& vertex);
        void HighlightFace(const TopoDS_Face& face);
        void UnhighlightAllEdges();
        void UnhighlightAllVertices();
        void UnhighlightAllFaces();

        // Advanced selection methods
        std::vector<cad_core::SelectionInfo> GetSelectedShapes() const;
        std::vector<cad_core::SelectionInfo> GetSelectedFaces() const;
        std::vector<cad_core::SelectionInfo> GetSelectedEdges() const;
        std::vector<cad_core::SelectionInfo> GetSelectedVertices() const;

        // Selection manager access
        cad_core::SelectionManager* GetSelectionManager() { return m_selectionManager.get(); }

        // View access
        Handle(V3d_View) GetView() const { return m_view; }
        Handle(AIS_InteractiveContext) GetContext() const { return m_context; }

        // Grid
        void ShowGrid(bool show);
        void SetGridSpacing(double spacing);

        // Axes
        void ShowAxes(bool show);

        // Transparency control
        void SetAllTransparency(double transparency);
        void SetShapeTransparency(const cad_core::ShapePtr& shape, double transparency);
        cad_core::ShapePtr GetCurrentSelectedShape() const;

        // Sketch mode support
        bool IsInSketchMode() const;
        void EnterSketchMode(const TopoDS_Face& face);
        void EnterSketchMode(const gp_Ax3& customCS);
        void ExitSketchMode();
        void EditSketch(const std::shared_ptr<cad_sketch::Sketch>& sketch);
        bool HasActiveSketchTool() const;
        void StopSketchTool();
        void UndoSketch();
        void RedoSketch();
        bool CanUndoSketch() const;
        bool CanRedoSketch() const;
        void StartRectangleTool();
        void StartPointTool();
        void StartLineTool();
        void StartCircleTool();
        void StartArcTool();
        void StartCurveTool();

        // Sketch preview and rendering interface
        void ShowSketchPreviewElements(const std::vector<cad_sketch::SketchElementPtr>& elements, const gp_Ax3& sketchCS);
        void ClearSketchPreview();
        void AddSketchElements(const std::vector<cad_sketch::SketchElementPtr>& elements, const gp_Ax3& sketchCS);
        void ClearSketchObjects();
        void SetSketchVisibility(const std::shared_ptr<cad_sketch::Sketch>& sketch, bool visible);

        // Show/hide the snap indicator
        void ShowSnapIndicator(const gp_Pnt& pnt, cad_sketch::SnapType snapType);
        void HideSnapIndicator();

        // Sketch reference face highlight
        void HighlightSketchFace(const TopoDS_Face& face);
        void ClearSketchFaceHighlight();

        // Sketch closed profile rendering
        void RenderSketchProfiles(const std::vector<cad_sketch::SketchProfilePtr>& profiles);
        void ClearSketchProfiles();

        // Draw/clear the centroid indicator
        void DrawCentroid(const gp_Pnt& pnt);
        void ClearCentroid();

        // Formal sweep execution interface called by the UI panel
        cad_core::ShapePtr GetSweepPathShape();
        cad_core::ShapePtr GetSweepProfileShape() const { return m_currentSelectedShape; }
        void CleanupSweepUI();
        void StartSweepInteraction();
        void CancelSweepInteraction();
        void ToggleSweepPathTool(bool enableDrawing);

        // Get selected sketch elements
        std::vector<cad_sketch::SketchElementPtr> GetSelectedSketchElements();

        void RemoveSketchElements(const std::vector<cad_sketch::SketchElementPtr>& elements); // Remove from view
        void ClearSketchElementMap(); // Clear the element-to-AIS mapping table

        // Hide a specific sketch profile
        void HideSingleSketchProfile(const cad_core::ShapePtr& profileShape);

        // Refresh the visual representation of a sketch element
        void UpdateSketchElementVisuals(const cad_sketch::SketchElementPtr& elem);

        // Get the currently active sketch
        std::shared_ptr<cad_sketch::Sketch> GetActiveSketch() const;

        // Get the reference face of the current sketch
        TopoDS_Face GetSketchFace() const;

        // Get the local coordinate system of the current sketch
        gp_Ax3 GetSketchCS() const;

        // Get the actual selected sub-shape (e.g. the specific face in face-selection mode)
        TopoDS_Shape GetSelectedSubShape() const;


    signals:
        void ShapeSelected(const cad_core::ShapePtr& shape);
        void FaceSelected(const TopoDS_Face& face);
        void ViewChanged();
        void SketchModeEntered();
        void SketchModeExited();
        void SketchHistoryChanged();
        void MousePositionChanged(int x, int y);
        void Mouse3DPositionChanged(double x, double y, double z);
        void SketchToolChanged(const QString& toolName);


    protected:
        void paintEvent(QPaintEvent* event) override;
        void resizeEvent(QResizeEvent* event) override;
        void mousePressEvent(QMouseEvent* event) override;
        void mouseMoveEvent(QMouseEvent* event) override;
        void mouseReleaseEvent(QMouseEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;
        void keyReleaseEvent(QKeyEvent* event) override;
        void wheelEvent(QWheelEvent* event) override;
        void focusInEvent(QFocusEvent* event) override;
        void focusOutEvent(QFocusEvent* event) override;
        void enterEvent(QEvent* event) override;
        void leaveEvent(QEvent* event) override;
        void showEvent(QShowEvent* event) override;
        void changeEvent(QEvent* event) override;

    private:
        Handle(V3d_Viewer) m_viewer;
        Handle(V3d_View) m_view;
        Handle(AIS_InteractiveContext) m_context;
        Handle(Graphic3d_GraphicDriver) m_driver;
        Handle(AIS_InteractiveObject) m_previewFaceAIS;

        QPoint m_lastMousePos;
        QPoint m_leftPressPos;  // Records the position where the left button was pressed
        Qt::MouseButton m_currentMouseButton;
        bool m_isInitialized;
        bool m_isLeftDragging = false;  // Whether the left button has entered drag-rotate state

        QTimer* m_redrawTimer;

        // Selection manager
        std::unique_ptr<cad_core::SelectionManager> m_selectionManager;

        // Shape-to-AIS mapping for selection synchronisation
        std::map<cad_core::ShapePtr, Handle(AIS_Shape)> m_shapeToAIS;

        // Current selection state (single-selection mode)
        cad_core::ShapePtr m_currentSelectedShape;
        Handle(AIS_Shape) m_currentSelectedAIS;

        bool m_multiSelectionMode = false;

        // Edge selection state for fillet/chamfer and similar operations
        std::vector<TopoDS_Edge> m_selectedEdges;
        std::vector<Handle(AIS_InteractiveObject)> m_highlightedEdges;
        std::vector<cad_core::ShapePtr> m_edgeParentShapes;  // Tracks the parent shape of each edge (same index as m_selectedEdges)

        // Vertex highlight state
        std::vector<TopoDS_Vertex> m_selectedVertices;
        std::vector<Handle(AIS_InteractiveObject)> m_highlightedVertices;

        // Face highlight state
        std::vector<TopoDS_Face> m_selectedFaces;
        std::vector<Handle(AIS_InteractiveObject)> m_highlightedFaces;

        // Sketch mode
        std::unique_ptr<class SketchMode> m_sketchMode;
        // Preview objects (cyan)
        std::vector<Handle(AIS_InteractiveObject)> m_sketchPreviewObjects;
        // Committed sketch objects (yellow)
        std::vector<Handle(AIS_InteractiveObject)> m_sketchObjects;
        // Highlighted sketch face
        Handle(AIS_InteractiveObject) m_highlightedFace;
        // Snap indicator (small circle)
        Handle(AIS_InteractiveObject) m_snapIndicator;
        // Centroid indicator (small red sphere)
        Handle(AIS_InteractiveObject) m_CentroidActor;
        // Sketch element to AIS object mapping (for selection synchronisation)
        std::map<Handle(AIS_InteractiveObject), cad_sketch::SketchElementPtr> m_sketchElementMap;
        // Sketch profile mapping
        std::map<Handle(AIS_InteractiveObject), cad_core::ShapePtr> m_sketchProfileMap;
        // Current selection mode
        int m_currentSelectionMode;
        // Closed sketch profile AIS objects
        std::vector<Handle(AIS_InteractiveObject)> m_sketchProfileObjects;
        // Temporary selection highlight layer for sketch elements
        Handle(AIS_Shape) m_sketchHighlightAIS;
        std::vector<Handle(AIS_InteractiveObject)> m_sketchHighlightList;
        void UnhighlightSketchElement();

        void InitializeOCC();
        void RedrawView();

        void HandleSelection(const QPoint& point);

        // Selection logic split functions
        void ClearPreviousSelectionState();
        void ProcessEdgeSelection();
        void ProcessVertexSelection();
        void ProcessFaceSelection();
        void ProcessShapeOrSketchSelection();

        // OCC selection object extraction utilities
        Handle(AIS_InteractiveObject) GetFirstSelectedObject() const;
        Handle(StdSelect_BRepOwner) GetFirstSelectedOwner() const;

        // Sweep dynamic plane state variables
        SweepInteractionMode m_sweepInteractionState = SweepInteractionMode::None;
        bool m_isDrawingSweepPath = false;
        gp_Pnt m_sweepCentroid;              // Locked centroid
        gp_Dir m_sweepProfileNormal;         // Original profile normal
        gp_Ax3 m_currentSweepPathCS;         // Candidate path coordinate system (updated in real time)
        gp_Ax3 m_baseSweepPathCS;            // Base coordinate system at rotation angle 0
        Handle(AIS_InteractiveObject) m_sweepPlanePreview; // Semi-transparent preview face

    public slots:
        void ShowOpFace(int faceIndex);
        void ClearOpFace();

    private slots:
        void OnRedrawTimer();

    };

} // namespace cad_ui
#ifndef SKETCHMODE_H
#define SKETCHMODE_H

#include <QObject>
#include <QWidget>
#include <QMouseEvent>
#include <QKeyEvent>
#include <vector>
#include <memory>
#include <string>

#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <gp_Ax3.hxx>
#include <TopoDS_Face.hxx>
#include <V3d_View.hxx>
#include <Graphic3d_Camera.hxx>

#include "cad_core/Shape.h"
#include "cad_sketch/Sketch.h"
#include "cad_sketch/SketchElement.h"
#include "cad_sketch/SketchLine.h"
#include "cad_sketch/SketchCircle.h"
#include "cad_sketch/SketchArc.h"
#include "cad_sketch/SketchCurve.h"
#include "cad_sketch/SnappingManager.h"

namespace cad_ui {

    class QtOccView;

    /**
     * @class SketchToolBase
     * @brief Abstract base class for sketch drawing tools
     *
     * Uses polymorphism to manage the common behaviour of all drawing tools
     * (line, rectangle, circle, etc.) uniformly.
     */
    class SketchToolBase : public QObject {
        Q_OBJECT
    public:
        explicit SketchToolBase(QObject* parent = nullptr) : QObject(parent), m_isDrawing(false) {}
        virtual ~SketchToolBase() = default;

        virtual void StartDrawing(const QPoint& startPoint) = 0;
        virtual void UpdateDrawing(const QPoint& currentPoint) = 0;
        virtual void FinishDrawing(const QPoint& endPoint) = 0;
        virtual void CancelDrawing() = 0;
        virtual void HoverMove(const QPoint& currentPoint);

        bool IsDrawing() const { return m_isDrawing; }

        // Pass the sketch plane and view to the tool
        void SetSketchPlane(const gp_Pln& plane) { m_sketchPlane = plane; }
        void SetView(Handle(V3d_View) view) { m_view = view; }

        // Inject snapping context (manager and existing sketch elements)
        void SetSnappingContext(cad_sketch::SnappingManager* manager, const std::vector<cad_sketch::SketchElementPtr>* elements) {
            m_snappingManager = manager;
            m_existingElements = elements;
        }

    signals:
        // Dynamic preview signal during drawing
        void previewUpdated(const std::vector<cad_sketch::SketchElementPtr>& elements);
        // Confirmation signal when drawing is complete
        void elementsCreated(const std::vector<cad_sketch::SketchElementPtr>& elements);
        // Drawing cancelled signal
        void drawingCancelled();
        // Snap indicator signals
        void snapPointDetected(const gp_Pnt& pnt, cad_sketch::SnapType snapType);
        void snapPointLost();

    protected:
        // Convert 2D screen mouse coordinates to a 3D point on the sketch plane
        gp_Pnt ScreenToSketchPlane(const QPoint& screenPoint);

        // Get the snapped 2D coordinate (for actual drawing — snap type not needed)
        bool GetSnappedCoordinate(const QPoint& screenPoint, Standard_Real& u, Standard_Real& v);
        // Get the snapped 2D coordinate with snap type (for hover/tooltip use)
        bool GetSnappedCoordinate(const QPoint& screenPoint, Standard_Real& u, Standard_Real& v, cad_sketch::SnapType& outSnapType);

        bool m_isDrawing;            // Whether drawing is currently in progress
        gp_Pln m_sketchPlane;        // The sketch plane currently attached to
        Handle(V3d_View) m_view;     // OCC view handle

        // Snapping-related member variables
        cad_sketch::SnappingManager* m_snappingManager = nullptr;
        const std::vector<cad_sketch::SketchElementPtr>* m_existingElements = nullptr;
    };

    /**
     * @class SketchRectangleTool
     * @brief Rectangle drawing tool
     */
    class SketchRectangleTool : public SketchToolBase {
        Q_OBJECT
    public:
        explicit SketchRectangleTool(QObject* parent = nullptr);

        void StartDrawing(const QPoint& startPoint) override;
        void UpdateDrawing(const QPoint& currentPoint) override;
        void FinishDrawing(const QPoint& endPoint) override;
        void CancelDrawing() override;

    private:
        QPoint m_startPoint;   // Start point in screen coordinates
        QPoint m_currentPoint; // Current mouse position in screen coordinates
        std::vector<cad_sketch::SketchElementPtr> m_currentElements; // Elements currently being drawn

        // Receives pre-computed 2D UV coordinates directly
        std::vector<cad_sketch::SketchElementPtr> CreateRectangleLines(Standard_Real u1, Standard_Real v1, Standard_Real u2, Standard_Real v2);
    };

    /**
     * @class SketchPointTool
     * @brief Point drawing tool
     * A single click is sufficient to create a point.
     */
    class SketchPointTool : public SketchToolBase {
        Q_OBJECT
    public:
        explicit SketchPointTool(QObject* parent = nullptr);

        void StartDrawing(const QPoint& startPoint) override;
        void UpdateDrawing(const QPoint& currentPoint) override;
        void FinishDrawing(const QPoint& endPoint) override;
        void CancelDrawing() override;

    private:
        std::vector<cad_sketch::SketchElementPtr> m_currentElements;
    };

    /**
     * @class SketchLineTool
     * @brief Line drawing tool
     */
    class SketchLineTool : public SketchToolBase {
        Q_OBJECT
    public:
        explicit SketchLineTool(QObject* parent = nullptr);

        void StartDrawing(const QPoint& startPoint) override;
        void UpdateDrawing(const QPoint& currentPoint) override;
        void FinishDrawing(const QPoint& endPoint) override;
        void CancelDrawing() override;

    private:
        QPoint m_startPoint;
        std::vector<cad_sketch::SketchElementPtr> m_currentElements;
    };

    /**
     * @class SketchCircleTool
     * @brief Circle drawing tool
     */
    class SketchCircleTool : public SketchToolBase {
        Q_OBJECT
    public:
        explicit SketchCircleTool(QObject* parent = nullptr);

        void StartDrawing(const QPoint& startPoint) override;
        void UpdateDrawing(const QPoint& currentPoint) override;
        void FinishDrawing(const QPoint& endPoint) override;
        void CancelDrawing() override;

    private:
        QPoint m_centerPoint;
        std::vector<cad_sketch::SketchElementPtr> m_currentElements;
    };

    /**
     * @class SketchArcTool
     * @brief Arc drawing tool
     * Uses a three-point interaction:
     *   centre → start point (sets radius and start angle) → end point (sets end angle)
     */
    class SketchArcTool : public SketchToolBase {
        Q_OBJECT
    public:
        explicit SketchArcTool(QObject* parent = nullptr);

        void StartDrawing(const QPoint& startPoint) override;
        void UpdateDrawing(const QPoint& currentPoint) override;
        void FinishDrawing(const QPoint& endPoint) override;
        void CancelDrawing() override;

        // Override to support hover preview (mouse not pressed)
        void HoverMove(const QPoint& currentPoint) override;

    private:
        // Interaction states
        enum State { Init, CenterSet, StartSet };
        State m_state;

        QPoint m_centerPoint;
        QPoint m_startPoint;
        std::vector<cad_sketch::SketchElementPtr> m_currentElements;
    };

    /**
     * @class SketchCurveTool
     * @brief Spline curve drawing tool
     * Supports adding nodes by successive clicks; right-click or Enter to finish.
     */
    class SketchCurveTool : public SketchToolBase {
        Q_OBJECT
    public:
        explicit SketchCurveTool(QObject* parent = nullptr);

        void StartDrawing(const QPoint& startPoint) override;
        void UpdateDrawing(const QPoint& currentPoint) override;
        void FinishDrawing(const QPoint& endPoint) override;
        void CancelDrawing() override;
        void HoverMove(const QPoint& currentPoint) override;

        // Confirm and finish drawing
        void ConfirmDrawing();

        // Force-inject a start point (used by Sweep to lock the centroid)
        void InjectStartPoint(double u, double v);

    private:
        std::vector<cad_sketch::SketchPointPtr> m_points;          // Clicked points so far
        std::vector<cad_sketch::SketchElementPtr> m_currentElements; // Current preview elements
    };

    /**
     * @class SketchMode
     * @brief Sketch mode manager
     *
     * Manages entering/exiting sketch mode, view switching, and
     * dispatching events to the currently active drawing tool.
     */
    class SketchMode : public QObject {
        Q_OBJECT
    public:
        explicit SketchMode(QtOccView* viewer, QObject* parent = nullptr);
        ~SketchMode() = default;

        // Sketch lifecycle management
        bool EnterSketchMode(const TopoDS_Face& face); // Enter sketch mode on a face
        bool EnterSketchMode(const gp_Ax3& customCS);  // Enter sketch mode using a pure mathematical CS
        void ExitSketchMode();                         // Exit sketch mode
        // Edit an existing sketch
        bool EditSketch(const std::shared_ptr<cad_sketch::Sketch>& sketch);
        bool IsInSketchMode() const { return m_isActive; }
        bool HasActiveTool() const { return m_currentTool != nullptr; }
        void Undo();
        void Redo();
        bool CanUndo() const { return !m_undoStack.empty(); }
        bool CanRedo() const { return !m_redoStack.empty(); }

        // Access saved sketch context data
        const cad_sketch::SketchPtr& GetCurrentSketch() const { return m_currentSketch; }
        const TopoDS_Face& GetSketchFace() const { return m_sketchFace; }

        // Sketch local coordinate system
        const gp_Ax3& GetSketchCS() const { return m_sketchCS; }
        const gp_Pln& GetSketchPlane() const { return m_sketchPlane; }
        const gp_Ax3& GetSketchCoordinateSystem() const { return m_sketchCS; }

        // Temporary 3D view control
        void StartTemporary3DView();
        void StopTemporary3DView();
        bool IsTemporary3DViewActive() const { return m_isTemporary3DView; }

        // Drawing tool control
        void StartRectangleTool();
        void StartPointTool();
        void StartLineTool();
        void StartCircleTool();
        void StartArcTool();
        void StartCurveTool();
        void StopCurrentTool();
        SketchToolBase* GetCurrentTool() const { return m_currentTool.get(); }

        // Event handling
        void HandleMousePress(QMouseEvent* event);
        void HandleMouseMove(QMouseEvent* event);
        void HandleMouseRelease(QMouseEvent* event);
        void HandleKeyPress(QKeyEvent* event);


    signals:
        void sketchModeEntered();
        void sketchModeExited();
        void sketchElementCreated(cad_sketch::SketchElementPtr element);
        void sketchHistoryChanged(); // Notifies UI to update Undo/Redo button state
        void statusMessageChanged(const QString& message);
        void toolChanged(const QString& toolName);

    private slots:
        // Receive signals from tools and bridge them to the renderer for display
        void OnPreviewUpdated(const std::vector<cad_sketch::SketchElementPtr>& elements);
        void OnElementsCreated(const std::vector<cad_sketch::SketchElementPtr>& elements);
        void OnDrawingCancelled();
        void OnSnapPointDetected(const gp_Pnt& pnt, cad_sketch::SnapType snapType);
        void OnSnapPointLost();


    private:
        QtOccView* m_viewer; // Pointer to the view renderer
        bool m_isActive;     // Whether sketch mode is currently active

        // Tracks whether the mouse actually moved (distinguishes "click" from "drag")
        bool m_didActuallyMove = false;

        // Core sketch data model
        cad_sketch::SketchPtr m_currentSketch; // Stores drawn lines and constraints
        TopoDS_Face m_sketchFace;              // Attached 3D topological face
        gp_Pln m_sketchPlane;                  // Extracted mathematical plane
        gp_Ax3 m_sketchCS;                     // Local coordinate system (LCS)

        // Helper for auto-constraining rectangles
        void AutoConstrainRectangle(const std::vector<cad_sketch::SketchElementPtr>& elements);

        // Drag state variables
        bool m_isDragging = false;          // Whether a drag is in progress
        double m_lastDragU = 0.0;           // U (X) coordinate from the previous frame
        double m_lastDragV = 0.0;           // V (Y) coordinate from the previous frame
        std::vector<cad_sketch::SketchElementPtr> m_draggedElements;    // Elements being dragged
        std::vector<cad_sketch::SketchElementPtr> m_dragStartSnapshot;  // State snapshot before drag (for Undo)

        // Rotation state variables
        bool m_isRotating = false;
        bool m_isFirstRotation = false;
        double m_rotCenterU = 0.0;
        double m_rotCenterV = 0.0;
        double m_lastAngle = 0.0;

        // Helper: compute the centroid of the selected elements
        void CalculateSelectionCenter(double& outU, double& outV);

        // History record structure
        struct SketchHistoryStep {
            enum ActionType { ADD, REMOVE };
            ActionType type;
            std::vector<cad_sketch::SketchElementPtr> elements;
        };
        std::vector<SketchHistoryStep> m_undoStack;
        std::vector<SketchHistoryStep> m_redoStack;
        void RefreshSketchView();
        void DeleteSelectedElements();

        // Viewport state saved for restoring the original view on exit
        gp_Pnt m_savedEye;     // Camera position
        gp_Pnt m_savedAt;      // Look-at target point
        gp_Dir m_savedUp;      // Camera up vector
        double m_savedScale;   // Zoom scale
        Graphic3d_Camera::Projection m_savedProjectionType; // Projection type (perspective or orthographic)

        // Variables for storing the orthographic sketch view
        gp_Pnt m_tempSketchEye;
        gp_Pnt m_tempSketchAt;
        gp_Dir m_tempSketchUp;
        Standard_Real m_tempSketchScale;
        Graphic3d_Camera::Projection m_tempSketchProj;

        bool m_isTemporary3DView = false; // Whether Spacebar is currently held (temporary 3D view)

        // Polymorphic smart pointer managing the currently active tool
        std::unique_ptr<SketchToolBase> m_currentTool;

        // Snapping manager
        cad_sketch::SnappingManager m_snappingManager;

        // Internal helper methods
        void SetupSketchPlane(const TopoDS_Face& face);
        void SetupSketchView();
        void RestoreView();
        void CreateSketchCoordinateSystem();
        // Coordinate conversion helpers
        void GetPlaneCoordinate(const QPoint& screenPos, double& u, double& v);
        gp_Pln ExtractPlaneFromFace(const TopoDS_Face& face);
    };

} // namespace cad_ui

#endif // SKETCHMODE_H
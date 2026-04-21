#include "cad_ui/SketchMode.h"
#include "cad_ui/QtOccView.h"
#include "cad_sketch/SketchPoint.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDebug>
#include <cad_sketch/Concreteconstraints.h>
#include <BRep_Tool.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <Geom_Plane.hxx>
#include <cmath>
#include <ElSLib.hxx>
#include <Standard_ErrorHandler.hxx>
#include <Precision.hxx>
#include <gp_Lin.hxx>
#include <IntAna_IntConicQuad.hxx>
#include <BRepTools.hxx>
#include <GeomLProp_SLProps.hxx>

namespace cad_ui {

    // =============================================================================
    // SketchToolBase Implementation 
    // =============================================================================
    gp_Pnt SketchToolBase::ScreenToSketchPlane(const QPoint& screenPoint) {
        if (m_view.IsNull()) return gp_Pnt(0, 0, 0);

        Standard_Real X, Y, Z, dX, dY, dZ;
        // Convert 2D screen coordinates to a ray in 3D space
        m_view->ConvertWithProj(screenPoint.x(), screenPoint.y(), X, Y, Z, dX, dY, dZ);

        gp_Pnt rayOrigin(X, Y, Z);
        gp_Dir rayDir(dX, dY, dZ);
        gp_Lin ray(rayOrigin, rayDir);

        // Calculate the analytical intersection point of the ray and the sketch plane 
        IntAna_IntConicQuad intersection(ray, m_sketchPlane, Precision::Angular(), Precision::Confusion());
        if (intersection.IsDone() && intersection.NbPoints() > 0) {
            return intersection.Point(1);
        }
        return gp_Pnt(0, 0, 0);
    }



    // 1. Three-parameter overload: call the four-parameter version and ignore the snap type

    bool SketchToolBase::GetSnappedCoordinate(const QPoint& screenPoint, Standard_Real& u, Standard_Real& v) {
        cad_sketch::SnapType dummyType;
        return GetSnappedCoordinate(screenPoint, u, v, dummyType);
    }

    // 2. Four-parameter snap function that also returns the snap type

    bool SketchToolBase::GetSnappedCoordinate(const QPoint& screenPoint, Standard_Real& u, Standard_Real& v, cad_sketch::SnapType& outSnapType) {
        gp_Pnt p3d = ScreenToSketchPlane(screenPoint);
        ElSLib::Parameters(m_sketchPlane, p3d, u, v);

        if (m_snappingManager && m_existingElements) {
            cad_core::Point inputPt(u, v, 0);
            cad_sketch::SnapResult snapRes = m_snappingManager->FindSnapPoint(inputPt, *m_existingElements);

            if (snapRes.found) {
                u = snapRes.snapPoint.X();
                v = snapRes.snapPoint.Y();
                outSnapType = snapRes.type;
                return true;
            }
        }
        return false;
    }

    void SketchToolBase::HoverMove(const QPoint& currentPoint) {
        Standard_Real u, v;
        cad_sketch::SnapType snapType; // Prepare a variable to receive the snap type


        // Call the snap function

        if (GetSnappedCoordinate(currentPoint, u, v, snapType)) {
            gp_Pnt p3d = m_sketchPlane.Location().Translated(
                gp_Vec(m_sketchPlane.XAxis().Direction()) * u +
                gp_Vec(m_sketchPlane.YAxis().Direction()) * v
            );
            // Emit both the snapped point and snap type together

            emit snapPointDetected(p3d, snapType);
        }
        else {
            emit snapPointLost();
        }
    }

    // =============================================================================
    // SketchRectangleTool Implementation (Rectangle tool implementation)

    // =============================================================================
    SketchRectangleTool::SketchRectangleTool(QObject* parent) : SketchToolBase(parent) {}

    void SketchRectangleTool::StartDrawing(const QPoint& startPoint) {
        m_isDrawing = true;
        m_startPoint = startPoint;
        m_currentPoint = startPoint;
        m_currentElements.clear(); // Clear the data from the previous draw operation

    }

    void SketchRectangleTool::UpdateDrawing(const QPoint& currentPoint) {
        if (!m_isDrawing) return;
        m_currentPoint = currentPoint;

        Standard_Real u1, v1, u2, v2;
        GetSnappedCoordinate(m_startPoint, u1, v1);
        GetSnappedCoordinate(m_currentPoint, u2, v2);

        m_currentElements = CreateRectangleLines(u1, v1, u2, v2);
        emit previewUpdated(m_currentElements);
    }

    void SketchRectangleTool::FinishDrawing(const QPoint& endPoint) {
        if (!m_isDrawing) return;
        UpdateDrawing(endPoint); // Perform one final position update

        m_isDrawing = false;
        emit elementsCreated(m_currentElements); // Submit the final shape

    }

    void SketchRectangleTool::CancelDrawing() {
        if (!m_isDrawing) return;
        m_isDrawing = false;
        m_currentElements.clear();
        emit drawingCancelled();
    }

    std::vector<cad_sketch::SketchElementPtr> SketchRectangleTool::CreateRectangleLines(Standard_Real u1, Standard_Real v1, Standard_Real u2, Standard_Real v2) {
        std::vector<cad_sketch::SketchElementPtr> elements;

        // Guard against rectangles with too small an area

        if (Abs(u1 - u2) < Precision::PConfusion() || Abs(v1 - v2) < Precision::PConfusion()) return elements;

        auto pt1 = std::make_shared<cad_sketch::SketchPoint>(u1, v1);
        auto pt2 = std::make_shared<cad_sketch::SketchPoint>(u2, v1);
        auto pt3 = std::make_shared<cad_sketch::SketchPoint>(u2, v2);
        auto pt4 = std::make_shared<cad_sketch::SketchPoint>(u1, v2);

        elements.push_back(std::make_shared<cad_sketch::SketchLine>(pt1, pt2));
        elements.push_back(std::make_shared<cad_sketch::SketchLine>(pt2, pt3));
        elements.push_back(std::make_shared<cad_sketch::SketchLine>(pt3, pt4));
        elements.push_back(std::make_shared<cad_sketch::SketchLine>(pt4, pt1));

        return elements;
    }

    // =============================================================================
    // SketchPointTool Implementation (Point tool implementation)

    // =============================================================================
    SketchPointTool::SketchPointTool(QObject* parent) : SketchToolBase(parent) {}

    void SketchPointTool::StartDrawing(const QPoint& startPoint) {
        Standard_Real u, v;
        // Get the snapped 2D coordinates

        GetSnappedCoordinate(startPoint, u, v);

        auto pt = std::make_shared<cad_sketch::SketchPoint>(u, v);

        m_currentElements.clear();
        m_currentElements.push_back(pt);

        // A single click completes the point and emits the creation signal

        emit elementsCreated(m_currentElements);

        m_currentElements.clear();
        m_isDrawing = false;
    }

    // Points do not need drag preview, so Update and Finish remain empty

    void SketchPointTool::UpdateDrawing(const QPoint& currentPoint) { Q_UNUSED(currentPoint); }
    void SketchPointTool::FinishDrawing(const QPoint& endPoint) { Q_UNUSED(endPoint); }

    void SketchPointTool::CancelDrawing() {
        m_isDrawing = false;
        m_currentElements.clear();
        emit drawingCancelled();
    }

    // =============================================================================
    // Comment translated to English
    // =============================================================================
    SketchLineTool::SketchLineTool(QObject* parent) : SketchToolBase(parent) {}

    void SketchLineTool::StartDrawing(const QPoint& startPoint) {
        m_isDrawing = true;
        m_startPoint = startPoint;
        m_currentElements.clear();
    }

    void SketchLineTool::UpdateDrawing(const QPoint& currentPoint) {
        if (!m_isDrawing) return;

        Standard_Real u1, v1, u2, v2;

        // Get the snapped coordinates of the start and end points

        GetSnappedCoordinate(m_startPoint, u1, v1);
        GetSnappedCoordinate(currentPoint, u2, v2);

        // Safety check: avoid creating a zero-length line

        double dx = u2 - u1;
        double dy = v2 - v1;
        if (std::sqrt(dx * dx + dy * dy) < Precision::Confusion()) {
            return;
        }

        auto pt1 = std::make_shared<cad_sketch::SketchPoint>(u1, v1);
        auto pt2 = std::make_shared<cad_sketch::SketchPoint>(u2, v2);

        m_currentElements.clear();
        m_currentElements.push_back(std::make_shared<cad_sketch::SketchLine>(pt1, pt2));
        emit previewUpdated(m_currentElements);
    }


    void SketchLineTool::FinishDrawing(const QPoint& endPoint) {
        if (!m_isDrawing) return;
        UpdateDrawing(endPoint);
        m_isDrawing = false;
        emit elementsCreated(m_currentElements);
    }

    void SketchLineTool::CancelDrawing() {
        m_isDrawing = false;
        m_currentElements.clear();
        emit drawingCancelled();
    }


    // =============================================================================
    // SketchCircleTool Implementation (Circle tool implementation)

    // =============================================================================
    SketchCircleTool::SketchCircleTool(QObject* parent) : SketchToolBase(parent) {}

    void SketchCircleTool::StartDrawing(const QPoint& startPoint) {
        m_isDrawing = true;
        m_centerPoint = startPoint;
        m_currentElements.clear();
    }

    void SketchCircleTool::UpdateDrawing(const QPoint& currentPoint) {
        if (!m_isDrawing) return;

        Standard_Real u1, v1, u2, v2;
        // Get the snapped center point and current mouse point

        GetSnappedCoordinate(m_centerPoint, u1, v1);
        GetSnappedCoordinate(currentPoint, u2, v2);

        // Compute the radius

        double dx = u2 - u1;
        double dy = v2 - v1;
        double radius = std::sqrt(dx * dx + dy * dy);

        // Safety check: the radius must be greater than zero

        if (radius < Precision::Confusion()) {
            return;
        }

        // Construct the circle data model

        auto center = std::make_shared<cad_sketch::SketchPoint>(u1, v1);
        auto circle = std::make_shared<cad_sketch::SketchCircle>(center, radius);

        m_currentElements.clear();
        m_currentElements.push_back(circle);
        emit previewUpdated(m_currentElements);
    }

    void SketchCircleTool::FinishDrawing(const QPoint& endPoint) {
        if (!m_isDrawing) return;
        UpdateDrawing(endPoint);
        m_isDrawing = false;
        emit elementsCreated(m_currentElements);
    }

    void SketchCircleTool::CancelDrawing() {
        m_isDrawing = false;
        m_currentElements.clear();
        emit drawingCancelled();
    }

    // =============================================================================
    // SketchArcTool Implementation (Arc tool implementation)

    // =============================================================================
    SketchArcTool::SketchArcTool(QObject* parent) : SketchToolBase(parent), m_state(Init) {}

    void SketchArcTool::StartDrawing(const QPoint& startPoint) {
        if (m_state == Init) {
            // First click: set the center

            m_isDrawing = true;
            m_centerPoint = startPoint;
            m_state = CenterSet;
            m_currentElements.clear();
        }
        else if (m_state == StartSet) {
            // Third interaction: confirm the end angle and finish

            m_isDrawing = true;
            UpdateDrawing(startPoint);
            emit elementsCreated(m_currentElements); // Comment translated to English

            // Reset the state for the next drawing operation

            m_state = Init;
            m_isDrawing = false;
            m_currentElements.clear();
        }
    }

    void SketchArcTool::UpdateDrawing(const QPoint& currentPoint) {
        Standard_Real u1, v1, u2, v2;
        GetSnappedCoordinate(m_centerPoint, u1, v1);

        if (m_state == CenterSet) {
            // Phase 1: preview the radius using a full circle while dragging

            GetSnappedCoordinate(currentPoint, u2, v2);
            double radius = std::hypot(u2 - u1, v2 - v1);

            if (radius < Precision::Confusion()) return;

            auto center = std::make_shared<cad_sketch::SketchPoint>(u1, v1);
            auto previewCircle = std::make_shared<cad_sketch::SketchCircle>(center, radius);

            m_currentElements.clear();
            m_currentElements.push_back(previewCircle);
            emit previewUpdated(m_currentElements);
        }
        else if (m_state == StartSet) {
            // Phase 2: preview the arc in real time while confirming the end angle

            Standard_Real uStart, vStart;
            GetSnappedCoordinate(m_startPoint, uStart, vStart);
            double radius = std::hypot(uStart - u1, vStart - v1);

            GetSnappedCoordinate(currentPoint, u2, v2);
            // Calculate the start and end angles

            double startAngle = std::atan2(vStart - v1, uStart - u1);
            double endAngle = std::atan2(v2 - v1, u2 - u1);

            // Normalize angles into the range [0, 2π]

            if (startAngle < 0) startAngle += 2 * M_PI;
            if (endAngle < 0) endAngle += 2 * M_PI;

            auto center = std::make_shared<cad_sketch::SketchPoint>(u1, v1);
            auto arc = std::make_shared<cad_sketch::SketchArc>(center, radius, startAngle, endAngle);

            m_currentElements.clear();
            m_currentElements.push_back(arc);
            emit previewUpdated(m_currentElements);
        }
    }

    void SketchArcTool::FinishDrawing(const QPoint& endPoint) {
        if (m_state == CenterSet) {
            // After the first drag completes, record the start point

            m_startPoint = endPoint;

            Standard_Real u1, v1, u2, v2;
            GetSnappedCoordinate(m_centerPoint, u1, v1);
            GetSnappedCoordinate(m_startPoint, u2, v2);

            // Safety check: the radius must not be too small

            if (std::hypot(u2 - u1, v2 - v1) > Precision::Confusion()) {
                m_state = StartSet; // Advance the state machine

            }
            else {
                CancelDrawing();
                return;
            }
            m_isDrawing = false; // Leave drag mode and enter hover-detection mode

        }
        else if (m_state == StartSet) {
            // If the user finishes the last stage by dragging

            UpdateDrawing(endPoint);
            emit elementsCreated(m_currentElements);
            m_state = Init;
            m_isDrawing = false;
            m_currentElements.clear();
        }
    }

    void SketchArcTool::CancelDrawing() {
        m_isDrawing = false;
        m_state = Init;
        m_currentElements.clear();
        emit drawingCancelled();
    }

    void SketchArcTool::HoverMove(const QPoint& currentPoint) {
        // Call the base implementation to keep snap detection active

        SketchToolBase::HoverMove(currentPoint);

        // Update the arc preview while hovering

        if (m_state == CenterSet || m_state == StartSet) {
            UpdateDrawing(currentPoint);
        }
    }

    // =============================================================================
    // SketchCurveTool Implementation (Curve tool implementation)

    // =============================================================================
    SketchCurveTool::SketchCurveTool(QObject* parent) : SketchToolBase(parent) {}

    void SketchCurveTool::StartDrawing(const QPoint& startPoint) {
        m_isDrawing = true;
        Standard_Real u, v;
        GetSnappedCoordinate(startPoint, u, v);

        // Prevent duplicate clicks at the same position

        if (!m_points.empty()) {
            auto lastPt = m_points.back();
            if (std::hypot(lastPt->GetX() - u, lastPt->GetY() - v) < Precision::Confusion()) {
                return;
            }
        }

        // Add a new control point on each click

        m_points.push_back(std::make_shared<cad_sketch::SketchPoint>(u, v));
    }

    void SketchCurveTool::UpdateDrawing(const QPoint& currentPoint) {
        if (!m_isDrawing || m_points.empty()) return;

        Standard_Real u, v;
        GetSnappedCoordinate(currentPoint, u, v);

        // Build the dynamic preview curve

        auto previewCurve = std::make_shared<cad_sketch::SketchCurve>();
        for (const auto& pt : m_points) {
            previewCurve->AddControlPoint(pt);
        }
        // Use the current mouse position as a temporary endpoint

        previewCurve->AddControlPoint(std::make_shared<cad_sketch::SketchPoint>(u, v));

        m_currentElements.clear();
        m_currentElements.push_back(previewCurve);
        emit previewUpdated(m_currentElements);
    }

    void SketchCurveTool::FinishDrawing(const QPoint& endPoint) {
        // Curve completion is controlled by ConfirmDrawing(), not mouse release

        // Leave this empty intentionally

        Q_UNUSED(endPoint);
    }

    void SketchCurveTool::CancelDrawing() {
        m_isDrawing = false;
        m_points.clear();
        m_currentElements.clear();
        emit drawingCancelled();
    }

    void SketchCurveTool::HoverMove(const QPoint& currentPoint) {
        SketchToolBase::HoverMove(currentPoint); // Keep the snapping indicator active

        UpdateDrawing(currentPoint);             // Update the curve preview while moving

    }

    void SketchCurveTool::ConfirmDrawing() {
        // At least two points are required to form a curve

        if (!m_isDrawing || m_points.size() < 2) {
            CancelDrawing();
            return;
        }

        auto finalCurve = std::make_shared<cad_sketch::SketchCurve>();
        m_currentElements.clear();

        // Also add all control points to the sketch element list

        // This makes the points render as independent vertices that can be selected and dragged

        for (const auto& pt : m_points) {
            finalCurve->AddControlPoint(pt);
            m_currentElements.push_back(pt);
        }

        // Finally add the whole curve

        m_currentElements.push_back(finalCurve);

        emit elementsCreated(m_currentElements); // Comment translated to English

        m_isDrawing = false;
        m_points.clear();
        m_currentElements.clear();
    }

    void SketchCurveTool::InjectStartPoint(double u, double v) {
        m_isDrawing = true;
        // Force (u, v) into the first control point regardless of mouse position

        m_points.push_back(std::make_shared<cad_sketch::SketchPoint>(u, v));
    }

    // =============================================================================
    // SketchMode Implementation (Main control logic of sketch mode)

    // =============================================================================

    SketchMode::SketchMode(QtOccView* viewer, QObject* parent)
        : QObject(parent), m_viewer(viewer), m_isActive(false) {
    }

    bool SketchMode::EnterSketchMode(const TopoDS_Face& face) {
        if (m_isActive) ExitSketchMode(); // Prevent repeated entry

        if (face.IsNull() || !m_viewer) return false;

        try {
            // 1. Back up the current 3D viewport state

            if (!m_viewer->GetView().IsNull()) {
                Handle(Graphic3d_Camera) camera = m_viewer->GetView()->Camera();
                if (!camera.IsNull()) {
                    m_savedEye = camera->Eye();
                    m_savedAt = camera->Center();
                    m_savedUp = camera->Up();
                    m_savedScale = camera->Scale();
                    m_savedProjectionType = camera->ProjectionType();
                }
            }

            // 2. Initialize the sketch plane and coordinate system

            m_sketchFace = face;
            SetupSketchPlane(face);
            m_isActive = true;
            m_undoStack.clear();
            m_redoStack.clear();

            // 3. If this is a fresh entry, instantiate and record the base face

            if (!m_currentSketch) {
                m_currentSketch = std::make_shared<cad_sketch::Sketch>("Sketch_001");

                // Store the computed face and coordinate system in the Sketch object

                m_currentSketch->SetBaseFace(m_sketchFace);
                m_currentSketch->SetBaseCS(m_sketchCS);
            }

            // 4. Switch the camera to the orthographic sketch view

            SetupSketchView();

            // When re-entering, refresh immediately to show existing sketch elements

            RefreshSketchView();

            if (m_viewer) m_viewer->HighlightSketchFace(face);
            emit sketchModeEntered();
            emit statusMessageChanged("Enter Sketch Mode");
            return true;
        }
        catch (...) { return false; }
    }

    // Enter sketch mode using a mathematical coordinate system

    bool SketchMode::EnterSketchMode(const gp_Ax3& customCS) {
        if (m_isActive) ExitSketchMode();
        if (!m_viewer) return false;

        try {
            // 1. Back up the current 3D viewport state

            if (!m_viewer->GetView().IsNull()) {
                Handle(Graphic3d_Camera) camera = m_viewer->GetView()->Camera();
                if (!camera.IsNull()) {
                    m_savedEye = camera->Eye();
                    m_savedAt = camera->Center();
                    m_savedUp = camera->Up();
                    m_savedScale = camera->Scale();
                    m_savedProjectionType = camera->ProjectionType();
                }
            }

            // 2. Initialize a purely mathematical sketch plane independent of TopoDS_Face

            m_sketchFace.Nullify(); // Clear the physical face reference

            m_sketchCS = customCS;
            Handle(Geom_Plane) plane = new Geom_Plane(m_sketchCS);
            m_sketchPlane = plane->Pln();

            m_isActive = true;
            m_undoStack.clear();
            m_redoStack.clear();

            // 3. Instantiate and record the base plane

            if (!m_currentSketch) {
                // Optionally give it a special name to mark it as a sweep-path sketch

                m_currentSketch = std::make_shared<cad_sketch::Sketch>("SweepPath_001");
                m_currentSketch->SetBaseCS(m_sketchCS);
                // Note: there is no SetBaseFace because this is a virtual plane

            }

            // 4. Switch the camera to the orthographic sketch view

            SetupSketchView();
            RefreshSketchView();

            emit sketchModeEntered();
            emit statusMessageChanged("Entered Sweep Path Sketch Mode");
            return true;
        }
        catch (...) { return false; }
    }

    bool SketchMode::EditSketch(const std::shared_ptr<cad_sketch::Sketch>& sketch) {
        if (m_isActive) ExitSketchMode();
        if (!sketch || !m_viewer) return false;

        try {
            // 1. Back up the current 3D viewport state (Camera Backup)

            if (!m_viewer->GetView().IsNull()) {
                Handle(Graphic3d_Camera) camera = m_viewer->GetView()->Camera();
                if (!camera.IsNull()) {
                    m_savedEye = camera->Eye();
                    m_savedAt = camera->Center();
                    m_savedUp = camera->Up();
                    m_savedScale = camera->Scale();
                    m_savedProjectionType = camera->ProjectionType();
                }
            }

            // 2. Inject the existing sketch to be edited

            m_currentSketch = sketch;

            // 3. Restore the base plane and local coordinate system from the sketch

            m_sketchFace = sketch->GetBaseFace();
            m_sketchCS = sketch->GetBaseCS();

            // Rebuild the mathematical plane used for snapping and ray tests

            Handle(Geom_Plane) plane = new Geom_Plane(m_sketchCS);
            m_sketchPlane = plane->Pln();

            m_isActive = true;
            m_undoStack.clear();
            m_redoStack.clear();

            // 4. Switch the camera to the orthographic sketch view

            SetupSketchView();

            // 5. Refresh immediately to render the sketch lines already stored in this sketch

            RefreshSketchView();

            if (m_viewer) m_viewer->HighlightSketchFace(m_sketchFace);
            emit sketchModeEntered();
            emit statusMessageChanged("Editing Sketch...");
            return true;
        }
        catch (...) { return false; }
    }

    void SketchMode::ExitSketchMode() {
        if (!m_isActive) return;
        if (m_viewer) m_viewer->HideSnapIndicator();

        StopCurrentTool();

        // Restore the camera view

        RestoreView();

        // Clear internal data

        m_isActive = false;
        m_viewer->ClearSketchFaceHighlight();
        m_currentSketch.reset();
        emit sketchModeExited();
        emit statusMessageChanged("Exited sketch mode");
    }

    void SketchMode::StartRectangleTool() {
        if (!m_isActive) return;
        StopCurrentTool(); // Stop the current tool before switching


        // Instantiate the tool and inject the context

        m_currentTool = std::make_unique<SketchRectangleTool>(this);
        m_currentTool->SetSketchPlane(m_sketchPlane);
        m_currentTool->SetView(m_viewer->GetView());
        m_currentTool->SetSnappingContext(&m_snappingManager, &(m_currentSketch->GetElements()));

        // Connect signals and slots

        connect(m_currentTool.get(), &SketchToolBase::previewUpdated, this, &SketchMode::OnPreviewUpdated);
        connect(m_currentTool.get(), &SketchToolBase::elementsCreated, this, &SketchMode::OnElementsCreated);
        connect(m_currentTool.get(), &SketchToolBase::drawingCancelled, this, &SketchMode::OnDrawingCancelled);
        connect(m_currentTool.get(), &SketchToolBase::snapPointDetected, this, &SketchMode::OnSnapPointDetected);
        connect(m_currentTool.get(), &SketchToolBase::snapPointLost, this, &SketchMode::OnSnapPointLost);

        emit statusMessageChanged("Started rectangle tool");
        emit toolChanged("Rectangle");
    }

    void SketchMode::StartPointTool() {
        if (!m_isActive) return;
        StopCurrentTool();

        m_currentTool = std::make_unique<SketchPointTool>(this);
        m_currentTool->SetSketchPlane(m_sketchPlane);
        m_currentTool->SetView(m_viewer->GetView());

        if (m_currentSketch) {
            m_currentTool->SetSnappingContext(&m_snappingManager, &(m_currentSketch->GetElements()));
        }

        connect(m_currentTool.get(), &SketchToolBase::previewUpdated, this, &SketchMode::OnPreviewUpdated);
        connect(m_currentTool.get(), &SketchToolBase::elementsCreated, this, &SketchMode::OnElementsCreated);
        connect(m_currentTool.get(), &SketchToolBase::drawingCancelled, this, &SketchMode::OnDrawingCancelled);
        connect(m_currentTool.get(), &SketchToolBase::snapPointDetected, this, &SketchMode::OnSnapPointDetected);
        connect(m_currentTool.get(), &SketchToolBase::snapPointLost, this, &SketchMode::OnSnapPointLost);

        emit statusMessageChanged("Started point tool");
        emit toolChanged("Point");
    }

    void SketchMode::StartLineTool() {
        if (!m_isActive) return;
        StopCurrentTool();

        m_currentTool = std::make_unique<SketchLineTool>(this);
        m_currentTool->SetSketchPlane(m_sketchPlane);
        m_currentTool->SetView(m_viewer->GetView());
        m_currentTool->SetSnappingContext(&m_snappingManager, &(m_currentSketch->GetElements()));

        connect(m_currentTool.get(), &SketchToolBase::previewUpdated, this, &SketchMode::OnPreviewUpdated);
        connect(m_currentTool.get(), &SketchToolBase::elementsCreated, this, &SketchMode::OnElementsCreated);
        connect(m_currentTool.get(), &SketchToolBase::drawingCancelled, this, &SketchMode::OnDrawingCancelled);
        connect(m_currentTool.get(), &SketchToolBase::snapPointDetected, this, &SketchMode::OnSnapPointDetected);
        connect(m_currentTool.get(), &SketchToolBase::snapPointLost, this, &SketchMode::OnSnapPointLost);

        emit statusMessageChanged("Started line tool");
        emit toolChanged("Line");
    }

    void SketchMode::StartCircleTool() {
        if (!m_isActive) return;
        StopCurrentTool();

        m_currentTool = std::make_unique<SketchCircleTool>(this);
        m_currentTool->SetSketchPlane(m_sketchPlane);
        m_currentTool->SetView(m_viewer->GetView());

        // Inject the snapping context so circles can also snap

        if (m_currentSketch) {
            m_currentTool->SetSnappingContext(&m_snappingManager, &(m_currentSketch->GetElements()));
        }

        connect(m_currentTool.get(), &SketchToolBase::previewUpdated, this, &SketchMode::OnPreviewUpdated);
        connect(m_currentTool.get(), &SketchToolBase::elementsCreated, this, &SketchMode::OnElementsCreated);
        connect(m_currentTool.get(), &SketchToolBase::drawingCancelled, this, &SketchMode::OnDrawingCancelled);
        connect(m_currentTool.get(), &SketchToolBase::snapPointDetected, this, &SketchMode::OnSnapPointDetected);
        connect(m_currentTool.get(), &SketchToolBase::snapPointLost, this, &SketchMode::OnSnapPointLost);

        emit statusMessageChanged("Started circle tool");
        emit toolChanged("Circle");
    }

    void SketchMode::StartArcTool() {
        if (!m_isActive) return;
        StopCurrentTool(); // Stop the current tool before switching


        m_currentTool = std::make_unique<SketchArcTool>(this);
        m_currentTool->SetSketchPlane(m_sketchPlane);
        m_currentTool->SetView(m_viewer->GetView());

        // Inject the snapping context

        if (m_currentSketch) {
            m_currentTool->SetSnappingContext(&m_snappingManager, &(m_currentSketch->GetElements()));
        }

        connect(m_currentTool.get(), &SketchToolBase::previewUpdated, this, &SketchMode::OnPreviewUpdated);
        connect(m_currentTool.get(), &SketchToolBase::elementsCreated, this, &SketchMode::OnElementsCreated);
        connect(m_currentTool.get(), &SketchToolBase::drawingCancelled, this, &SketchMode::OnDrawingCancelled);
        connect(m_currentTool.get(), &SketchToolBase::snapPointDetected, this, &SketchMode::OnSnapPointDetected);
        connect(m_currentTool.get(), &SketchToolBase::snapPointLost, this, &SketchMode::OnSnapPointLost);

        emit statusMessageChanged("Started arc tool");
        emit toolChanged("Arc");
    }

    void SketchMode::StartCurveTool() {
        if (!m_isActive) return;
        StopCurrentTool();

        m_currentTool = std::make_unique<SketchCurveTool>(this);
        m_currentTool->SetSketchPlane(m_sketchPlane);
        m_currentTool->SetView(m_viewer->GetView());

        if (m_currentSketch) {
            m_currentTool->SetSnappingContext(&m_snappingManager, &(m_currentSketch->GetElements()));
        }

        connect(m_currentTool.get(), &SketchToolBase::previewUpdated, this, &SketchMode::OnPreviewUpdated);
        connect(m_currentTool.get(), &SketchToolBase::elementsCreated, this, &SketchMode::OnElementsCreated);
        connect(m_currentTool.get(), &SketchToolBase::drawingCancelled, this, &SketchMode::OnDrawingCancelled);
        connect(m_currentTool.get(), &SketchToolBase::snapPointDetected, this, &SketchMode::OnSnapPointDetected);
        connect(m_currentTool.get(), &SketchToolBase::snapPointLost, this, &SketchMode::OnSnapPointLost);

        emit statusMessageChanged("Started curve tool");
        emit toolChanged("Curve");
    }

    void SketchMode::StopCurrentTool() {
        if (m_viewer) m_viewer->HideSnapIndicator();

        if (m_currentTool) {
            if (m_currentTool->IsDrawing()) {
                m_currentTool->CancelDrawing();
            }
            m_currentTool.reset();
        }

        // Emit a signal to notify the UI that no tool is running

        emit toolChanged("None");
    }

    // The following three functions delegate view mouse events to the active tool

    void SketchMode::HandleMousePress(QMouseEvent* event) {
        // If temporary 3D navigation is active, skip handling and give control back to the 3D view

        if (m_isTemporary3DView) return;

        if (!m_isActive) return;

        // 1. If a drawing tool is currently active

        if (m_currentTool) {
            if (event->button() == Qt::LeftButton) {
                // Left button: start drawing or add a control point

                m_currentTool->StartDrawing(event->pos());
            }
            else if (event->button() == Qt::RightButton) {
                // Right button: confirm multi-click tools such as curves, or cancel other tools

                auto curveTool = dynamic_cast<SketchCurveTool*>(m_currentTool.get());
                if (curveTool) {
                    curveTool->ConfirmDrawing(); // Confirm and generate the curve

                }
                else {
                    m_currentTool->CancelDrawing(); // Cancel other single-click tools on right-click

                }
            }
        }
        // 2. If no drawing tool is active and the left button is pressed, enter selection or edit mode

        else if (!m_currentTool && event->button() == Qt::LeftButton) {
            auto selected = m_viewer->GetSelectedSketchElements();
            if (!selected.empty()) {

                // Keep only elements that belong to the active sketch

                std::vector<cad_sketch::SketchElementPtr> validElements;
                if (m_currentSketch) {
                    const auto& currentElements = m_currentSketch->GetElements();
                    for (const auto& elem : selected) {
                        // Only selected elements contained in the current sketch are draggable

                        if (std::find(currentElements.begin(), currentElements.end(), elem) != currentElements.end()) {
                            validElements.push_back(elem);
                        }
                    }
                }

                // Enter drag or rotate mode only if valid elements remain after filtering

                if (!validElements.empty()) {
                    m_draggedElements = validElements; // Use the filtered valid elements instead


                    // Hold Ctrl to enter rotation mode

                    if (event->modifiers() & Qt::ControlModifier) {
                        m_isRotating = true;
                        m_didActuallyMove = false;
                        m_isFirstRotation = true; // Mark this as the first rotation frame

                        GetPlaneCoordinate(event->pos(), m_rotCenterU, m_rotCenterV);
                        m_lastAngle = 0.0;
                    }
                    // Otherwise enter translation mode

                    else {
                        m_isDragging = true;
                        m_didActuallyMove = false;
                        GetPlaneCoordinate(event->pos(), m_lastDragU, m_lastDragV);
                    }
                }
            }
        }
    }

    void SketchMode::HandleMouseMove(QMouseEvent* event) {
        // If temporary 3D navigation is active, skip handling and give control back to the 3D view

        if (m_isTemporary3DView) return;

        if (!m_isActive) return;

        if (m_currentTool) {
            m_currentTool->HoverMove(event->pos());
            if (m_currentTool->IsDrawing()) {
                m_currentTool->UpdateDrawing(event->pos());
            }
        }
        else {
            // Rotation logic

            if (m_isRotating && !m_draggedElements.empty()) {
                double currentU = 0.0, currentV = 0.0;
                GetPlaneCoordinate(event->pos(), currentU, currentV);

                // Compute the current mouse angle

                double currentAngle = std::atan2(currentV - m_rotCenterV, currentU - m_rotCenterU);

                // On the first drag frame, record the initial angle without rotating

                if (m_isFirstRotation) {
                    m_lastAngle = currentAngle;
                    m_isFirstRotation = false;
                    return;
                }

                // Compute the angle delta

                double deltaAngle = currentAngle - m_lastAngle;

                for (auto& elem : m_draggedElements) {
                    elem->Rotate(m_rotCenterU, m_rotCenterV, deltaAngle);
                    m_viewer->UpdateSketchElementVisuals(elem);
                }

                m_lastAngle = currentAngle;
            }
            // Translation logic

            else if (m_isDragging && !m_draggedElements.empty()) {
                double currentU = 0.0, currentV = 0.0;
                GetPlaneCoordinate(event->pos(), currentU, currentV);

                double dx = currentU - m_lastDragU;
                double dy = currentV - m_lastDragV;

                if (std::abs(dx) > 1e-6 || std::abs(dy) > 1e-6) {   // Dead-zone test

                    m_didActuallyMove = true;   //Confirm that an actual movement occurred

                    for (auto& elem : m_draggedElements) {
                        elem->Translate(dx, dy);
                        m_viewer->UpdateSketchElementVisuals(elem);
                    }
                    m_lastDragU = currentU;
                    m_lastDragV = currentV;
                }
            }
        }
    }


    void SketchMode::HandleMouseRelease(QMouseEvent* event) {
        // If temporary 3D navigation is active, skip handling and give control back to the 3D view

        if (m_isTemporary3DView) return;

        if (!m_isActive) return;

        // If a drawing tool is active, let it finish the drawing

        if (m_currentTool && event->button() == Qt::LeftButton && m_currentTool->IsDrawing()) {
            m_currentTool->FinishDrawing(event->pos());
        }

        // Finish translation dragging

        else if (!m_currentTool && event->button() == Qt::LeftButton) {
            if (m_isDragging || m_isRotating) {
                bool needRebuild = m_didActuallyMove;
                m_isDragging = false;
                m_isRotating = false;
                m_didActuallyMove = false;
                m_draggedElements.clear();

                if (m_currentSketch && needRebuild) {   //Rebuild only if an actual movement occurred

                    if (!m_currentSketch->GetConstraints().empty()) {
                        m_currentSketch->SolveConstraints();
                        m_viewer->ClearSketchObjects();
                        m_viewer->AddSketchElements(m_currentSketch->GetElements(), m_sketchCS);
                    }
                    m_currentSketch->UpdateProfiles(m_sketchCS);
                    m_viewer->ClearSketchProfiles();
                    m_viewer->RenderSketchProfiles(m_currentSketch->GetProfiles());
                }

                emit sketchHistoryChanged();
            }
        }

    }

    void SketchMode::HandleKeyPress(QKeyEvent* event) {
        if (!m_isActive) return;

        // Press Enter to confirm the curve

        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            if (m_currentTool) {
                auto curveTool = dynamic_cast<SketchCurveTool*>(m_currentTool.get());
                if (curveTool) {
                    curveTool->ConfirmDrawing();
                }
            }
            return;
        }

        // Intercept Delete and Backspace

        if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
            // Execute deletion only when not drawing or dragging

            if (!m_currentTool || !m_currentTool->IsDrawing()) {
                DeleteSelectedElements();
            }
            return;
        }

        // Esc key handling

        if (event->key() == Qt::Key_Escape) {
            if (m_currentTool && m_currentTool->IsDrawing()) {
                m_currentTool->CancelDrawing();
            }
            else {
                ExitSketchMode(); // Press Esc to exit sketch mode when no drawing is in progress

            }
        }
    }

    void SketchMode::OnElementsCreated(const std::vector<cad_sketch::SketchElementPtr>& elements) {
        if (m_viewer && m_isActive) {
            m_viewer->HideSnapIndicator();
            m_viewer->ClearSketchPreview();
            m_viewer->AddSketchElements(elements, m_sketchCS);

            if (m_currentSketch) {
                for (const auto& elem : elements) {
                    m_currentSketch->AddElement(elem);
                }

                m_undoStack.push_back({ SketchHistoryStep::ADD, elements });
                m_redoStack.clear();
                emit sketchHistoryChanged();

                // ---- New: automatic constraints for rectangles ----

                if (auto* rectTool = dynamic_cast<SketchRectangleTool*>(m_currentTool.get())) {
                    AutoConstrainRectangle(elements);
                }
                // ---- End of new rectangle-constraint logic ----


                m_currentSketch->UpdateProfiles(m_sketchCS);
                m_viewer->RenderSketchProfiles(m_currentSketch->GetProfiles());
            }
        }
        emit statusMessageChanged(tr("Shape created."));
    }

    void SketchMode::OnDrawingCancelled() {
        if (m_viewer) m_viewer->HideSnapIndicator();
        emit statusMessageChanged("Drawing cancelled");
    }

    void SketchMode::OnPreviewUpdated(const std::vector<cad_sketch::SketchElementPtr>& elements) {
        if (m_viewer && m_isActive) {
            m_viewer->ShowSketchPreviewElements(elements, m_sketchCS);
        }
    }

    void SketchMode::OnSnapPointDetected(const gp_Pnt& pnt, cad_sketch::SnapType snapType) {
        if (m_viewer && m_isActive) {
            m_viewer->ShowSnapIndicator(pnt, snapType);
        }
    }

    void SketchMode::OnSnapPointLost() {
        if (m_viewer && m_isActive) {
            m_viewer->HideSnapIndicator();
        }
    }

    void SketchMode::SetupSketchPlane(const TopoDS_Face& face) {
        // Extract the geometric surface from the topological face

        Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
        Handle(Geom_Plane) plane = Handle(Geom_Plane)::DownCast(surface);

        if (!plane.IsNull()) {
            m_sketchPlane = plane->Pln(); // Get the underlying mathematical plane


            // Compute the UV bounds of the face

            Standard_Real uMin, uMax, vMin, vMax;
            BRepTools::UVBounds(face, uMin, uMax, vMin, vMax);
            Standard_Real uMid = (uMin + uMax) / 2.0;
            Standard_Real vMid = (vMin + vMax) / 2.0;

            // Compute the normal at the face center

            GeomLProp_SLProps props(surface, uMid, vMid, 1, Precision::Confusion());
            gp_Dir normal = props.Normal();

            // Correct the topological orientation so the normal points outward

            if (face.Orientation() == TopAbs_REVERSED) normal.Reverse();

            // Rebuild the local coordinate system from the corrected outward normal

            gp_Ax3 correctCS(m_sketchPlane.Location(), normal, m_sketchPlane.XAxis().Direction());
            m_sketchPlane.SetPosition(correctCS);
            m_sketchCS = m_sketchPlane.Position();
        }
    }

    void SketchMode::SetupSketchView() {
        if (!m_viewer || m_viewer->GetView().IsNull()) return;
        Handle(V3d_View) view = m_viewer->GetView();
        Handle(Graphic3d_Camera) camera = view->Camera();

        // Place the camera 100 units above the plane along its normal

        gp_Pnt planeOrigin = m_sketchPlane.Location();
        gp_Dir planeNormal = m_sketchPlane.Axis().Direction();
        gp_Pnt eyePosition = planeOrigin.XYZ() + planeNormal.XYZ() * 100.0;
        gp_Dir yDir = m_sketchCS.YDirection(); // Make the Y axis point upward


        camera->SetEye(eyePosition);
        camera->SetCenter(planeOrigin);
        camera->SetUp(yDir);
        camera->OrthogonalizeUp(); // Force the Up vector to stay orthogonal to the view direction


        // Switch to orthographic projection to avoid perspective distortion while sketching

        camera->SetProjectionType(Graphic3d_Camera::Projection_Orthographic);

        view->FitAll(); // Fit the view to the screen automatically

        view->ZFitAll(); // Adjust the Z clipping planes

    }

    // Enter temporary 3D view mode

    void SketchMode::StartTemporary3DView() {
        if (!m_isActive || m_isTemporary3DView || !m_viewer) return;
        if (m_viewer->GetView().IsNull()) return;

        m_isTemporary3DView = true;
    }

    void SketchMode::StopTemporary3DView() {
        if (!m_isActive || !m_isTemporary3DView || !m_viewer) return;
        if (m_viewer->GetView().IsNull()) return;

        // Do not restore the backup camera; reinitialize the orthographic sketch view directly

        SetupSketchView();

        // Force an immediate redraw of the underlying OpenGL buffer

        m_viewer->GetView()->Redraw();

        // Notify Qt to refresh the view widget

        m_viewer->update();

        m_isTemporary3DView = false;
    }


    void SketchMode::RestoreView() {
        if (!m_viewer || m_viewer->GetView().IsNull()) return;
        Handle(V3d_View) view = m_viewer->GetView();
        Handle(Graphic3d_Camera) camera = view->Camera();

        // Restore the backed-up camera parameters

        camera->SetProjectionType(m_savedProjectionType);
        camera->SetEye(m_savedEye);
        camera->SetCenter(m_savedAt);
        camera->SetUp(m_savedUp);
        camera->OrthogonalizeUp();
        camera->SetScale(m_savedScale);

        view->AutoZFit();
        view->Redraw(); // Comment translated to English
    }

    void SketchMode::CreateSketchCoordinateSystem() {
        m_sketchCS = gp_Ax3(m_sketchPlane.Location(), m_sketchPlane.Axis().Direction(), m_sketchPlane.XAxis().Direction());
    }

    gp_Pln SketchMode::ExtractPlaneFromFace(const TopoDS_Face& face) {
        if (face.IsNull()) return gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)); // Default to the XY plane

        Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
        Handle(Geom_Plane) plane = Handle(Geom_Plane)::DownCast(surface);
        if (!plane.IsNull()) return plane->Pln();
        return gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
    }

    // A coordinate query function without snapping, used specifically for translation deltas

    void SketchMode::GetPlaneCoordinate(const QPoint& screenPos, double& u, double& v) {
        if (!m_viewer || m_viewer->GetView().IsNull()) return;

        // 1. Use ConvertWithProj to get the 3D point and projection direction

        Standard_Real X, Y, Z;
        Standard_Real Vx, Vy, Vz;
        m_viewer->GetView()->ConvertWithProj(screenPos.x(), screenPos.y(), X, Y, Z, Vx, Vy, Vz);

        gp_Pnt p1(X, Y, Z);
        gp_Vec dir(Vx, Vy, Vz);

        // Prevent crashes caused by a zero direction vector

        if (dir.SquareMagnitude() < Precision::Confusion()) {
            return;
        }

        // 2. Construct a ray

        gp_Lin ray(p1, dir);

        // 3. Construct the sketch plane

        gp_Pln pln(m_sketchCS);

        // 4. Compute the ray-plane intersection

        Standard_Real u_param, v_param;
        IntAna_IntConicQuad intersection(ray, pln, Precision::Angular(), Precision::Confusion());
        if (intersection.IsDone() && intersection.NbPoints() > 0) {
            gp_Pnt pt = intersection.Point(1); // Get the 3D intersection point

            ElSLib::Parameters(pln, pt, u_param, v_param); // Convert the 3D intersection point to 2D (U, V) coordinates on the plane

            u = u_param;
            v = v_param;
        }
    }


    void SketchMode::DeleteSelectedElements() {
        if (!m_viewer) return;

        auto selected = m_viewer->GetSelectedSketchElements();
        if (selected.empty()) return;

        std::vector<cad_sketch::SketchElementPtr> validElements;
        if (m_currentSketch) {
            const auto& currentElements = m_currentSketch->GetElements();
            for (const auto& elem : selected) {
                if (std::find(currentElements.begin(), currentElements.end(), elem) != currentElements.end()) {
                    validElements.push_back(elem);
                }
            }
        }

        if (validElements.empty()) return; // Reject deletion if there are no elements from the current sketch


        // 1. Erase valid elements from the screen

        m_viewer->RemoveSketchElements(validElements);

        // 2. Remove them from the underlying data

        if (m_currentSketch) {
            for (auto& elem : validElements) {
                m_currentSketch->RemoveElement(elem);
            }
            m_currentSketch->UpdateProfiles(m_sketchCS);
            m_viewer->RenderSketchProfiles(m_currentSketch->GetProfiles());
        }

        // 3. Record the undo step (only for valid deleted elements)

        m_undoStack.push_back({ SketchHistoryStep::REMOVE, validElements });
        m_redoStack.clear();

        emit sketchHistoryChanged();
        emit statusMessageChanged(tr("Deleted selected element(s)"));
    }

    void SketchMode::RefreshSketchView() {
        if (!m_viewer) return;

        if (m_currentSketch) {
            m_viewer->RemoveSketch(m_currentSketch); // Precisely erase the old graphics of the current sketch


            m_viewer->AddSketchElements(m_currentSketch->GetElements(), m_sketchCS);
            m_currentSketch->UpdateProfiles(m_sketchCS);
            m_viewer->RenderSketchProfiles(m_currentSketch->GetProfiles());
        }
    }

    void SketchMode::AutoConstrainRectangle(const std::vector<cad_sketch::SketchElementPtr>& elements) {
        // A rectangle must consist of exactly four lines

        if (elements.size() != 4) return;

        std::vector<cad_sketch::SketchLinePtr> lines;
        for (const auto& elem : elements) {
            auto line = std::dynamic_pointer_cast<cad_sketch::SketchLine>(elem);
            if (!line) return;  // If not all elements are lines, it is not a rectangle

            lines.push_back(line);
        }

        // lines[0]=bottom, lines[1]=right, lines[2]=top, lines[3]=left

        // Horizontal constraints: bottom and top

        m_currentSketch->AddConstraint(std::make_shared<cad_sketch::HorizontalConstraint>(lines[0]));
        m_currentSketch->AddConstraint(std::make_shared<cad_sketch::HorizontalConstraint>(lines[2]));
        // Vertical constraints: right and left

        m_currentSketch->AddConstraint(std::make_shared<cad_sketch::VerticalConstraint>(lines[1]));
        m_currentSketch->AddConstraint(std::make_shared<cad_sketch::VerticalConstraint>(lines[3]));

        // Coincidence constraints at the four corners to make adjacency explicit to the solver

        m_currentSketch->AddConstraint(std::make_shared<cad_sketch::CoincidentConstraint>(
            lines[0]->GetEndPoint(), lines[1]->GetStartPoint()));
        m_currentSketch->AddConstraint(std::make_shared<cad_sketch::CoincidentConstraint>(
            lines[1]->GetEndPoint(), lines[2]->GetStartPoint()));
        m_currentSketch->AddConstraint(std::make_shared<cad_sketch::CoincidentConstraint>(
            lines[2]->GetEndPoint(), lines[3]->GetStartPoint()));
        m_currentSketch->AddConstraint(std::make_shared<cad_sketch::CoincidentConstraint>(
            lines[3]->GetEndPoint(), lines[0]->GetStartPoint()));
    }

    void SketchMode::Undo() {
        if (m_undoStack.empty()) return;

        auto step = m_undoStack.back();
        m_undoStack.pop_back();
        m_redoStack.push_back(step); // Push onto the redo stack


        if (m_currentSketch) {
            if (step.type == SketchHistoryStep::ADD) {
                // Undo add = remove it

                for (auto& elem : step.elements) m_currentSketch->RemoveElement(elem);
            }
            else if (step.type == SketchHistoryStep::REMOVE) {
                // Undo delete = restore it

                for (auto& elem : step.elements) m_currentSketch->AddElement(elem);
            }
        }
        RefreshSketchView(); // Refresh the screen display

        emit sketchHistoryChanged();
    }

    void SketchMode::Redo() {
        if (m_redoStack.empty()) return;

        auto step = m_redoStack.back();
        m_redoStack.pop_back();
        m_undoStack.push_back(step); // Push back onto the undo stack


        if (m_currentSketch) {
            if (step.type == SketchHistoryStep::ADD) {
                // Redo add = add it again

                for (auto& elem : step.elements) m_currentSketch->AddElement(elem);
            }
            else if (step.type == SketchHistoryStep::REMOVE) {
                // Redo delete = remove it again

                for (auto& elem : step.elements) m_currentSketch->RemoveElement(elem);
            }
        }
        RefreshSketchView(); // Refresh the screen display

        emit sketchHistoryChanged();
    }

} // namespace cad_ui
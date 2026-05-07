#include "cad_ui/QtOccView.h"
#include "cad_ui/SketchMode.h"
#include "cad_core/FilletChamferOperations.h"
#include <BRepBuilderAPI_MakeFace.hxx>
#include <OpenGl_GraphicDriver.hxx>
#include <Aspect_Handle.hxx>
#include <Aspect_DisplayConnection.hxx>
#include <V3d_AmbientLight.hxx>
#include <V3d_DirectionalLight.hxx>
#include <Prs3d_Drawer.hxx>
#include <AIS_ViewCube.hxx>
#include <AIS_Trihedron.hxx>
#include <Geom_Axis2Placement.hxx>
#include <Aspect_RectangularGrid.hxx>
#include <QFocusEvent>
#include <QShowEvent>
#include <QDebug>
#include <QPainter>
#include <StdSelect_BRepOwner.hxx>
#include <TopoDS.hxx>
#include <TopAbs.hxx>
#include <Prs3d_LineAspect.hxx>
#include <Quantity_Color.hxx>
#include <algorithm>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <AIS_Shape.hxx>
#include <Quantity_NameOfColor.hxx>
#include <ElSLib.hxx>
#include <Graphic3d_MaterialAspect.hxx>
#include "cad_sketch/SketchCircle.h"
#include "cad_sketch/SketchArc.h"
#include <gp_Circ.hxx> 
#include <AIS_Point.hxx>
#include <Geom_CartesianPoint.hxx>
#include <Aspect_TypeOfMarker.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include "cad_core/Shape.h"
#include <BRepPrimAPI_MakeSphere.hxx>
#include <QMessageBox>
#include <QApplication>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <GeomLProp_SLProps.hxx>
#include <Precision.hxx>
#include <GeomAPI_Interpolate.hxx>
#include <TColgp_HArray1OfPnt.hxx>
#include <Geom_BSplineCurve.hxx>
#include "cad_sketch/SketchCurve.h"
#include <Geom_Plane.hxx>
#include "cad_feature/SweepFeature.h"
#include <BRepBuilderAPI_MakeWire.hxx>
#include <TopoDS_Edge.hxx>

#ifdef _WIN32
#include <WNT_Window.hxx>
#elif defined(__APPLE__)
#include <Cocoa_Window.hxx>
#else
#include <Xw_Window.hxx>
#endif

namespace cad_ui {


    QtOccView::QtOccView(QWidget* parent)
        : QWidget(parent),
        m_isInitialized(false),
        m_isLeftDragging(false),
        m_currentMouseButton(Qt::NoButton),
        m_currentSelectedShape(nullptr),
        m_currentSelectionMode(0) {

        // Set widget attributes to reduce flicker
        setAttribute(Qt::WA_PaintOnScreen);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_NativeWindow);
        setAttribute(Qt::WA_OpaquePaintEvent);  // Prevent Qt from erasing background
        setAttribute(Qt::WA_StaticContents);    // Widget contents don't scroll
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setAutoFillBackground(false);
        setFocusPolicy(Qt::StrongFocus);

        // Initialize timer for redraw
        m_redrawTimer = new QTimer(this);
        m_redrawTimer->setSingleShot(true);
        connect(m_redrawTimer, &QTimer::timeout, this, &QtOccView::OnRedrawTimer);

        // Initialize selection manager
        m_selectionManager = std::make_unique<cad_core::SelectionManager>();

        // Initialize sketch mode (delayed initialization to avoid crash)
        m_sketchMode = nullptr; // Will be initialized on first use

        // Initialize OpenCASCADE
        InitializeOCC();
    }

    bool QtOccView::InitViewer() {
        if (m_isInitialized) {
            return true;
        }

        // Ensure widget has a valid window ID
        if (winId() == 0) {
            // Widget not ready yet, defer initialization
            return false;
        }

        try {
            // Create graphics driver
            Handle(Aspect_DisplayConnection) displayConnection = new Aspect_DisplayConnection();
            m_driver = new OpenGl_GraphicDriver(displayConnection);

            // Create viewer
            m_viewer = new V3d_Viewer(m_driver);
            m_viewer->SetDefaultLights();
            m_viewer->SetLightOn();

            // Create interactive context
            m_context = new AIS_InteractiveContext(m_viewer);

            // Create view
            m_view = m_viewer->CreateView();

            // Create window
#ifdef _WIN32
            Handle(WNT_Window) window = new WNT_Window(reinterpret_cast<Aspect_Handle>(winId()));
#elif defined(__APPLE__)
            Handle(Cocoa_Window) window = new Cocoa_Window(reinterpret_cast<NSView*>(winId()));
#else
            Handle(Xw_Window) window = new Xw_Window(displayConnection, winId());
#endif

            m_view->SetWindow(window);

            // Ensure window is mapped properly
            if (!window->IsMapped()) {
                window->Map();
            }

            // Set up view
            m_view->SetBackgroundColor(Quantity_NOC_GRAY30);

            // Ensure proper sizing
            window->DoResize();
            m_view->MustBeResized();
            // Note: Trihedron (coordinate axes) will be controlled by ShowAxes() function

            // Add ViewCube for navigation (without axis labels to avoid duplication)
            Handle(AIS_ViewCube) viewCube = new AIS_ViewCube();
            viewCube->SetSize(50, Standard_False);
            viewCube->SetBoxColor(Quantity_NOC_GRAY75);
            viewCube->SetInnerColor(Quantity_NOC_GRAY90);
            viewCube->SetTextColor(Quantity_NOC_BLACK);
            // Remove axis labels to avoid duplication with Trihedron
            viewCube->SetTransparency(0.1);
            viewCube->SetMaterial(Graphic3d_NOM_PLASTIC);
            m_context->Display(viewCube, Standard_False);

            // Set up context
            m_context->SetDisplayMode(AIS_Shaded, Standard_False);

            // Set unified highlight style
            // 1. Global selection style (state after clicking a normal entity)
            Handle(Prs3d_Drawer) selectedDrawer = m_context->HighlightStyle(Prs3d_TypeOfHighlight_Selected);
            if (!selectedDrawer.IsNull()) {
                selectedDrawer->SetColor(Quantity_NOC_RED);
                selectedDrawer->SetDisplayMode(1);     // Shaded
                selectedDrawer->SetTransparency(0.0f); // Opaque, as final selected state
            }

            // 2. Global hover style (preview when hovering over a normal entity)
            Handle(Prs3d_Drawer) dynamicDrawer = m_context->HighlightStyle(Prs3d_TypeOfHighlight_Dynamic);
            if (!dynamicDrawer.IsNull()) {
                dynamicDrawer->SetColor(Quantity_NOC_LIGHTSKYBLUE1);
                dynamicDrawer->SetDisplayMode(1);      // Shaded
                dynamicDrawer->SetTransparency(0.15f); // As preview state
            }

            // 3. Local selection style (state after clicking Face / Edge / Vertex)
            Handle(Prs3d_Drawer) localSelectedDrawer = m_context->HighlightStyle(Prs3d_TypeOfHighlight_LocalSelected);
            if (!localSelectedDrawer.IsNull()) {
                localSelectedDrawer->SetColor(Quantity_NOC_RED);
                localSelectedDrawer->SetDisplayMode(1);      // Force filled display
                localSelectedDrawer->SetTransparency(0.25f); // Slightly transparent to see model clearly
            }

            // 4. Local hover style (preview when hovering over Face / Edge / Vertex)
            Handle(Prs3d_Drawer) localDynamicDrawer = m_context->HighlightStyle(Prs3d_TypeOfHighlight_LocalDynamic);
            if (!localDynamicDrawer.IsNull()) {
                localDynamicDrawer->SetColor(Quantity_NOC_LIGHTBLUE);
                localDynamicDrawer->SetDisplayMode(1);      // Force filled display
                localDynamicDrawer->SetTransparency(0.25f); // As preview, do not make it too heavy
            }

            // Set up selection manager
            m_selectionManager->SetContext(m_context);
            m_selectionManager->SetView(m_view);

            m_isInitialized = true;

            // Initial view setup and render
            FitAll();
            ShowAxes(false);  // Show coordinate axes by default
            m_view->Redraw();  // Ensure initial rendering

            return true;
        }
        catch (const Standard_Failure& e) {
            m_isInitialized = false;
            return false;
        }
    }

    void QtOccView::FitAll() {
        if (m_view.IsNull()) return;

        m_view->FitAll();
        m_view->ZFitAll();
        m_view->Redraw();
    }

    void QtOccView::ZoomIn() {
        if (m_view.IsNull()) return;

        m_view->SetZoom(1.5);
        m_view->Redraw();
    }

    void QtOccView::ZoomOut() {
        if (m_view.IsNull()) return;

        m_view->SetZoom(0.75);
        m_view->Redraw();
    }

    void QtOccView::Pan(int dx, int dy) {
        if (m_view.IsNull()) return;

        m_view->Pan(dx, dy);
        // Remove update() call to reduce flickering
    }

    void QtOccView::Rotate(int dx, int dy) {
        if (m_view.IsNull()) return;

        m_view->Rotation(dx, dy);
        // Remove update() call to reduce flickering
    }

    void QtOccView::SetViewMode(const QString& mode) {
        if (m_view.IsNull()) return;

        if (mode == "wireframe") {
            m_context->SetDisplayMode(AIS_WireFrame, Standard_True);
        }
        else if (mode == "shaded") {
            m_context->SetDisplayMode(AIS_Shaded, Standard_True);
        }
        m_view->Redraw();
    }

    void QtOccView::SetProjectionMode(bool orthographic) {
        if (m_view.IsNull()) return;

        if (orthographic) {
            m_view->Camera()->SetProjectionType(Graphic3d_Camera::Projection_Orthographic);
        }
        else {
            m_view->Camera()->SetProjectionType(Graphic3d_Camera::Projection_Perspective);
        }
        m_view->Redraw();
    }

    void QtOccView::DisplayShape(const cad_core::ShapePtr& shape) {
        if (!shape || shape->GetOCCTShape().IsNull() || m_context.IsNull()) {
            return;
        }

        Handle(AIS_Shape) aisShape = new AIS_Shape(shape->GetOCCTShape());

        // Set shape properties for better visibility
        aisShape->SetColor(Quantity_NOC_GRAY);
        aisShape->SetTransparency(0.0);

        m_context->Display(aisShape, Standard_False);


        // Store mapping for selection synchronization
        m_shapeToAIS[shape] = aisShape;

        // Fit all objects in view to ensure visibility and render
        m_view->FitAll();
        m_view->Redraw();

        // Force immediate rendering
        update();
    }

    void QtOccView::SetShapeVisibility(const cad_core::ShapePtr& shape, bool visible) {
        if (!shape || m_context.IsNull()) return;

        // Find the corresponding AIS display object for this shape in the mapping table
        auto it = m_shapeToAIS.find(shape);
        if (it != m_shapeToAIS.end()) {
            Handle(AIS_Shape) aisShape = it->second;
            if (!aisShape.IsNull()) {
                if (visible) {
                    m_context->Display(aisShape, Standard_False); // Show
                }
                else {
                    // If it is currently selected, deselect it first
                    if (m_currentSelectedAIS == aisShape) {
                        m_context->SetSelected(aisShape, Standard_False);
                        m_currentSelectedAIS.Nullify();
                        m_currentSelectedShape.reset();
                    }
                    m_context->Erase(aisShape, Standard_False); // Hide (Erase)
                }
                m_view->Redraw();
            }
        }
    }

    QPaintEngine* QtOccView::paintEngine() const
    {
        return nullptr;
    }

    void QtOccView::RemoveShape(const cad_core::ShapePtr& shape) {
        if (!shape || m_context.IsNull()) {
            return;
        }

        // Find and remove the AIS_Shape
        auto it = m_shapeToAIS.find(shape);
        if (it != m_shapeToAIS.end()) {
            Handle(AIS_Shape) aisShape = it->second;
            if (!aisShape.IsNull()) {
                m_context->Remove(aisShape, Standard_False);
            }
            m_shapeToAIS.erase(it);
        }

        m_view->Redraw();
        update();
    }

    void QtOccView::ClearShapes() {
        if (m_context.IsNull()) return;

        for (auto& pair : m_shapeToAIS) {
            if (!pair.second.IsNull()) {
                m_context->Remove(pair.second, Standard_False);
            }
        }

        m_shapeToAIS.clear();
        m_view->Redraw();
    }

    void QtOccView::RedrawAll() {
        if (m_view.IsNull()) return;

        m_view->Redraw();
    }

    void QtOccView::SetBackgroundColor(const QColor& color) {
        if (m_view.IsNull()) return;

        Quantity_Color occColor(color.redF(), color.greenF(), color.blueF(), Quantity_TOC_RGB);
        m_view->SetBackgroundColor(occColor);
        m_view->Redraw();
    }

    void QtOccView::SetBackgroundGradient(const QColor& color1, const QColor& color2) {
        if (m_view.IsNull()) return;

        Quantity_Color occColor1(color1.redF(), color1.greenF(), color1.blueF(), Quantity_TOC_RGB);
        Quantity_Color occColor2(color2.redF(), color2.greenF(), color2.blueF(), Quantity_TOC_RGB);

        m_view->SetBgGradientColors(occColor1, occColor2, Aspect_GFM_VER, Standard_True);
        m_view->Redraw();
    }

    void QtOccView::SetSelectionMode(int mode) {
        if (m_context.IsNull()) return;

        // Clear all highlights when switching selection modes
        UnhighlightAllVertices();
        UnhighlightAllEdges();
        UnhighlightAllFaces();

        // Clear current selection
        if (!m_currentSelectedAIS.IsNull()) {
            m_context->SetSelected(m_currentSelectedAIS, Standard_False);
            m_currentSelectedAIS.Nullify();
            m_currentSelectedShape.reset();
        }
        m_context->ClearSelected(Standard_False);

        // Store current selection mode
        m_currentSelectionMode = mode;

        qDebug() << "SetSelectionMode called with mode:" << mode;

        // Clear all existing selection modes
        m_context->Deactivate();

        // Activate the specific selection mode
        switch (mode) {
        case 0: // Shape
            m_context->Activate(0, Standard_True);
            qDebug() << "Activated shape selection mode";
            break;
        case 1: // Vertex  
            m_context->Activate(1, Standard_True);
            qDebug() << "Activated vertex selection mode";
            break;
        case 2: // Edge
            m_context->Activate(2, Standard_True);
            qDebug() << "Activated edge selection mode";
            break;
        case 4: // Face
            m_context->Activate(4, Standard_True);
            qDebug() << "Activated face selection mode";
            break;
        default:
            m_context->Activate(0, Standard_True); // Default to shape selection
            m_currentSelectionMode = 0;
            qDebug() << "Activated default shape selection mode";
            break;
        }

        m_view->Redraw();
    }

    void QtOccView::ClearSelection() {
        if (m_context.IsNull()) return;

        // Clear current selection state
        if (!m_currentSelectedAIS.IsNull()) {
            m_context->SetSelected(m_currentSelectedAIS, Standard_False);
            m_currentSelectedAIS.Nullify();
            m_currentSelectedShape.reset();
        }

        // Clear all highlights
        UnhighlightAllEdges();
        UnhighlightAllVertices();
        UnhighlightAllFaces();
        UnhighlightSketchElement();

        m_context->ClearSelected(Standard_True);
        m_view->Redraw();
    }

    void QtOccView::ShowGrid(bool show) {
        if (m_viewer.IsNull()) return;

        if (show) {
            m_viewer->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);
        }
        else {
            m_viewer->DeactivateGrid();
        }
        m_view->Redraw();
    }

    void QtOccView::SetGridSpacing(double spacing) {
        if (m_viewer.IsNull()) return;

        // Simplified grid spacing implementation
        // In a real implementation, you'd set the grid spacing properly
        Q_UNUSED(spacing);

        m_view->Redraw();
    }

    void QtOccView::ShowAxes(bool show) {
        if (m_view.IsNull()) return;

        if (show) {
            m_view->TriedronDisplay(Aspect_TOTP_LEFT_LOWER, Quantity_NOC_GOLD, 0.08, V3d_ZBUFFER);
        }
        else {
            m_view->TriedronErase();
        }
        m_view->Redraw();
    }

    void QtOccView::SetAllTransparency(double transparency) {
        if (m_context.IsNull()) return;

        // Clamp transparency value between 0.0 and 1.0
        transparency = std::max(0.0, std::min(1.0, transparency));

        // Iterate through all displayed AIS_Shape objects
        AIS_ListOfInteractive aList;
        m_context->DisplayedObjects(aList);

        for (AIS_ListOfInteractive::Iterator anIter(aList); anIter.More(); anIter.Next()) {
            Handle(AIS_Shape) aShape = Handle(AIS_Shape)::DownCast(anIter.Value());
            if (!aShape.IsNull()) {
                // Set transparency for the shape
                if (transparency > 0.0) {
                    m_context->SetTransparency(aShape, transparency, Standard_False);
                }
                else {
                    m_context->UnsetTransparency(aShape, Standard_False);
                }
            }
        }

        // Update the view
        m_context->UpdateCurrentViewer();
        m_view->Redraw();
    }

    // Get the currently selected shape
    cad_core::ShapePtr QtOccView::GetCurrentSelectedShape() const {
        return m_currentSelectedShape;
    }

    // Set the transparency of a specific shape
    void QtOccView::SetShapeTransparency(const cad_core::ShapePtr& shape, double transparency) {
        if (!shape || m_context.IsNull()) return;

        // Clamp transparency value between 0.0 and 1.0
        transparency = std::max(0.0, std::min(1.0, transparency));

        // Find the corresponding AIS_Shape in the mapping table
        auto it = m_shapeToAIS.find(shape);
        if (it != m_shapeToAIS.end()) {
            Handle(AIS_Shape) aisShape = it->second;
            if (!aisShape.IsNull()) {
                // Apply transparency
                if (transparency > 0.0) {
                    m_context->SetTransparency(aisShape, transparency, Standard_False);
                }
                else {
                    m_context->UnsetTransparency(aisShape, Standard_False);
                }
                // Update view
                m_context->UpdateCurrentViewer();
                m_view->Redraw();
            }
        }
    }

    void QtOccView::paintEvent(QPaintEvent* event) {
        Q_UNUSED(event);

        if (!m_isInitialized) {
            if (!InitViewer()) {
                // If initialization fails, paint a gray background
                QPainter painter(this);
                painter.fillRect(rect(), QColor(128, 128, 128));
                painter.setPen(Qt::white);
                painter.drawText(rect(), Qt::AlignCenter, "Initializing 3D View...");
                return;
            }
        }

        if (!m_view.IsNull()) {
            // Only redraw, avoid window remapping which can cause flicker
            m_view->Redraw();
        }
    }

    void QtOccView::resizeEvent(QResizeEvent* event) {
        Q_UNUSED(event);

        if (!m_view.IsNull()) {
            m_view->MustBeResized();
        }
    }

    void QtOccView::SetMultiSelectionMode(bool multi) {
        m_multiSelectionMode = multi;
    }

    void QtOccView::mousePressEvent(QMouseEvent* event) {
        m_lastMousePos = event->pos();
        m_currentMouseButton = event->button();

        // If previewing, left-clicking "confirms" this plane
        if (m_sweepInteractionState == SweepInteractionMode::PreviewingPathPlane && event->button() == Qt::LeftButton) {

            // 1. Exit preview state
            m_sweepInteractionState = SweepInteractionMode::None;

            // 2. Destroy that translucent sea-green preview plane
            if (!m_sweepPlanePreview.IsNull()) {
                m_context->Remove(m_sweepPlanePreview, Standard_False);
                m_sweepPlanePreview.Nullify();
            }

            // 3. Formally enter sketch mode (camera instantly aligns)
            EnterSketchMode(m_currentSweepPathCS);

            return;
        }

        // Process sketch mode first
        if (IsInSketchMode() && !m_sketchMode->IsTemporary3DViewActive()) {
            if (HasActiveSketchTool()) {
                m_sketchMode->HandleMousePress(event);
                return;
            }

            if (event->button() == Qt::LeftButton) {
                HandleSelection(event->pos());
                m_sketchMode->HandleMousePress(event);
            }
            return;
        }

        // Non-sketch mode (or in temporary 3D view): Only record state on left click, do not rotate immediately
        if (event->button() == Qt::LeftButton) {
            m_leftPressPos = event->pos();
            m_isLeftDragging = false;
            return;
        }
    }

    void QtOccView::mouseMoveEvent(QMouseEvent* event) {
        if (m_view.IsNull()) return;

        QPoint currentPos = event->pos();
        emit MousePositionChanged(currentPos.x(), currentPos.y());

        Standard_Real X, Y, Z;
        try {
            m_view->Convert(currentPos.x(), currentPos.y(), X, Y, Z);
            emit Mouse3DPositionChanged(X, Y, Z);
        }
        catch (...) {
            X = 0; Y = 0; Z = 0;
        }

        // If in preview plane state, calculate and rotate the plane in real-time
        if (m_sweepInteractionState == SweepInteractionMode::PreviewingPathPlane) {

            // 1. Convert 3D absolute centroid to 2D pixel coordinates (cx, cy) on screen
            Standard_Integer cx, cy;
            m_view->Convert(m_sweepCentroid.X(), m_sweepCentroid.Y(), m_sweepCentroid.Z(), cx, cy);

            // 2. Calculate the difference between current mouse position and the screen coordinates of the centroid
            double dx = currentPos.x() - cx;
            double dy = currentPos.y() - cy;

            // 3. Anti-shake mechanism: If mouse is too close to the center (less than 5 pixels), do not rotate to prevent angle jumping
            if (std::sqrt(dx * dx + dy * dy) > 5.0) {

                // 4. Calculate pure 2D screen rotation angle (radians)
                // atan2 perfectly covers the -180 to 180 degrees range
                double angle = std::atan2(dy, dx);

                // 5. Use the section's normal as the rotation axis, overlaying this angle from the "0 degree reference plane"
                gp_Trsf rot;
                rot.SetRotation(gp_Ax1(m_sweepCentroid, m_sweepProfileNormal), -angle); // Negative sign to match intuitive mouse direction

                m_currentSweepPathCS = m_baseSweepPathCS; // Re-rotate from 0 degrees every time
                m_currentSweepPathCS.Transform(rot);

                // 6. Update and redraw the face
                gp_Pln updatedPln(m_currentSweepPathCS);
                TopoDS_Face updatedFace = BRepBuilderAPI_MakeFace(updatedPln, -5.0, 5.0, -5.0, 5.0).Face();

                Handle(AIS_Shape) aisPlane = Handle(AIS_Shape)::DownCast(m_sweepPlanePreview);
                if (!aisPlane.IsNull()) {
                    aisPlane->SetShape(updatedFace);
                    m_context->Redisplay(aisPlane, Standard_False);
                }
                m_view->Redraw();
            }
            return; // Intercept event, do not propagate further
        }

        // Process sketch mode first
        if (IsInSketchMode() && !m_sketchMode->IsTemporary3DViewActive()) {
            m_sketchMode->HandleMouseMove(event);

            if (!HasActiveSketchTool() && !m_context.IsNull()) {
                m_context->MoveTo(currentPos.x(), currentPos.y(), m_view, Standard_True);

                // Selected objects no longer show hover preview color
                if (m_context->HasDetected()) {
                    Handle(AIS_InteractiveObject) detectedObj = m_context->DetectedInteractive();
                    if (!detectedObj.IsNull()) {
                        bool isAlreadySelected = false;

                        for (m_context->InitSelected(); m_context->MoreSelected(); m_context->NextSelected()) {
                            if (m_context->SelectedInteractive() == detectedObj) {
                                isAlreadySelected = true;
                                break;
                            }
                        }

                        if (isAlreadySelected) {
                            m_context->ClearDetected(Standard_True);
                        }
                    }
                }
            }

            if (m_currentMouseButton == Qt::MiddleButton) {
                QPoint delta = currentPos - m_lastMousePos;
                m_view->Pan(delta.x(), -delta.y());
                m_view->Redraw();
            }
            else if (m_currentMouseButton == Qt::RightButton) {
                QPoint delta = currentPos - m_lastMousePos;
                if (delta.y() != 0) {
                    m_view->SetZoom((delta.y() > 0) ? 0.9 : 1.1);
                    m_view->Redraw();
                }
            }

            m_lastMousePos = currentPos;
            return;
        }

        // Non-sketch mode: Mouse hover detection
        if (m_currentMouseButton == Qt::NoButton && !m_context.IsNull()) {
            m_context->MoveTo(currentPos.x(), currentPos.y(), m_view, Standard_False);

            // Already selected objects no longer show hover preview color
            if (m_context->HasDetected()) {
                Handle(AIS_InteractiveObject) detectedObj = m_context->DetectedInteractive();
                if (!detectedObj.IsNull()) {
                    bool isAlreadySelected = false;

                    for (m_context->InitSelected(); m_context->MoreSelected(); m_context->NextSelected()) {
                        if (m_context->SelectedInteractive() == detectedObj) {
                            isAlreadySelected = true;
                            break;
                        }
                    }

                    if (isAlreadySelected) {
                        m_context->ClearDetected(Standard_False);
                    }
                }
            }
        }

        // Rotate only when left button is held and dragged
        if (m_currentMouseButton == Qt::LeftButton) {
            const int dragThreshold = QApplication::startDragDistance();
            const int moveDistance = (currentPos - m_leftPressPos).manhattanLength();

            if (!m_isLeftDragging && moveDistance >= dragThreshold) {
                m_isLeftDragging = true;
                m_view->StartRotation(m_leftPressPos.x(), m_leftPressPos.y());
            }

            if (m_isLeftDragging) {
                m_view->Rotation(currentPos.x(), currentPos.y());
                m_view->Redraw();
            }
        }
        else if (m_currentMouseButton == Qt::MiddleButton) {
            QPoint delta = currentPos - m_lastMousePos;
            m_view->Pan(delta.x(), -delta.y());
            m_view->Redraw();
        }
        else if (m_currentMouseButton == Qt::RightButton) {
            QPoint delta = currentPos - m_lastMousePos;
            if (delta.y() != 0) {
                double factor = (delta.y() > 0) ? 0.9 : 1.1;
                m_view->SetZoom(factor);
                m_view->Redraw();
            }
        }

        m_lastMousePos = currentPos;
    }

    void QtOccView::mouseReleaseEvent(QMouseEvent* event) {
        // Process sketch mode first
        if (IsInSketchMode() && !m_sketchMode->IsTemporary3DViewActive()) {
            m_sketchMode->HandleMouseRelease(event);
            m_currentMouseButton = Qt::NoButton;
            m_isLeftDragging = false;
            return;
        }

        // Non-sketch mode: On left release, if no dragging occurred, treat as a click selection
        if (event->button() == Qt::LeftButton) {
            if (!m_isLeftDragging) {
                HandleSelection(event->pos());
            }
            m_isLeftDragging = false;
        }

        m_currentMouseButton = Qt::NoButton;
    }

    void QtOccView::keyPressEvent(QKeyEvent* event) {

        // 1. Intercept spacebar first, used to enable temporary 3D view
        if (event->key() == Qt::Key_Space) {
            if (!event->isAutoRepeat() && m_sketchMode && m_sketchMode->IsInSketchMode()) {
                m_sketchMode->StartTemporary3DView();
            }
            event->accept();
            return; // Interception complete, return directly, do not propagate
        }

        // Forward event to sketch mode
        if (m_sketchMode && m_sketchMode->IsInSketchMode()) {
            m_sketchMode->HandleKeyPress(event);
        }

        // 3. Original base class call
        QWidget::keyPressEvent(event);
    }


    void QtOccView::keyReleaseEvent(QKeyEvent* event) {

        // If the spacebar is released
        if (event->key() == Qt::Key_Space) {
            if (!event->isAutoRepeat() && m_sketchMode && m_sketchMode->IsInSketchMode()) {
                m_sketchMode->StopTemporary3DView();
            }
            event->accept();
            return;
        }

        QWidget::keyReleaseEvent(event);
    }

    void QtOccView::wheelEvent(QWheelEvent* event) {
        if (m_view.IsNull()) return;

        const int delta = event->angleDelta().y();
        const double factor = (delta > 0) ? 1.1 : 0.9;

        m_view->SetZoom(factor);
        m_view->Redraw();
    }


    void QtOccView::InitializeOCC() {
        // This is called in constructor, actual initialization happens in InitViewer
    }

    void QtOccView::RedrawView() {
        if (!m_view.IsNull()) {
            m_view->Redraw();
        }
    }


    // Extract object from current OCC selection
    Handle(AIS_InteractiveObject) QtOccView::GetFirstSelectedObject() const {
        m_context->InitSelected();
        if (m_context->MoreSelected()) {
            return m_context->SelectedInteractive();
        }
        return nullptr;
    }

    Handle(StdSelect_BRepOwner) QtOccView::GetFirstSelectedOwner() const {
        m_context->InitSelected();
        if (m_context->MoreSelected()) {
            return Handle(StdSelect_BRepOwner)::DownCast(m_context->SelectedOwner());
        }
        return nullptr;
    }

    // Specifically clear old states
    void QtOccView::ClearPreviousSelectionState() {
        if (m_multiSelectionMode) {
            return;
        }

        if (m_currentSelectionMode != 2) { // Edge mode supports multi-selection, do not clear edges here
            UnhighlightAllVertices();
            UnhighlightAllFaces();
            UnhighlightSketchElement();

            // In normal single selection mode, clear previously recorded shape selection state
            if (!m_currentSelectedAIS.IsNull()) {
                m_context->SetSelected(m_currentSelectedAIS, Standard_False);
                m_currentSelectedAIS.Nullify();
                m_currentSelectedShape.reset();
            }

            // In face / vertex / normal single selection mode, clear OCC's current selection pool
            m_context->ClearSelected(Standard_False);
        }
    }

    // =========================================================================
    // Main entry: Handle selection dispatch
    // =========================================================================
    void QtOccView::HandleSelection(const QPoint& point) {
        if (m_context.IsNull()) return;

        // Detect Ctrl key: Automatically enter multi-selection mode when Ctrl is held
        bool ctrlHeld = QApplication::keyboardModifiers() & Qt::ControlModifier;
        bool wasMultiMode = m_multiSelectionMode;
        if (ctrlHeld) {
            m_multiSelectionMode = true;
        }

        // 1. Clear previous selection state (skipped in multi-selection mode)
        ClearPreviousSelectionState();

        // 2. Detect objects under mouse position
        m_context->MoveTo(point.x(), point.y(), m_view, Standard_True);

        if (m_context->HasDetected()) {
            if (m_multiSelectionMode) {
                m_context->ShiftSelect(Standard_True);
            }
            else {
                m_context->Select(Standard_True);
            }

            // 3. Process based on current selection mode
            switch (m_currentSelectionMode) {
            case 2: ProcessEdgeSelection(); break;
            case 1: ProcessVertexSelection(); break;
            case 4: ProcessFaceSelection(); break;
            default: ProcessShapeOrSketchSelection(); break;
            }
        }
        else {
            qDebug() << "No object detected, clearing all selections";
            if (m_currentSelectionMode != 2) {
                UnhighlightAllEdges();
            }
            // When clicking in an empty area, if not in multi-selection mode, clear all sketch highlights
            if (!m_multiSelectionMode) {
                UnhighlightSketchElement();
            }
            ClearCentroid();
        }

        // Restore multi-selection mode state (if temporarily enabled by Ctrl)
        if (ctrlHeld && !wasMultiMode) {
            // Do not restore, keep multi-selection state until user clicks empty space
            // m_multiSelectionMode = wasMultiMode;
        }

        m_view->Redraw();
        emit ViewChanged();
    }

    // Sub-mode processing functions
    void QtOccView::ProcessEdgeSelection() {
        qDebug() << "Edge selection mode detected, attempting to select edge...";

        int selectedCount = 0;
        // Edge mode may involve multi-selection, so keep the loop iteration
        for (m_context->InitSelected(); m_context->MoreSelected(); m_context->NextSelected()) {
            selectedCount++;
            Handle(AIS_InteractiveObject) anIO = m_context->SelectedInteractive();
            Handle(AIS_Shape) aisShape = Handle(AIS_Shape)::DownCast(anIO);

            if (!aisShape.IsNull()) {
                cad_core::ShapePtr parentShape = nullptr;
                for (const auto& pair : m_shapeToAIS) {
                    if (pair.second == aisShape) {
                        parentShape = pair.first;
                        break;
                    }
                }

                if (!parentShape) {
                    qDebug() << "Could not find parent shape for selected edge";
                    continue;
                }

                Handle(StdSelect_BRepOwner) anOwner = Handle(StdSelect_BRepOwner)::DownCast(m_context->SelectedOwner());
                if (!anOwner.IsNull()) {
                    TopoDS_Shape selectedShape = anOwner->Shape();
                    if (selectedShape.ShapeType() == TopAbs_EDGE) {
                        TopoDS_Edge edge = TopoDS::Edge(selectedShape);

                        bool alreadySelected = false;
                        for (const auto& existingEdge : m_selectedEdges) {
                            if (edge.IsSame(existingEdge)) {
                                alreadySelected = true;
                                break;
                            }
                        }

                        if (!alreadySelected) {
                            m_selectedEdges.push_back(edge);
                            m_edgeParentShapes.push_back(parentShape);
                            qDebug() << "Added edge to selection, total edges:" << m_selectedEdges.size();
                            HighlightEdge(edge);
                        }
                        else {
                            qDebug() << "Edge already selected";
                        }
                    }
                }
            }
        }

        if (selectedCount == 0) {
            qDebug() << "No objects selected in context";
        }
    }

    void QtOccView::ProcessVertexSelection() {
        qDebug() << "Vertex selection mode detected, attempting to select vertex...";

        // Vertices are generally single-selected, use utility function to get the first object directly
        Handle(AIS_Shape) aisShape = Handle(AIS_Shape)::DownCast(GetFirstSelectedObject());
        if (!aisShape.IsNull()) {
            Handle(StdSelect_BRepOwner) anOwner = GetFirstSelectedOwner();
            if (!anOwner.IsNull()) {
                TopoDS_Shape selectedShape = anOwner->Shape();
                if (selectedShape.ShapeType() == TopAbs_VERTEX) {
                    TopoDS_Vertex vertex = TopoDS::Vertex(selectedShape);
                    HighlightVertex(vertex);
                    qDebug() << "Vertex selected";
                }
            }
        }
    }

    void QtOccView::ProcessFaceSelection() {
        qDebug() << "Face selection mode detected, attempting to select face...";

        // Faces are generally single-selected, use utility function to get the first object directly
        Handle(AIS_Shape) aisShape = Handle(AIS_Shape)::DownCast(m_context->DetectedInteractive());
        if (!aisShape.IsNull()) {
            Handle(StdSelect_BRepOwner) anOwner = Handle(StdSelect_BRepOwner)::DownCast(m_context->DetectedOwner());
            if (!anOwner.IsNull()) {
                TopoDS_Shape selectedShape = anOwner->Shape();
                if (selectedShape.ShapeType() == TopAbs_FACE) {
                    TopoDS_Face face = TopoDS::Face(selectedShape);
                    qDebug() << "Face selected, emitting FaceSelected signal";
                    emit FaceSelected(face);
                }
            }
        }
    }

    void QtOccView::ProcessShapeOrSketchSelection() {
        Handle(AIS_InteractiveObject) selectedObj = m_context->DetectedInteractive();

        if (!selectedObj.IsNull()) {
            Handle(AIS_Shape) aisShape = Handle(AIS_Shape)::DownCast(selectedObj);

            // 1. Clicked on a closed sketch profile
            if (m_sketchProfileMap.find(selectedObj) != m_sketchProfileMap.end()) {
                cad_core::ShapePtr profileShape = m_sketchProfileMap[selectedObj];

                m_currentSelectedAIS = aisShape;
                m_currentSelectedShape = profileShape;

                // Sweep dynamic plane preview logic 

                if (m_sweepInteractionState == SweepInteractionMode::SelectingProfile) {

                    if (profileShape && !profileShape->GetOCCTShape().IsNull()) {
                        m_sweepCentroid = profileShape->GetCentroid();

                        TopoDS_Face face = TopoDS::Face(profileShape->GetOCCTShape());
                        Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
                        Handle(Geom_Plane) plane = Handle(Geom_Plane)::DownCast(surface);

                        if (!plane.IsNull()) {
                            gp_Ax3 profileCS = plane->Pln().Position();
                            m_sweepProfileNormal = profileCS.Direction(); // Record original section normal
                            gp_Dir profileX = profileCS.XDirection();

                            // Switch interaction state to: Previewing Path Plane
                            m_sweepInteractionState = SweepInteractionMode::PreviewingPathPlane;

                            // Generate initial path coordinate system (centroid as origin, original normal as X-axis, original X-axis as normal)
                            m_baseSweepPathCS = gp_Ax3(m_sweepCentroid, profileX, m_sweepProfileNormal);
                            m_currentSweepPathCS = m_baseSweepPathCS;

                            // Generate a plane face for visual preview
                            gp_Pln previewPln(m_currentSweepPathCS);
                            // Shrink UV bounds to make the plane smaller
                            TopoDS_Face previewFace = BRepBuilderAPI_MakeFace(previewPln, -5.0, 5.0, -5.0, 5.0).Face();

                            // Wrap into a displayable AIS object (AIS_Shape)
                            Handle(AIS_Shape) aisPlane = new AIS_Shape(previewFace);
                            aisPlane->SetColor(Quantity_NOC_LIGHTSEAGREEN); // Sea green, giving a high-tech feel
                            aisPlane->SetTransparency(0.6);                 // Translucent, doesn't block the model
                            aisPlane->SetDisplayMode(AIS_Shaded);           // Solid shaded mode
                            aisPlane->SetZLayer(Graphic3d_ZLayerId_Topmost);// Ensure it is displayed on the top layer, not blocked by the section

                            m_sweepPlanePreview = aisPlane;
                            m_context->Display(m_sweepPlanePreview, Standard_False);
                            m_view->Redraw();

                            qDebug() << "Entered Sweep Plane Preview State.";
                        }
                    }
                }

                // Emit signal to UI panel
                emit ShapeSelected(profileShape);
                qDebug() << "Sketch Profile selected natively.";
            }

            // 2. Clicked on a sketch element 
            else if (m_sketchElementMap.find(selectedObj) != m_sketchElementMap.end()) {
                if (m_multiSelectionMode) {
                    // Multi-selection mode: do not clear old highlights, append new highlight to the list
                    Handle(AIS_Shape) highlight = new AIS_Shape(aisShape->Shape());
                    highlight->SetColor(Quantity_NOC_BLUE1);
                    highlight->SetWidth(4.0);
                    highlight->SetPolygonOffsets(Aspect_POM_Line, 1.0f, -2.0f);
                    m_context->Display(highlight, Standard_False);
                    m_sketchHighlightList.push_back(highlight);
                }
                else {
                    // Single-selection mode: clear old highlights, keep only one
                    UnhighlightSketchElement();
                    m_sketchHighlightAIS = new AIS_Shape(aisShape->Shape());
                    m_sketchHighlightAIS->SetColor(Quantity_NOC_BLUE1);
                    m_sketchHighlightAIS->SetWidth(4.0);
                    m_sketchHighlightAIS->SetPolygonOffsets(Aspect_POM_Line, 1.0f, -2.0f);
                    m_context->Display(m_sketchHighlightAIS, Standard_False);
                }

                m_currentSelectedAIS = aisShape;
                qDebug() << "Sketch element selected" << (m_multiSelectionMode ? "(multi)" : "(single)");
            }

            // 3. Clicked on a normal 3D entity 
            else {
                cad_core::ShapePtr foundShape = nullptr;
                for (const auto& pair : m_shapeToAIS) {
                    if (pair.second == aisShape) {
                        foundShape = pair.first;
                        break;
                    }
                }

                m_currentSelectedAIS = aisShape;
                m_currentSelectedShape = foundShape;

                if (foundShape) {
                    emit ShapeSelected(foundShape);
                    qDebug() << "Normal model shape selected natively.";
                }
            }
        }
    }


    void QtOccView::OnRedrawTimer() {
        RedrawView();
    }

    // Selection mode settings
    void QtOccView::SetSelectionMode(cad_core::SelectionMode mode) {
        if (m_selectionManager) {
            m_selectionManager->SetSelectionMode(mode);
        }
    }

    // Get selection results
    std::vector<cad_core::SelectionInfo> QtOccView::GetSelectedShapes() const {
        if (m_selectionManager) {
            return m_selectionManager->GetSelectedShapes();
        }
        return std::vector<cad_core::SelectionInfo>();
    }

    std::vector<cad_core::SelectionInfo> QtOccView::GetSelectedFaces() const {
        if (m_selectionManager) {
            return m_selectionManager->GetSelectedFaces();
        }
        return std::vector<cad_core::SelectionInfo>();
    }

    std::vector<cad_core::SelectionInfo> QtOccView::GetSelectedEdges() const {
        if (m_selectionManager) {
            return m_selectionManager->GetSelectedEdges();
        }
        return std::vector<cad_core::SelectionInfo>();
    }

    std::vector<cad_core::SelectionInfo> QtOccView::GetSelectedVertices() const {
        if (m_selectionManager) {
            return m_selectionManager->GetSelectedVertices();
        }
        return std::vector<cad_core::SelectionInfo>();
    }

    // Fix for view turning white when window loses focus
    void QtOccView::focusInEvent(QFocusEvent* event) {
        QWidget::focusInEvent(event);

        // Minimal focus handling to prevent flicker
        if (!m_view.IsNull() && m_isInitialized) {
            // Don't touch window mapping - let it be handled by showEvent and paintEvent
            qDebug() << "Focus gained - view is initialized";
        }
    }

    void QtOccView::focusOutEvent(QFocusEvent* event) {
        QWidget::focusOutEvent(event);

        // Minimal handling - don't force redraws on focus loss
        // This reduces flicker when mouse enters/leaves the widget
    }

    void QtOccView::enterEvent(QEvent* event) {
        QWidget::enterEvent(event);

        // Don't perform any heavy operations on mouse enter
        // This prevents flicker when mouse enters the widget
    }

    void QtOccView::leaveEvent(QEvent* event) {
        QWidget::leaveEvent(event);

        // Don't perform any heavy operations on mouse leave
        // This prevents flicker when mouse leaves the widget
    }

    void QtOccView::showEvent(QShowEvent* event) {
        QWidget::showEvent(event);

        // Try to initialize if not done yet
        if (!m_isInitialized) {
            InitViewer();
            return; // InitViewer will handle the initial redraw
        }

        // Minimal redraw when widget is shown - only if necessary
        if (!m_view.IsNull()) {
            m_view->MustBeResized();
            m_view->Redraw();
        }
    }

    // Add event handler for window activation changes
    void QtOccView::changeEvent(QEvent* event) {
        QWidget::changeEvent(event);

        if (event->type() == QEvent::ActivationChange ||
            event->type() == QEvent::WindowStateChange ||
            event->type() == QEvent::WindowActivate ||
            event->type() == QEvent::WindowDeactivate) {

            if (!m_view.IsNull() && !m_context.IsNull()) {
                // Force maintain viewer state regardless of activation
                m_context->UpdateCurrentViewer();
                m_view->Redraw();
            }
        }
    }

    // Select shape programmatically for document tree synchronization
    void QtOccView::SelectShape(const cad_core::ShapePtr& shape) {
        if (!shape || m_context.IsNull()) {
            return;
        }

        // Clear any previous selection first (single selection mode)
        if (!m_currentSelectedAIS.IsNull()) {
            m_context->SetSelected(m_currentSelectedAIS, Standard_False);
            m_currentSelectedAIS.Nullify();
            m_currentSelectedShape.reset();
        }

        // Find the AIS_Shape corresponding to this shape
        auto it = m_shapeToAIS.find(shape);
        if (it != m_shapeToAIS.end()) {
            Handle(AIS_Shape) aisShape = it->second;
            if (!aisShape.IsNull()) {
                // Set new selection with highlighting
                m_context->SetSelected(aisShape, Standard_True);
                m_context->HilightSelected(Standard_True);
                m_currentSelectedAIS = aisShape;
                m_currentSelectedShape = shape;

                // Redraw to show selection
                m_view->Redraw();
            }
        }
    }

    // Edge selection methods for fillet/chamfer operations
    void QtOccView::ClearEdgeSelection() {
        if (m_context.IsNull()) return;

        // Remove all edge highlights
        UnhighlightAllEdges();

        // Clear edge lists and parent shape tracking
        m_selectedEdges.clear();
        m_highlightedEdges.clear();
        m_edgeParentShapes.clear();

        m_view->Redraw();
    }

    std::map<cad_core::ShapePtr, std::vector<TopoDS_Edge>> QtOccView::GetSelectedEdgesByShape() const {
        std::map<cad_core::ShapePtr, std::vector<TopoDS_Edge>> result;

        // Group edges by their parent shapes using parallel vectors
        for (size_t i = 0; i < m_selectedEdges.size() && i < m_edgeParentShapes.size(); ++i) {
            const TopoDS_Edge& edge = m_selectedEdges[i];
            const cad_core::ShapePtr& parentShape = m_edgeParentShapes[i];

            if (parentShape) {
                result[parentShape].push_back(edge);
            }
        }

        return result;
    }


    // Only for edge multi-selection or special business previews
    // Normal hover / selection prefers OCC's built-in LocalDynamic / LocalSelected
    void QtOccView::HighlightEdge(const TopoDS_Edge& edge) {
        if (m_context.IsNull()) return;

        // Create AIS object for edge highlighting
        Handle(AIS_Shape) aisEdge = new AIS_Shape(edge);

        // Set edge highlighting properties - make it thicker and colored
        Handle(Prs3d_Drawer) drawer = aisEdge->Attributes();
        drawer->SetLineAspect(new Prs3d_LineAspect(Quantity_NOC_RED, Aspect_TOL_SOLID, 3.0));
        drawer->SetWireAspect(new Prs3d_LineAspect(Quantity_NOC_RED, Aspect_TOL_SOLID, 3.0));

        // Display the highlighted edge
        m_context->Display(aisEdge, Standard_False);
        m_highlightedEdges.push_back(aisEdge);

        m_view->Redraw();
    }

    void QtOccView::UnhighlightAllEdges() {
        if (m_context.IsNull()) return;

        // Remove all highlighted edges from display
        for (const auto& highlightedEdge : m_highlightedEdges) {
            m_context->Remove(highlightedEdge, Standard_False);
        }

        m_highlightedEdges.clear();
        m_view->Redraw();
    }


    // Only for special point selection or debug previews
    // Normal hover / selection prefers OCC's built-in LocalDynamic / LocalSelected
    void QtOccView::HighlightVertex(const TopoDS_Vertex& vertex) {
        if (m_context.IsNull()) return;

        // Use a temporary AIS object to display the highlighted point
        Handle(AIS_Shape) aisVertex = new AIS_Shape(vertex);

        // Set point highlight properties - red sphere
        aisVertex->SetColor(Quantity_NOC_RED);
        aisVertex->SetWidth(5.0);

        // Display highlighted point
        m_context->Display(aisVertex, Standard_False);

        // Add to selected points list
        bool alreadySelected = false;
        for (const auto& existingVertex : m_selectedVertices) {
            if (vertex.IsSame(existingVertex)) {
                alreadySelected = true;
                break;
            }
        }

        if (!alreadySelected) {
            m_selectedVertices.push_back(vertex);
            m_highlightedVertices.push_back(aisVertex);
            qDebug() << "Added vertex to selection, total vertices:" << m_selectedVertices.size();
        }

        m_view->Redraw();
    }

    void QtOccView::UnhighlightAllVertices() {
        if (m_context.IsNull()) return;

        // Remove all highlighted vertices from display
        for (const auto& highlightedVertex : m_highlightedVertices) {
            m_context->Remove(highlightedVertex, Standard_False);
        }

        m_highlightedVertices.clear();
        m_selectedVertices.clear();
        m_view->Redraw();
    }


    // Only for special business previews (e.g., adjacent face preview, auxiliary hints)
    // No longer used as the default highlight method for normal face clicking
    void QtOccView::HighlightFace(const TopoDS_Face& face) {
        if (m_context.IsNull()) return;

        // Use a temporary AIS object to display the highlighted face
        Handle(AIS_Shape) aisFace = new AIS_Shape(face);

        // Set face highlight properties - translucent red
        aisFace->SetColor(Quantity_NOC_RED);
        aisFace->SetTransparency(0.3); // Translucent
        aisFace->SetDisplayMode(AIS_Shaded);

        aisFace->SetZLayer(Graphic3d_ZLayerId_Topmost);

        // Display highlighted face
        m_context->Display(aisFace, Standard_False);

        // Add to selected faces list
        bool alreadySelected = false;
        for (const auto& existingFace : m_selectedFaces) {
            if (face.IsSame(existingFace)) {
                alreadySelected = true;
                break;
            }
        }

        if (!alreadySelected) {
            m_selectedFaces.push_back(face);
            m_highlightedFaces.push_back(aisFace);
            qDebug() << "Added face to selection, total faces:" << m_selectedFaces.size();
        }

        m_view->Redraw();
    }

    void QtOccView::UnhighlightAllFaces() {
        if (m_context.IsNull()) return;

        // Remove all highlighted faces from display
        for (const auto& highlightedFace : m_highlightedFaces) {
            m_context->Remove(highlightedFace, Standard_False);
        }

        m_highlightedFaces.clear();
        m_selectedFaces.clear();
        m_view->Redraw();
    }

    void QtOccView::ClearOpFace() {
        if (!m_previewFaceAIS.IsNull() && !m_context.IsNull()) {
            m_context->Remove(m_previewFaceAIS, Standard_False);
            m_previewFaceAIS.Nullify();
            m_view->Redraw();
        }
    }

    void QtOccView::ShowOpFace(int faceIndex) {
        ClearOpFace(); // Clear old ones first

        // Ensure there is a currently selected edge and its corresponding parent entity
        if (m_selectedEdges.empty() || m_edgeParentShapes.empty()) return;

        TopoDS_Edge currentEdge = m_selectedEdges[0];
        cad_core::ShapePtr parentShape = m_edgeParentShapes[0]; // Directly use existing parent shape records

        if (!parentShape) return;

        std::vector<TopoDS_Face> adjacentFaces = cad_core::FilletChamferOperations::GetAdjacentFaces(parentShape, currentEdge);

        TopoDS_Face faceToHighlight;
        if (faceIndex == 0 && adjacentFaces.size() > 0) {
            faceToHighlight = adjacentFaces[0];
        }
        else if (faceIndex == 1 && adjacentFaces.size() > 1) {
            faceToHighlight = adjacentFaces[1];
        }

        if (!faceToHighlight.IsNull()) {
            // Reuse HighlightFace render style
            Handle(AIS_Shape) aisFace = new AIS_Shape(faceToHighlight);
            aisFace->SetColor(Quantity_NOC_RED);
            aisFace->SetTransparency(0.3);
            aisFace->SetDisplayMode(AIS_Shaded);

            m_context->Display(aisFace, Standard_False);
            m_previewFaceAIS = aisFace;

            m_view->Redraw();
        }
    }

    // =============================================================================
    // Sketch Mode Implementation
    // =============================================================================
    namespace {
        /**
         * @brief Convert sketch local 2D point to world 3D point
         */
        static gp_Pnt Sketch2DToWorld(const cad_sketch::SketchPointPtr& pt, const gp_Ax3& cs)
        {
            if (!pt) return gp_Pnt(0, 0, 0);

            gp_Pnt origin = cs.Location();
            gp_Dir xDir = cs.XDirection();
            gp_Dir yDir = cs.YDirection();

            return origin.Translated(
                gp_Vec(xDir) * pt->GetX() +
                gp_Vec(yDir) * pt->GetY()
            );
        }

        // Generate corresponding OCC topological shape based on element type
        static TopoDS_Shape MakeShapeFromSketchElement(const cad_sketch::SketchElementPtr& elem, const gp_Ax3& cs) {
            if (!elem) return TopoDS_Shape();

            if (auto point = std::dynamic_pointer_cast<cad_sketch::SketchPoint>(elem)) {
                gp_Pnt worldPt = Sketch2DToWorld(point, cs);
                return BRepBuilderAPI_MakeVertex(worldPt); // Generate OCC vertex
            }
            // 1. If it's a line
            else if (auto line = std::dynamic_pointer_cast<cad_sketch::SketchLine>(elem)) {
                gp_Pnt p1 = Sketch2DToWorld(line->GetStartPoint(), cs);
                gp_Pnt p2 = Sketch2DToWorld(line->GetEndPoint(), cs);
                if (p1.Distance(p2) > Precision::Confusion()) {
                    return BRepBuilderAPI_MakeEdge(p1, p2);
                }
            }
            // 2. If it's a circle
            else if (auto circle = std::dynamic_pointer_cast<cad_sketch::SketchCircle>(elem)) {
                gp_Pnt center = Sketch2DToWorld(circle->GetCenter(), cs);
                double radius = circle->GetRadius();
                if (radius > Precision::Confusion()) {
                    // Construct a plane coordinate system using current sketch normal direction and X-axis direction
                    gp_Ax2 ax2(center, cs.Direction(), cs.XDirection());
                    gp_Circ gpCirc(ax2, radius);
                    return BRepBuilderAPI_MakeEdge(gpCirc); // OCC generates circular edge
                }
            }
            // 3. If it's an arc
            else if (auto arc = std::dynamic_pointer_cast<cad_sketch::SketchArc>(elem)) {
                gp_Pnt center = Sketch2DToWorld(arc->GetCenter(), cs);
                double radius = arc->GetRadius();
                if (radius > Precision::Confusion()) {
                    // Construct a plane coordinate system using current sketch normal direction and X-axis direction 
                    gp_Ax2 ax2(center, cs.Direction(), cs.XDirection());
                    gp_Circ gpCirc(ax2, radius); // Construct geometric base circle

                    // Get the start and end radians of the arc 
                    double startAngle = arc->GetStartAngle();
                    double endAngle = arc->GetEndAngle();

                    // OCC creates arc edge: drawn counter-clockwise along the base circle from start parameter to end parameter
                    return BRepBuilderAPI_MakeEdge(gpCirc, startAngle, endAngle);
                }
            }

            // 4. If it's a spline curve
            else if (auto curve = std::dynamic_pointer_cast<cad_sketch::SketchCurve>(elem)) {
                const auto& rawPoints = curve->GetControlPoints();

                // 1. Data cleaning: filter out points that are too close
                std::vector<gp_Pnt> validPoints;
                const double MIN_DISTANCE = 1e-5; // Set a minimum tolerance distance

                for (const auto& pt : rawPoints) {
                    gp_Pnt worldPt = Sketch2DToWorld(pt, cs);
                    // If it's the first point, or the distance between current and last valid point is > min tolerance, add to valid list
                    if (validPoints.empty() || validPoints.back().Distance(worldPt) > MIN_DISTANCE) {
                        validPoints.push_back(worldPt);
                    }
                }

                // 2. Need at least two valid points for interpolation 
                if (validPoints.size() >= 2) {
                    // OCC array indices start from 1
                    Handle(TColgp_HArray1OfPnt) occPoints = new TColgp_HArray1OfPnt(1, validPoints.size());

                    for (size_t i = 0; i < validPoints.size(); ++i) {
                        occPoints->SetValue(i + 1, validPoints[i]);
                    }

                    // 3. Catch exceptions thrown by the OCC underlying layer 
                    try {
                        GeomAPI_Interpolate interpolator(occPoints, Standard_False, Precision::Confusion());
                        interpolator.Perform();

                        if (interpolator.IsDone()) {
                            Handle(Geom_BSplineCurve) splineCurve = interpolator.Curve();
                            return BRepBuilderAPI_MakeEdge(splineCurve);
                        }
                    }
                    catch (...) { // Catch Standard_Failure or other underlying exceptions
                        qDebug() << "Curve interpolation failed due to invalid geometric input.";
                        // Return empty shape on exception, just discard this frame's rendering, never let the program crash
                        return TopoDS_Shape();
                    }
                }
            }
            return TopoDS_Shape();
        }

        // Used to track the blue profile faces owned by each sketch to prevent mutual accidental deletion
        static std::map<cad_sketch::Sketch*, std::vector<Handle(AIS_InteractiveObject)>> s_sketchProfileCache;
    }


    bool QtOccView::IsInSketchMode() const {
        return m_sketchMode && m_sketchMode->IsInSketchMode();
    }

    void QtOccView::EnterSketchMode(const TopoDS_Face& face) {
        // Lazy initialization of sketch mode
        if (!m_sketchMode) {
            try {
                m_sketchMode = std::make_unique<SketchMode>(this, this);

                // Connect sketch mode signals
                if (m_sketchMode) {
                    connect(m_sketchMode.get(), &SketchMode::sketchModeEntered,
                        this, &QtOccView::SketchModeEntered);
                    connect(m_sketchMode.get(), &SketchMode::sketchModeExited,
                        this, &QtOccView::SketchModeExited);
                    connect(m_sketchMode.get(), &SketchMode::sketchHistoryChanged,
                        this, &QtOccView::SketchHistoryChanged);
                    connect(m_sketchMode.get(), &SketchMode::toolChanged,
                        this, &QtOccView::SketchToolChanged);

                }
                qDebug() << "Sketch mode initialized successfully";
            }
            catch (const std::exception& e) {
                qDebug() << "Failed to initialize sketch mode:" << e.what();
                return;
            }
        }

        try {
            if (m_sketchMode->EnterSketchMode(face)) {
                qDebug() << "Successfully entered sketch mode";
                if (!m_context.IsNull()) {
                    m_context->ClearSelected(Standard_False);
                    m_context->Deactivate();   // Disable normal model selection
                    // Re-enable selection capability for current sketch elements
                    for (const auto& obj : m_sketchObjects) {
                        if (!obj.IsNull()) {
                            m_context->SetSelectionModeActive(obj, 0, Standard_True);
                        }
                    }

                    // Default to single object selection handling in sketch mode
                    m_currentSelectionMode = 0;
                }
                emit SketchModeEntered();
            }
            else {
                qDebug() << "Failed to enter sketch mode";
            }
        }
        catch (const std::exception& e) {
            qDebug() << "Exception in EnterSketchMode:" << e.what();
        }
    }

    // Enter sketch based on mathematical coordinate system 
    void QtOccView::EnterSketchMode(const gp_Ax3& customCS) {
        // Lazy initialization
        if (!m_sketchMode) {
            try {
                m_sketchMode = std::make_unique<SketchMode>(this, this);
                connect(m_sketchMode.get(), &SketchMode::sketchModeEntered, this, &QtOccView::SketchModeEntered);
                connect(m_sketchMode.get(), &SketchMode::sketchModeExited, this, &QtOccView::SketchModeExited);
                connect(m_sketchMode.get(), &SketchMode::sketchHistoryChanged, this, &QtOccView::SketchHistoryChanged);
                connect(m_sketchMode.get(), &SketchMode::toolChanged, this, &QtOccView::SketchToolChanged);
            }
            catch (...) { return; }
        }

        if (m_sketchMode->EnterSketchMode(customCS)) {
            if (!m_context.IsNull()) {
                m_context->ClearSelected(Standard_False);
                m_context->Deactivate(); // Disable normal model selection
                m_currentSelectionMode = 0;
            }
            emit SketchModeEntered();
        }
    }

    void QtOccView::EditSketch(const std::shared_ptr<cad_sketch::Sketch>& sketch) {
        if (m_sketchMode) {
            m_sketchMode->EditSketch(sketch);
        }
    }

    void QtOccView::ExitSketchMode() {
        if (!m_sketchMode) {
            return;
        }

        try {
            m_sketchMode->ExitSketchMode();

            qDebug() << "Exited sketch mode";
            emit SketchModeExited();
        }
        catch (const std::exception& e) {
            qDebug() << "Exception in ExitSketchMode:" << e.what();
        }
    }

    // Start sketch tools
    void QtOccView::StartRectangleTool() {
        if (!m_sketchMode || !m_sketchMode->IsInSketchMode()) {
            qDebug() << "Cannot start rectangle tool: not in sketch mode";
            return;
        }

        m_sketchMode->StartRectangleTool();
        qDebug() << "Started rectangle tool";
    }

    void QtOccView::StartPointTool() {
        if (m_sketchMode && m_sketchMode->IsInSketchMode()) {
            m_sketchMode->StartPointTool();
        }
    }

    void QtOccView::StartLineTool() {
        if (m_sketchMode && m_sketchMode->IsInSketchMode()) {
            m_sketchMode->StartLineTool();
        }
    }

    void QtOccView::StartCircleTool() {
        if (m_sketchMode && m_sketchMode->IsInSketchMode()) {
            m_sketchMode->StartCircleTool();
        }
    }

    void QtOccView::StartArcTool() {
        if (m_sketchMode && m_sketchMode->IsInSketchMode()) {
            m_sketchMode->StartArcTool();
        }
    }

    void QtOccView::StartCurveTool() {
        if (m_sketchMode && m_sketchMode->IsInSketchMode()) {
            m_sketchMode->StartCurveTool();
        }
    }

    // Highlight selected sketch face
    void QtOccView::HighlightSketchFace(const TopoDS_Face& face) {
        if (m_context.IsNull() || face.IsNull()) return;

        // 1. Clear any possible old highlights
        ClearSketchFaceHighlight();

        // 2. Wrap the selected topological face into a displayable AIS_Shape
        Handle(AIS_Shape) aisFace = new AIS_Shape(face);

        // 3. Set visual effect (light blue + translucent)
        aisFace->SetColor(Quantity_NOC_LIGHTSKYBLUE1); // Light sky blue
        aisFace->SetTransparency(0.6);

        // 4. Enhance face boundary lines (Boundary Draw) to make edges clearer
        Handle(Prs3d_Drawer) drawer = aisFace->Attributes();
        drawer->SetFaceBoundaryDraw(Standard_True);
        drawer->FaceBoundaryAspect()->SetColor(Quantity_NOC_BLUE1); // Use dark blue for boundary lines
        drawer->FaceBoundaryAspect()->SetWidth(2.0);                // Slightly thicken

        // 5. Display and save reference
        m_context->Display(aisFace, Standard_False);
        m_context->Deactivate(aisFace);
        m_highlightedFace = aisFace;
        aisFace->SetPolygonOffsets(Aspect_POM_Fill, 1.0f, -2.0f);
        m_view->Redraw();
    }

    void QtOccView::ClearSketchFaceHighlight() {
        if (m_context.IsNull() || m_highlightedFace.IsNull()) return;

        m_context->Remove(m_highlightedFace, Standard_False);
        m_highlightedFace.Nullify(); // Clear handle
        m_view->Redraw();
    }

    // Show preview lines (usually for cyan feedback during mouse movement)
    void QtOccView::ShowSketchPreviewElements(const std::vector<cad_sketch::SketchElementPtr>& elements, const gp_Ax3& sketchCS) {
        if (m_context.IsNull()) return;
        ClearSketchPreview();

        for (const auto& elem : elements) {
            TopoDS_Shape shape = MakeShapeFromSketchElement(elem, sketchCS);
            if (shape.IsNull()) continue;

            Handle(AIS_Shape) aisLine = new AIS_Shape(shape);
            aisLine->SetColor(Quantity_NOC_CYAN1);
            aisLine->SetWidth(2.0);
            m_context->Display(aisLine, Standard_False);
            m_sketchPreviewObjects.push_back(aisLine);
        }
        m_view->Redraw();
    }

    // Add formal sketch lines (lines confirmed and left on screen after clicking)
    void QtOccView::AddSketchElements(const std::vector<cad_sketch::SketchElementPtr>& elements, const gp_Ax3& sketchCS) {
        if (m_context.IsNull()) return;

        // Helper lambda: add a visual small dot for an endpoint
        auto addEndpointMarker = [&](const cad_sketch::SketchPointPtr& point) {
            if (!point) return;
            // Check if this point is already registered (avoid duplicates, the same point might be shared by two lines)
            for (const auto& pair : m_sketchElementMap) {
                if (pair.second == point) return; // Already registered
            }
            gp_Pnt worldPt = Sketch2DToWorld(point, sketchCS);
            TopoDS_Shape vtx = BRepBuilderAPI_MakeVertex(worldPt);
            Handle(AIS_Shape) aisPoint = new AIS_Shape(vtx);
            aisPoint->SetColor(Quantity_NOC_RED);
            aisPoint->SetWidth(6.0);
            aisPoint->SetPolygonOffsets(Aspect_POM_Point, 1.0f, -3.0f);
            m_context->Display(aisPoint, Standard_False);
            m_sketchElementMap[aisPoint] = point;  // Map to SketchPoint
            m_sketchObjects.push_back(aisPoint);
            };

        for (const auto& elem : elements) {
            TopoDS_Shape shape = MakeShapeFromSketchElement(elem, sketchCS);
            if (shape.IsNull()) continue;

            Handle(AIS_Shape) aisLine = new AIS_Shape(shape);
            aisLine->SetColor(Quantity_NOC_RED);
            aisLine->SetWidth(2.0);
            aisLine->SetPolygonOffsets(Aspect_POM_Line, 1.0f, -2.0f);
            m_context->Display(aisLine, Standard_False);
            m_sketchElementMap[aisLine] = elem;
            m_sketchObjects.push_back(aisLine);

            // Add clickable markers for line segment endpoints
            if (auto line = std::dynamic_pointer_cast<cad_sketch::SketchLine>(elem)) {
                addEndpointMarker(line->GetStartPoint());
                addEndpointMarker(line->GetEndPoint());
            }
            // Add markers for arc endpoints
            else if (auto arc = std::dynamic_pointer_cast<cad_sketch::SketchArc>(elem)) {
                addEndpointMarker(arc->GetStartPoint());
                addEndpointMarker(arc->GetEndPoint());
            }
            // Also add a marker to circle centers (for easier selection/constraining)
            else if (auto circle = std::dynamic_pointer_cast<cad_sketch::SketchCircle>(elem)) {
                addEndpointMarker(circle->GetCenter());
            }
        }
        m_view->Redraw();
    }

    // Only clear preview objects
    void QtOccView::ClearSketchPreview() {
        if (m_context.IsNull()) return;

        for (auto& obj : m_sketchPreviewObjects) {
            m_context->Remove(obj, Standard_False);
        }
        m_sketchPreviewObjects.clear();
    }
    // Remove highlighted sketch elements (usually called when deselecting or switching tools)
    void QtOccView::UnhighlightSketchElement() {
        // Clear single selection highlight
        if (!m_sketchHighlightAIS.IsNull() && !m_context.IsNull()) {
            m_context->Remove(m_sketchHighlightAIS, Standard_False);
            m_sketchHighlightAIS.Nullify();
        }
        // Clear multi-selection highlight list
        for (auto& obj : m_sketchHighlightList) {
            if (!obj.IsNull() && !m_context.IsNull()) {
                m_context->Remove(obj, Standard_False);
            }
        }
        m_sketchHighlightList.clear();
    }

    // Clear all formal sketch geometries
    void QtOccView::ClearSketchObjects() {
        if (m_context.IsNull()) return;

        UnhighlightSketchElement();
        m_currentSelectedAIS.Nullify();
        m_currentSelectedShape.reset();

        for (auto& obj : m_sketchObjects) {
            m_context->Remove(obj, Standard_False);
        }
        m_sketchObjects.clear();
        m_sketchElementMap.clear();
        m_view->Redraw();
    }

    // Show snap indicator
    void QtOccView::ShowSnapIndicator(const gp_Pnt& pnt, cad_sketch::SnapType snapType) {
        if (m_context.IsNull()) return;

        // 1. Select the corresponding OCC icon shape based on the snap type from the underlying layer
        Aspect_TypeOfMarker markerType = Aspect_TOM_RING1; // Default to circle
        Quantity_NameOfColor markerColor = Quantity_NOC_MAGENTA1; // Default to magenta

        switch (snapType) {
        case cad_sketch::SnapType::Endpoint:
            markerType = Aspect_TOM_PLUS;       // Endpoint: cross 
            markerColor = Quantity_NOC_GREEN;   // Use green for endpoint
            break;
        case cad_sketch::SnapType::Midpoint:
            markerType = Aspect_TOM_O_STAR;     // Midpoint: circle with asterisk 
            markerColor = Quantity_NOC_CYAN1;   // Use cyan for midpoint
            break;
        case cad_sketch::SnapType::Center:
            markerType = Aspect_TOM_RING1;      // Center: hollow circle 
            markerColor = Quantity_NOC_MAGENTA1;// Use magenta for center
            break;
        case cad_sketch::SnapType::Nearest:
            markerType = Aspect_TOM_X;          // Nearest point: X shape
            markerColor = Quantity_NOC_BLACK;   // Use black for nearest point
            break;
        case cad_sketch::SnapType::Grid:
            markerType = Aspect_TOM_POINT;      // Grid: solid dot
            markerColor = Quantity_NOC_ORANGE;  // Use orange for grid
            break;
        default:
            break;
        }

        // 2. Create or update icon
        if (m_snapIndicator.IsNull()) {
            Handle(Geom_CartesianPoint) geomPt = new Geom_CartesianPoint(pnt);
            Handle(AIS_Point) aisPt = new AIS_Point(geomPt);

            aisPt->SetMarker(markerType);
            aisPt->SetColor(markerColor);
            aisPt->SetZLayer(Graphic3d_ZLayerId_Topmost); // Ensure it is not blocked by the model
            m_snapIndicator = aisPt;
        }
        else {
            Handle(AIS_Point) aisPt = Handle(AIS_Point)::DownCast(m_snapIndicator);
            aisPt->SetMarker(markerType); // Dynamically update icon shape
            aisPt->SetColor(markerColor); // Dynamically update icon color

            Handle(Geom_CartesianPoint) geomPt = Handle(Geom_CartesianPoint)::DownCast(aisPt->Component());
            geomPt->SetPnt(pnt);
            m_context->Redisplay(m_snapIndicator, Standard_False);
        }

        m_context->Display(m_snapIndicator, Standard_False);
        m_view->Redraw();
    }

    // Hide snap indicator
    void QtOccView::HideSnapIndicator() {
        if (!m_context.IsNull() && !m_snapIndicator.IsNull()) {
            m_context->Remove(m_snapIndicator, Standard_False);
            m_snapIndicator.Nullify();
            m_view->Redraw();
        }
    }


    // Get currently selected sketch elements 
    std::vector<cad_sketch::SketchElementPtr> QtOccView::GetSelectedSketchElements() {
        std::vector<cad_sketch::SketchElementPtr> result;
        if (m_context.IsNull()) return result;

        // Iterate through all selected objects in the OCC context
        for (m_context->InitSelected(); m_context->MoreSelected(); m_context->NextSelected()) {
            Handle(AIS_InteractiveObject) obj = m_context->SelectedInteractive();
            // If found in our mapping table, extract it
            if (m_sketchElementMap.find(obj) != m_sketchElementMap.end()) {
                result.push_back(m_sketchElementMap[obj]);
            }
        }
        return result;
    }

    // Set sketch visibility
    void QtOccView::SetSketchVisibility(const std::shared_ptr<cad_sketch::Sketch>& sketch, bool visible) {
        if (!sketch || m_context.IsNull()) return;

        // ★ Collect all elements that need to be matched (including top-level elements + sub-points)
        std::vector<cad_sketch::SketchElementPtr> allTargets;
        for (const auto& elem : sketch->GetElements()) {
            allTargets.push_back(elem);

            // Also add sub-points of lines/arcs/circles to the match list
            if (auto line = std::dynamic_pointer_cast<cad_sketch::SketchLine>(elem)) {
                if (line->GetStartPoint()) allTargets.push_back(line->GetStartPoint());
                if (line->GetEndPoint())   allTargets.push_back(line->GetEndPoint());
            }
            else if (auto arc = std::dynamic_pointer_cast<cad_sketch::SketchArc>(elem)) {
                if (arc->GetStartPoint()) allTargets.push_back(arc->GetStartPoint());
                if (arc->GetEndPoint())   allTargets.push_back(arc->GetEndPoint());
                if (arc->GetCenter())     allTargets.push_back(arc->GetCenter());
            }
            else if (auto circle = std::dynamic_pointer_cast<cad_sketch::SketchCircle>(elem)) {
                if (circle->GetCenter())  allTargets.push_back(circle->GetCenter());
            }
        }

        // 1. Control red lines and endpoint markers belonging to this sketch (Elements + Point Markers)
        for (const auto& target : allTargets) {
            for (const auto& pair : m_sketchElementMap) {
                if (pair.second == target) {
                    if (visible) m_context->Display(pair.first, Standard_False);
                    else m_context->Erase(pair.first, Standard_False);
                }
            }
        }

        // 2. Control light blue faces belonging to this sketch (Profiles)
        for (const auto& profile : sketch->GetProfiles()) {
            TopoDS_Face face = profile->GetFace();
            for (const auto& pair : m_sketchProfileMap) {
                // Use OCC's underlying IsSame to accurately compare topological shapes
                if (pair.second && pair.second->GetOCCTShape().IsSame(face)) {
                    if (visible) m_context->Display(pair.first, Standard_False);
                    else m_context->Erase(pair.first, Standard_False);
                }
            }
        }
        m_view->Redraw();
    }

    void QtOccView::RemoveSketch(const std::shared_ptr<cad_sketch::Sketch>& sketch) {
        if (!sketch || m_context.IsNull()) return;

        // Helper lambda: match by coordinates and remove the AIS object of SketchPoint
        auto removePointByCoord = [&](double px, double py) {
            for (auto it = m_sketchElementMap.begin(); it != m_sketchElementMap.end(); ) {
                auto pt = std::dynamic_pointer_cast<cad_sketch::SketchPoint>(it->second);
                if (pt && std::abs(pt->GetX() - px) < 1e-9 && std::abs(pt->GetY() - py) < 1e-9) {
                    if (m_currentSelectedAIS == it->first) {
                        UnhighlightSketchElement();
                        m_currentSelectedAIS.Nullify();
                        m_currentSelectedShape.reset();
                    }
                    m_context->Remove(it->first, Standard_False);
                    auto vecIt = std::find(m_sketchObjects.begin(), m_sketchObjects.end(), it->first);
                    if (vecIt != m_sketchObjects.end()) m_sketchObjects.erase(vecIt);
                    it = m_sketchElementMap.erase(it);
                    return;
                }
                else {
                    ++it;
                }
            }
            };

        // Collect arc elements to be deleted (process their endpoints separately)
        std::vector<std::shared_ptr<cad_sketch::SketchArc>> arcsToClean;

        // Collect all elements to be deleted (including endpoints, but excluding Arc endpoints)
        std::vector<cad_sketch::SketchElementPtr> allElems;
        for (const auto& elem : sketch->GetElements()) {
            allElems.push_back(elem);
            if (auto line = std::dynamic_pointer_cast<cad_sketch::SketchLine>(elem)) {
                allElems.push_back(line->GetStartPoint());
                allElems.push_back(line->GetEndPoint());
            }
            else if (auto arc = std::dynamic_pointer_cast<cad_sketch::SketchArc>(elem)) {
                // Arc endpoints are not added to allElems because the pointer is different each time; use coordinate matching instead
                arcsToClean.push_back(arc);
            }
            else if (auto circle = std::dynamic_pointer_cast<cad_sketch::SketchCircle>(elem)) {
                allElems.push_back(circle->GetCenter());
            }
        }

        // 1. Delete all element AIS corresponding to this sketch (pointer matching)
        for (const auto& elem : allElems) {
            for (auto it = m_sketchElementMap.begin(); it != m_sketchElementMap.end(); ) {
                if (it->second == elem) {
                    if (m_currentSelectedAIS == it->first) {
                        UnhighlightSketchElement();
                        m_currentSelectedAIS.Nullify();
                        m_currentSelectedShape.reset();
                    }

                    m_context->Remove(it->first, Standard_False);

                    // Safely remove from rendering pool
                    auto vecIt = std::find(m_sketchObjects.begin(), m_sketchObjects.end(), it->first);
                    if (vecIt != m_sketchObjects.end()) m_sketchObjects.erase(vecIt);

                    it = m_sketchElementMap.erase(it);
                }
                else {
                    ++it;
                }
            }
        }

        // 2. Clean up Arc endpoints using coordinate matching
        for (const auto& arc : arcsToClean) {
            auto startPt = arc->GetStartPoint();
            auto endPt = arc->GetEndPoint();
            if (startPt) removePointByCoord(startPt->GetX(), startPt->GetY());
            if (endPt) removePointByCoord(endPt->GetX(), endPt->GetY());
        }

        // 3. Clean up the closed-profile AIS objects
        cad_sketch::Sketch* sketchKey = sketch.get();
        auto cacheIt = s_sketchProfileCache.find(sketchKey);
        if (cacheIt != s_sketchProfileCache.end()) {
            for (auto& aisProfile : cacheIt->second) {
                if (aisProfile.IsNull()) continue;

                m_context->Remove(aisProfile, Standard_False);

                auto vecIt = std::find(m_sketchProfileObjects.begin(),
                    m_sketchProfileObjects.end(), aisProfile);
                if (vecIt != m_sketchProfileObjects.end()) {
                    m_sketchProfileObjects.erase(vecIt);
                }

                m_sketchProfileMap.erase(aisProfile);
            }
            s_sketchProfileCache.erase(cacheIt);
        }

        m_view->Redraw();
    }

    // Erase specified sketch elements from the screen
    void QtOccView::RemoveSketchElements(const std::vector<cad_sketch::SketchElementPtr>& elements) {
        if (m_context.IsNull()) return;

        // Helper lambda: match by coordinates and remove the AIS object of SketchPoint from m_sketchElementMap
        // Used to handle the issue where SketchArc::GetStartPoint()/GetEndPoint() returns a new pointer each time
        auto removePointByCoord = [&](double px, double py) {
            for (auto it = m_sketchElementMap.begin(); it != m_sketchElementMap.end(); ++it) {
                auto pt = std::dynamic_pointer_cast<cad_sketch::SketchPoint>(it->second);
                if (pt && std::abs(pt->GetX() - px) < 1e-9 && std::abs(pt->GetY() - py) < 1e-9) {
                    if (m_currentSelectedAIS == it->first) {
                        UnhighlightSketchElement();
                        m_currentSelectedAIS.Nullify();
                        m_currentSelectedShape.reset();
                        m_context->ClearSelected(Standard_False);
                    }
                    m_context->Remove(it->first, Standard_False);
                    auto vecIt = std::find(m_sketchObjects.begin(), m_sketchObjects.end(), it->first);
                    if (vecIt != m_sketchObjects.end()) m_sketchObjects.erase(vecIt);
                    m_sketchElementMap.erase(it);
                    return;
                }
            }
            };

        // Expand deletion list: endpoints of lines/circles return stable pointers, can be compared directly
        std::vector<cad_sketch::SketchElementPtr> allToRemove;
        for (const auto& elem : elements) {
            allToRemove.push_back(elem);
            if (auto line = std::dynamic_pointer_cast<cad_sketch::SketchLine>(elem)) {
                allToRemove.push_back(line->GetStartPoint());
                allToRemove.push_back(line->GetEndPoint());
            }
            else if (auto circle = std::dynamic_pointer_cast<cad_sketch::SketchCircle>(elem)) {
                allToRemove.push_back(circle->GetCenter());
            }

        }

        for (const auto& elem : allToRemove) {
            for (auto it = m_sketchElementMap.begin(); it != m_sketchElementMap.end(); ++it) {
                if (it->second == elem) {
                    if (m_currentSelectedAIS == it->first) {
                        UnhighlightSketchElement();
                        m_currentSelectedAIS.Nullify();
                        m_currentSelectedShape.reset();
                        m_context->ClearSelected(Standard_False);
                    }
                    m_context->Remove(it->first, Standard_False);
                    auto vecIt = std::find(m_sketchObjects.begin(), m_sketchObjects.end(), it->first);
                    if (vecIt != m_sketchObjects.end()) m_sketchObjects.erase(vecIt);
                    m_sketchElementMap.erase(it);
                    break;
                }
            }
        }

        // Process Arc endpoints separately: use coordinate matching to find and remove the corresponding AIS point objects
        for (const auto& elem : elements) {
            if (auto arc = std::dynamic_pointer_cast<cad_sketch::SketchArc>(elem)) {
                auto startPt = arc->GetStartPoint();
                auto endPt = arc->GetEndPoint();
                if (startPt) removePointByCoord(startPt->GetX(), startPt->GetY());
                if (endPt) removePointByCoord(endPt->GetX(), endPt->GetY());
            }
        }

        m_context->UpdateCurrentViewer();
    }

    // Refresh after moving elements (Update visuals after dragging)
    void QtOccView::UpdateSketchElementVisuals(const cad_sketch::SketchElementPtr& elem) {
        if (m_context.IsNull() || !m_sketchMode) return;

        // Iterate through mapping table to find all primitives affected by dragging
        for (const auto& pair : m_sketchElementMap) {
            bool shouldUpdate = false;

            // 1. If it's the currently dragged body itself, it must be updated
            if (pair.second == elem) {
                shouldUpdate = true;
            }
            // 2. If dragging a "point", check if any line or curve depends on it
            else if (elem->GetType() == cad_sketch::SketchElementType::Point) {
                // If primitive is a line, check if its start or end point is the currently dragged point
                if (auto line = std::dynamic_pointer_cast<cad_sketch::SketchLine>(pair.second)) {
                    if (line->GetStartPoint() == elem || line->GetEndPoint() == elem) {
                        shouldUpdate = true;
                    }
                }
                // If primitive is a curve, iterate through its control points to see if it includes the dragged point
                else if (auto curve = std::dynamic_pointer_cast<cad_sketch::SketchCurve>(pair.second)) {
                    for (const auto& pt : curve->GetControlPoints()) {
                        if (pt == elem) {
                            shouldUpdate = true;
                            break;
                        }
                    }
                }
            }
            // 3. Conversely, if dragging a whole curve, the visual control points underneath should translate accordingly
            else if (elem->GetType() == cad_sketch::SketchElementType::Curve && pair.second->GetType() == cad_sketch::SketchElementType::Point) {
                auto curve = std::dynamic_pointer_cast<cad_sketch::SketchCurve>(elem);
                for (const auto& pt : curve->GetControlPoints()) {
                    if (pt == pair.second) {
                        shouldUpdate = true;
                        break;
                    }
                }
            }

            // 4. If dragging a line, the visual endpoints underneath should update accordingly
            else if (auto dragLine = std::dynamic_pointer_cast<cad_sketch::SketchLine>(elem)) {
                if (pair.second == dragLine->GetStartPoint() || pair.second == dragLine->GetEndPoint()) {
                    shouldUpdate = true;
                }
            }
            // 5. If dragging an arc, endpoints should update too
            else if (auto dragArc = std::dynamic_pointer_cast<cad_sketch::SketchArc>(elem)) {
                if (pair.second == dragArc->GetStartPoint() || pair.second == dragArc->GetEndPoint()) {
                    shouldUpdate = true;
                }
            }

            // 6. If dragging a circle, the center point should update too 
            else if (auto dragCircle = std::dynamic_pointer_cast<cad_sketch::SketchCircle>(elem)) {
                if (pair.second == dragCircle->GetCenter()) {
                    shouldUpdate = true;
                }
            }

            // Unify redraw execution
            if (shouldUpdate) {
                Handle(AIS_Shape) aisShape = Handle(AIS_Shape)::DownCast(pair.first);
                if (!aisShape.IsNull()) {
                    // Regenerate topological shape and refresh
                    TopoDS_Shape newShape = MakeShapeFromSketchElement(pair.second, m_sketchMode->GetSketchCS());
                    aisShape->SetShape(newShape);
                    m_context->Redisplay(aisShape, Standard_False);

                    // If this thing is currently in blue highlight selected state, the highlight layer should update as well
                    if (m_currentSelectedAIS == pair.first && !m_sketchHighlightAIS.IsNull()) {
                        m_sketchHighlightAIS->SetShape(newShape);
                        m_context->Redisplay(m_sketchHighlightAIS, Standard_False);
                    }
                }
            }
        }
        m_view->Redraw(); // Force screen refresh
    }

    std::shared_ptr<cad_sketch::Sketch> QtOccView::GetActiveSketch() const {
        return m_sketchMode ? m_sketchMode->GetCurrentSketch() : nullptr;
    }

    TopoDS_Face QtOccView::GetSketchFace() const {
        if (m_sketchMode) {
            return m_sketchMode->GetSketchFace(); // Call the interface added in SketchMode
        }
        return TopoDS_Face(); // If sketch mode is not initialized, return an empty face
    }

    TopoDS_Shape QtOccView::GetSelectedSubShape() const {
        if (!m_context.IsNull()) {
            m_context->InitSelected();
            if (m_context->MoreSelected() && m_context->HasSelectedShape()) {
                return m_context->SelectedShape(); // Return exactly selected Face / Edge, etc.
            }
        }
        return TopoDS_Shape();
    }

    gp_Ax3 QtOccView::GetSketchCS() const {
        if (m_sketchMode) {
            return m_sketchMode->GetSketchCS();
        }
        return gp_Ax3();
    }

    void QtOccView::ClearSketchElementMap() {
        m_sketchElementMap.clear();
    }

    bool QtOccView::HasActiveSketchTool() const {
        return m_sketchMode && m_sketchMode->HasActiveTool();
    }

    void QtOccView::StopSketchTool() {
        if (m_sketchMode) {
            m_sketchMode->StopCurrentTool();
        }
    }

    // Sketch history management
    void QtOccView::UndoSketch() {
        if (IsInSketchMode()) m_sketchMode->Undo();
    }
    void QtOccView::RedoSketch() {
        if (IsInSketchMode()) m_sketchMode->Redo();
    }
    bool QtOccView::CanUndoSketch() const {
        return IsInSketchMode() && m_sketchMode->CanUndo();
    }
    bool QtOccView::CanRedoSketch() const {
        return IsInSketchMode() && m_sketchMode->CanRedo();
    }

    // Render sketch closed profiles
    void QtOccView::RenderSketchProfiles(const std::vector<cad_sketch::SketchProfilePtr>& profiles) {
        if (m_context.IsNull()) return;

        // 1. Get the currently active editing sketch
        auto activeSketch = GetActiveSketch();
        cad_sketch::Sketch* sketchKey = activeSketch ? activeSketch.get() : nullptr;

        // 2. Accurately clear old profile faces of current active sketch, do not touch other sketches
        if (sketchKey) {
            auto it = s_sketchProfileCache.find(sketchKey);
            if (it != s_sketchProfileCache.end()) {
                for (auto& ais : it->second) {
                    m_context->Remove(ais, Standard_False);
                    // Remove from global render pool
                    auto vecIt = std::find(m_sketchProfileObjects.begin(), m_sketchProfileObjects.end(), ais);
                    if (vecIt != m_sketchProfileObjects.end()) m_sketchProfileObjects.erase(vecIt);
                    m_sketchProfileMap.erase(ais);
                }
                s_sketchProfileCache.erase(it);
            }
        }
        else {
            // If no active sketch (e.g., when file just loaded), fallback to global cleanup
            ClearSketchProfiles();
        }

        if (m_isDrawingSweepPath) {
            m_view->Redraw();
            return;
        }

        // 3. Generate and display new profile faces
        for (const auto& profile : profiles) {
            TopoDS_Face face = profile->GetFace();
            if (face.IsNull()) continue;

            Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
            if (surface.IsNull()) continue;

            Standard_Real uMin, uMax, vMin, vMax;
            BRepTools::UVBounds(face, uMin, uMax, vMin, vMax);
            Standard_Real uMid = (uMin + uMax) * 0.5;
            Standard_Real vMid = (vMin + vMax) * 0.5;

            GeomLProp_SLProps props(surface, uMid, vMid, 1, Precision::Confusion());
            if (!props.IsNormalDefined()) continue;

            gp_Dir normal = props.Normal();
            if (face.Orientation() == TopAbs_REVERSED) {
                normal.Reverse();
            }

            const Standard_Real previewOffset = 0.01;
            gp_Trsf trsf;
            trsf.SetTranslation(gp_Vec(normal) * previewOffset);

            TopoDS_Shape liftedShape = BRepBuilderAPI_Transform(face, trsf, true).Shape();
            TopoDS_Face liftedFace = TopoDS::Face(liftedShape);

            Handle(AIS_Shape) aisFace = new AIS_Shape(liftedFace);
            Handle(Prs3d_Drawer) drawer = aisFace->Attributes();
            drawer->SetFaceBoundaryDraw(Standard_False);
            aisFace->SetColor(Quantity_NOC_LIGHTSKYBLUE1);
            aisFace->SetTransparency(0.6);
            aisFace->SetDisplayMode(AIS_Shaded);
            aisFace->SetPolygonOffsets(Aspect_POM_Fill, 1.0f, -4.0f);
            m_context->Display(aisFace, Standard_False);
            m_sketchProfileObjects.push_back(aisFace);

            cad_core::ShapePtr profileShape = std::make_shared<cad_core::Shape>(face);
            m_sketchProfileMap[aisFace] = profileShape;

            // 4. Register new faces to current sketch's cache for accurate cleanup next time
            if (sketchKey) {
                s_sketchProfileCache[sketchKey].push_back(aisFace);
            }
        }

        m_view->Redraw();
    }

    void QtOccView::ClearSketchProfiles() {
        if (m_context.IsNull()) return;

        for (auto& obj : m_sketchProfileObjects) {
            m_context->Remove(obj, Standard_False);
        }
        m_sketchProfileObjects.clear();
        m_sketchProfileMap.clear(); // Clear mapping table to prevent memory leaks or dangling pointers
        m_view->Redraw();
    }

    void QtOccView::HideSingleSketchProfile(const cad_core::ShapePtr& profileShape) {
        if (!profileShape || m_context.IsNull()) return;

        // Iterate through sketch profile mapping table to find matching business object
        for (auto it = m_sketchProfileMap.begin(); it != m_sketchProfileMap.end(); ++it) {
            if (it->second == profileShape) {
                Handle(AIS_InteractiveObject) aisObj = it->first;

                // 1. Hide this profile from 3D view (Erase)
                m_context->Erase(aisObj, Standard_False);

                // 2. Remove it from the object rendering pool
                auto vecIt = std::find(m_sketchProfileObjects.begin(), m_sketchProfileObjects.end(), aisObj);
                if (vecIt != m_sketchProfileObjects.end()) {
                    m_sketchProfileObjects.erase(vecIt);
                }

                // 3. Remove from mapping table to prevent dangling pointers
                m_sketchProfileMap.erase(it);
                break; // Exit loop if found
            }
        }
        m_view->Redraw();
    }


    void QtOccView::DrawCentroid(const gp_Pnt& pnt) {
        if (m_context.IsNull()) return;

        ClearCentroid();

        // View layer is only responsible for "drawing", not mathematical logic
        TopoDS_Shape sphereShape = BRepPrimAPI_MakeSphere(pnt, 0.1).Shape();
        Handle(AIS_Shape) aisSphere = new AIS_Shape(sphereShape);

        aisSphere->SetColor(Quantity_NOC_RED);
        aisSphere->SetZLayer(Graphic3d_ZLayerId_Topmost);

        m_CentroidActor = aisSphere;
        m_context->Display(m_CentroidActor, Standard_False);
        m_view->Redraw();
    }

    void QtOccView::ClearCentroid() {
        if (!m_context.IsNull() && !m_CentroidActor.IsNull()) {
            m_context->Remove(m_CentroidActor, Standard_False);
            m_CentroidActor.Nullify();
            m_view->Redraw();
        }
    }

    // Sweep execution logic
    cad_core::ShapePtr QtOccView::GetSweepPathShape() {
        if (!m_sketchMode) return nullptr;
        auto pathSketch = m_sketchMode->GetCurrentSketch();
        if (!pathSketch) return nullptr;

        BRepBuilderAPI_MakeWire wireMaker;
        for (const auto& elem : pathSketch->GetElements()) {
            if (elem) {
                for (auto it = m_sketchElementMap.begin(); it != m_sketchElementMap.end(); ++it) {
                    if (it->second == elem) {
                        Handle(AIS_Shape) aisShape = Handle(AIS_Shape)::DownCast(it->first);
                        if (!aisShape.IsNull() && aisShape->Shape().ShapeType() == TopAbs_EDGE) {
                            wireMaker.Add(TopoDS::Edge(aisShape->Shape()));
                        }
                        break;
                    }
                }
            }
        }
        if (!wireMaker.IsDone()) return nullptr;
        return std::make_shared<cad_core::Shape>(wireMaker.Wire());
    }

    // Clean up residual sketches and states from Sweep drawing
    void QtOccView::CleanupSweepUI() {
        if (!m_sketchMode) return;
        auto pathSketch = m_sketchMode->GetCurrentSketch();
        if (pathSketch) {
            for (const auto& elem : pathSketch->GetElements()) {
                if (elem) {
                    for (auto it = m_sketchElementMap.begin(); it != m_sketchElementMap.end(); ) {
                        if (it->second == elem) {
                            Handle(AIS_InteractiveObject) aisObj = Handle(AIS_InteractiveObject)::DownCast(it->first);
                            if (!aisObj.IsNull()) m_context->Remove(aisObj, Standard_False);
                            it = m_sketchElementMap.erase(it);
                        }
                        else {
                            ++it;
                        }
                    }
                }
            }
        }
        m_sketchMode->ExitSketchMode();
        m_sweepInteractionState = SweepInteractionMode::None;
        m_isDrawingSweepPath = false;   // Reset flag, otherwise subsequent profiles will never render
        ClearCentroid();
        m_view->Redraw();
    }

    // Sweep interaction state control 
    void QtOccView::StartSweepInteraction() {
        // Unlock: enter state waiting for profile selection
        m_sweepInteractionState = SweepInteractionMode::SelectingProfile;
        ClearSelection();
    }

    void QtOccView::CancelSweepInteraction() {
        // Re-lock and reset state
        m_sweepInteractionState = SweepInteractionMode::None;
        m_isDrawingSweepPath = false;

        // Destroy possible translucent preview plane
        if (!m_sweepPlanePreview.IsNull()) {
            m_context->Remove(m_sweepPlanePreview, Standard_False);
            m_sweepPlanePreview.Nullify();
        }

        // If already entered sketch, forcefully exit and clean up residual paths
        if (IsInSketchMode()) {
            // Call cleanup function
            CleanupSweepUI();
        }

        ClearSelection(); // Clear possibly residual selection highlights
        m_view->Redraw();
        qDebug() << "Sweep interaction cancelled and cleaned up.";
    }

    // Toggle Sweep path drawing tool
    void QtOccView::ToggleSweepPathTool(bool enableDrawing) {
        if (!m_sketchMode || !IsInSketchMode()) {
            qDebug() << "Cannot toggle tool: Not in sketch mode yet.";
            return;
        }

        m_isDrawingSweepPath = enableDrawing;

        if (enableDrawing) {
            // Activate curve tool and pin to centroid start point
            m_sketchMode->StartCurveTool();
            auto curveTool = dynamic_cast<cad_ui::SketchCurveTool*>(m_sketchMode->GetCurrentTool());
            if (curveTool) {
                curveTool->InjectStartPoint(0.0, 0.0);
            }
        }
        else {
            // Stop current tool, revert to default "selection mode" (can now select and delete line segments)
            m_sketchMode->StopCurrentTool();
        }
    }

} // namespace cad_ui
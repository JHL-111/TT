#include "cad_ui/QtOccView.h"
#include "cad_ui/SketchMode.h"
#include "cad_core/FilletChamferOperations.h"

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


#ifdef _WIN32
#include <WNT_Window.hxx>
#elif defined(__APPLE__)
#include <Cocoa_Window.hxx>
#else
#include <Xw_Window.hxx>
#endif

namespace cad_ui {


QtOccView::QtOccView(QWidget* parent) 
    : QWidget(parent), m_isInitialized(false), m_currentMouseButton(Qt::NoButton),
      m_currentSelectedShape(nullptr), m_currentSelectionMode(0) {
    
    // Set widget attributes to reduce flicker
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_OpaquePaintEvent);  // Prevent Qt from erasing background
    setAttribute(Qt::WA_StaticContents);    // Widget contents don't scroll
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);  // Don't fill background to reduce flicker
    
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
        
        // 设置选中和高亮样式
        // 使用更直接的方法设置高亮颜色
        Handle(Prs3d_Drawer) hilightDrawer = m_context->HighlightStyle(Prs3d_TypeOfHighlight_Selected);
        if (!hilightDrawer.IsNull()) {
            hilightDrawer->SetColor(Quantity_NOC_RED);
            hilightDrawer->SetDisplayMode(1); // Shaded mode
        }
        
        Handle(Prs3d_Drawer) preHilightDrawer = m_context->HighlightStyle(Prs3d_TypeOfHighlight_Dynamic);
        if (!preHilightDrawer.IsNull()) {
            preHilightDrawer->SetColor(Quantity_NOC_ORANGE);
            preHilightDrawer->SetDisplayMode(1); // Shaded mode
        }
        
        // Set up selection manager
        m_selectionManager->SetContext(m_context);
        m_selectionManager->SetView(m_view);
        
        m_isInitialized = true;
        
        // Initial view setup and render
        FitAll();
        ShowAxes(false);  // 默认显示坐标轴
        m_view->Redraw();  // 确保初始渲染
        
        return true;
    } catch (const Standard_Failure& e) {
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
    } else if (mode == "shaded") {
        m_context->SetDisplayMode(AIS_Shaded, Standard_True);
    }
    m_view->Redraw();
}

void QtOccView::SetProjectionMode(bool orthographic) {
    if (m_view.IsNull()) return;
    
    if (orthographic) {
        m_view->Camera()->SetProjectionType(Graphic3d_Camera::Projection_Orthographic);
    } else {
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
    aisShape->SetColor(Quantity_NOC_ORANGE);
    aisShape->SetTransparency(0.0);
    
    m_context->Display(aisShape, Standard_False);
    

    // Store mapping for selection synchronization
    m_shapeToAIS[shape] = aisShape;
    
    // Enable selection modes for this shape
    m_context->SetSelectionModeActive(aisShape, 0, Standard_True); // Shape
    m_context->SetSelectionModeActive(aisShape, 1, Standard_True); // Vertex
    m_context->SetSelectionModeActive(aisShape, 2, Standard_True); // Edge
    m_context->SetSelectionModeActive(aisShape, 4, Standard_True); // Face
    
    // Fit all objects in view to ensure visibility and render
    m_view->FitAll();
    m_view->Redraw();
    
    // Force immediate rendering
    update();
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
    
    // 切换选择模式时清除所有高亮
    UnhighlightAllVertices();
    UnhighlightAllEdges();
    UnhighlightAllFaces();
    
    // 清除当前选择
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
    
    m_context->ClearSelected(Standard_True);
    m_view->Redraw();
}

void QtOccView::ShowGrid(bool show) {
    if (m_viewer.IsNull()) return;
    
    if (show) {
        m_viewer->ActivateGrid(Aspect_GT_Rectangular, Aspect_GDM_Lines);
    } else {
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
    } else {
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
            } else {
                m_context->UnsetTransparency(aShape, Standard_False);
            }
        }
    }
    
    // Update the view
    m_context->UpdateCurrentViewer();
    m_view->Redraw();
}

// 获取当前选中的形状
cad_core::ShapePtr QtOccView::GetCurrentSelectedShape() const {
    return m_currentSelectedShape;
}

// 设置特定形状的透明度
void QtOccView::SetShapeTransparency(const cad_core::ShapePtr& shape, double transparency) {
    if (!shape || m_context.IsNull()) return;

    // 限制透明度在 0.0 到 1.0 之间 (Clamp transparency value)
    transparency = std::max(0.0, std::min(1.0, transparency));

    // 在映射表中查找对应的 AIS_Shape
    auto it = m_shapeToAIS.find(shape);
    if (it != m_shapeToAIS.end()) {
        Handle(AIS_Shape) aisShape = it->second;
        if (!aisShape.IsNull()) {
            // 应用透明度
            if (transparency > 0.0) {
                m_context->SetTransparency(aisShape, transparency, Standard_False);
            }
            else {
                m_context->UnsetTransparency(aisShape, Standard_False);
            }
            // 更新视图
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

void QtOccView::mousePressEvent(QMouseEvent* event) {
    m_lastMousePos = event->pos();
    m_currentMouseButton = event->button();
    
    // 优先处理草图模式
    if (IsInSketchMode()) {
        if (HasActiveSketchTool()) {
            m_sketchMode->HandleMousePress(event);
            return; 
        }

        // 如果当前没有画图工具，只允许左键进行选择 (不允许 3D 旋转视角)
        if (event->button() == Qt::LeftButton) {
            HandleSelection(event->pos());
        }
        return;
    }
    
    if (event->button() == Qt::LeftButton) {
        // Start rotation
        if (!m_view.IsNull()) {
            m_view->StartRotation(event->pos().x(), event->pos().y());
        }
        HandleSelection(event->pos());
    }
}

void QtOccView::mouseMoveEvent(QMouseEvent* event) {
    if (m_view.IsNull()) return;
    
    // 发射鼠标位置信号（屏幕坐标）
    QPoint currentPos = event->pos();
    emit MousePositionChanged(currentPos.x(), currentPos.y());
    
    // 尝试获取3D世界坐标
    try {
        // 将屏幕坐标转换为3D世界坐标
        Standard_Real X, Y, Z;
        m_view->Convert(currentPos.x(), currentPos.y(), X, Y, Z);
        emit Mouse3DPositionChanged(X, Y, Z);
    } catch (...) {
        // 如果3D转换失败，使用屏幕坐标
        emit Mouse3DPositionChanged(currentPos.x(), currentPos.y(), 0.0);
    }
    
    // 优先处理草图模式
    if (IsInSketchMode()) {
        m_sketchMode->HandleMouseMove(event); // 保持悬停吸附

        if (!HasActiveSketchTool() && !m_context.IsNull()) {
            // 将鼠标的 2D 坐标传递给底层的交互上下文，让它知道鼠标碰到了什么
            m_context->MoveTo(currentPos.x(), currentPos.y(), m_view, Standard_True);
        }

        // 允许中键平移和右键缩放，但屏蔽草图模式下的左键旋转
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
        return; // 返回，不执行下面的 3D 旋转逻辑
    }
    
    if (m_currentMouseButton == Qt::LeftButton) {
        // Rotate - use absolute position for rotation
        m_view->Rotation(currentPos.x(), currentPos.y());
        m_view->Redraw();  // 确保实时渲染
    } else if (m_currentMouseButton == Qt::MiddleButton) {
        // Pan - use delta for panning
        QPoint delta = currentPos - m_lastMousePos;
        m_view->Pan(delta.x(), -delta.y());
        m_view->Redraw();  // 确保实时渲染
    } else if (m_currentMouseButton == Qt::RightButton) {
        // Zoom - use delta for zooming
        QPoint delta = currentPos - m_lastMousePos;
        if (delta.y() != 0) {
            double factor = (delta.y() > 0) ? 0.9 : 1.1;
            m_view->SetZoom(factor);
            m_view->Redraw();  // 确保实时渲染
        }
    }
    
    m_lastMousePos = currentPos;
}

void QtOccView::mouseReleaseEvent(QMouseEvent* event) {
    // 优先处理草图模式
    if (IsInSketchMode()) {
        m_sketchMode->HandleMouseRelease(event);
        m_currentMouseButton = Qt::NoButton;
        return;
    }
    
    Q_UNUSED(event);
    m_currentMouseButton = Qt::NoButton;
}

void QtOccView::wheelEvent(QWheelEvent* event) {
    if (m_view.IsNull()) return;
    
    const int delta = event->angleDelta().y();
    const double factor = (delta > 0) ? 1.1 : 0.9;
    
    m_view->SetZoom(factor);
    m_view->Redraw();
}

void QtOccView::keyPressEvent(QKeyEvent* event) {
    // 优先处理草图模式
    if (IsInSketchMode()) {
        m_sketchMode->HandleKeyPress(event);
        return;
    }
    
    switch (event->key()) {
        case Qt::Key_F:
            FitAll();
            break;
        case Qt::Key_W:
            SetViewMode("wireframe");
            break;
        case Qt::Key_S:
            SetViewMode("shaded");
            break;
        default:
            QWidget::keyPressEvent(event);
            break;
    }
}

void QtOccView::InitializeOCC() {
    // This is called in constructor, actual initialization happens in InitViewer
}

void QtOccView::RedrawView() {
    if (!m_view.IsNull()) {
        m_view->Redraw();
    }
}

void QtOccView::HandleSelection(const QPoint& point) {
    if (m_context.IsNull()) return;

    qDebug() << "HandleSelection called, current selection mode:" << m_currentSelectionMode;

    // =========================================================================
    // 【步骤 1：清理阶段】在新选择开始时，统一清除之前的高亮和选中状态
    // =========================================================================
    if (m_currentSelectionMode != 2) { // 边模式支持多选，所以不清边
        UnhighlightAllVertices();
        UnhighlightAllFaces();

        // 清除草图的临时高亮层（克隆体）
        UnhighlightSketchElement();

        // 清除之前记录的普通实体单选状态
        if (!m_currentSelectedAIS.IsNull()) {
            m_currentSelectedAIS.Nullify();
            m_currentSelectedShape.reset();
        }

        // 彻底清空 OCC 底层的选中池
        m_context->ClearSelected(Standard_False);
    }

    // =========================================================================
    // 【步骤 2：鼠标位置检测】
    // =========================================================================
    m_context->MoveTo(point.x(), point.y(), m_view, Standard_True);

    if (m_context->HasDetected()) {
        if (m_currentSelectionMode == 2) { // Edge mode (边模式)
            qDebug() << "Edge selection mode detected, attempting to select edge...";
            m_context->Select(Standard_True);

            int selectedCount = 0;
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

                    Handle(StdSelect_BRepOwner) anOwner =
                        Handle(StdSelect_BRepOwner)::DownCast(m_context->SelectedOwner());

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
                else {
                    qDebug() << "Selected object is not an AIS_Shape";
                }
            }

            if (selectedCount == 0) {
                qDebug() << "No objects selected in context";
            }
        }
        else if (m_currentSelectionMode == 1) { // Vertex mode (顶点模式)
            qDebug() << "Vertex selection mode detected, attempting to select vertex...";
            m_context->Select(Standard_True);

            for (m_context->InitSelected(); m_context->MoreSelected(); m_context->NextSelected()) {
                Handle(AIS_InteractiveObject) anIO = m_context->SelectedInteractive();
                Handle(AIS_Shape) aisShape = Handle(AIS_Shape)::DownCast(anIO);

                if (!aisShape.IsNull()) {
                    Handle(StdSelect_BRepOwner) anOwner =
                        Handle(StdSelect_BRepOwner)::DownCast(m_context->SelectedOwner());

                    if (!anOwner.IsNull()) {
                        TopoDS_Shape selectedShape = anOwner->Shape();
                        if (selectedShape.ShapeType() == TopAbs_VERTEX) {
                            TopoDS_Vertex vertex = TopoDS::Vertex(selectedShape);
                            HighlightVertex(vertex);
                            qDebug() << "Vertex selected";
                            break;
                        }
                    }
                }
            }
        }
        else if (m_currentSelectionMode == 4) { // Face mode (面模式)
            qDebug() << "Face selection mode detected, attempting to select face...";
            m_context->Select(Standard_True);

            for (m_context->InitSelected(); m_context->MoreSelected(); m_context->NextSelected()) {
                Handle(AIS_InteractiveObject) anIO = m_context->SelectedInteractive();
                Handle(AIS_Shape) aisShape = Handle(AIS_Shape)::DownCast(anIO);

                if (!aisShape.IsNull()) {
                    Handle(StdSelect_BRepOwner) anOwner =
                        Handle(StdSelect_BRepOwner)::DownCast(m_context->SelectedOwner());

                    if (!anOwner.IsNull()) {
                        TopoDS_Shape selectedShape = anOwner->Shape();
                        if (selectedShape.ShapeType() == TopAbs_FACE) {
                            TopoDS_Face face = TopoDS::Face(selectedShape);
                            HighlightFace(face);
                            qDebug() << "Face selected, emitting FaceSelected signal";
                            emit FaceSelected(face);
                            break;
                        }
                    }
                }
            }
        }
        else {
            // =========================================================================
            // 【步骤 3：单选模式 (Shape / Sketch mode)】
            // =========================================================================

            // 让 OCC 底层真实选中，保证 Delete 能抓到它的数据
            m_context->Select(Standard_True);

            Handle(AIS_InteractiveObject) selectedObj;
            for (m_context->InitSelected(); m_context->MoreSelected(); m_context->NextSelected()) {
                selectedObj = m_context->SelectedInteractive();
                break; // 单选，拿第一个就行
            }

            if (!selectedObj.IsNull()) {
                Handle(AIS_Shape) aisShape = Handle(AIS_Shape)::DownCast(selectedObj);

                // --- 路线 A：点中的是草图元素 ---
                if (m_sketchElementMap.find(selectedObj) != m_sketchElementMap.end()) {
                    // 【高亮层核心逻辑】：克隆一个临时形状盖在上面，原物体属性分毫不动
                    m_sketchHighlightAIS = new AIS_Shape(aisShape->Shape());
                    m_sketchHighlightAIS->SetColor(Quantity_NOC_BLUE1);  // 设置为蓝色高亮
                    m_sketchHighlightAIS->SetWidth(4.0);                 // 加粗以完美覆盖原本的红线
                    m_sketchHighlightAIS->SetZLayer(Graphic3d_ZLayerId_Topmost);

                    // 仅仅在屏幕上显示这个克隆体，不要把它加入选中池
                    m_context->Display(m_sketchHighlightAIS, Standard_False);

                    // 记录真实的本体
                    m_currentSelectedAIS = aisShape;
                    qDebug() << "Sketch element selected natively and highlighted with Overlay.";
                }
                // --- 路线 B：点中的是普通 3D 实体 ---
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
    }
    else {
        // =========================================================================
        // 【步骤 4：点击了空白区域】
        // =========================================================================
        qDebug() << "No object detected, clearing all selections";
        if (m_currentSelectionMode != 2) {
            UnhighlightAllEdges(); // 非边模式下清空边高亮
        }
        // 注：由于我们在函数最开头的步骤 1 已经执行了清理高亮层的操作，这里什么都不用加了
    }

    // =========================================================================
    // 【步骤 5：刷新屏幕】
    // =========================================================================
    m_view->Redraw();
    emit ViewChanged();
}


void QtOccView::OnRedrawTimer() {
    RedrawView();
}

// 选择模式设置
void QtOccView::SetSelectionMode(cad_core::SelectionMode mode) {
    if (m_selectionManager) {
        m_selectionManager->SetSelectionMode(mode);
    }
}

// 获取选择结果
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

void QtOccView::HighlightVertex(const TopoDS_Vertex& vertex) {
    if (m_context.IsNull()) return;
    
    // 使用临时AIS对象显示高亮的点
    Handle(AIS_Shape) aisVertex = new AIS_Shape(vertex);
    
    // 设置点的高亮属性 - 红色球形
    aisVertex->SetColor(Quantity_NOC_RED);
    aisVertex->SetWidth(5.0);
    
    // 显示高亮的点
    m_context->Display(aisVertex, Standard_False);
    
    // 添加到选中点列表
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

void QtOccView::HighlightFace(const TopoDS_Face& face) {
    if (m_context.IsNull()) return;
    
    // 使用临时AIS对象显示高亮的面
    Handle(AIS_Shape) aisFace = new AIS_Shape(face);
    
    // 设置面的高亮属性 - 半透明红色
    aisFace->SetColor(Quantity_NOC_RED);
    aisFace->SetTransparency(0.3); // 半透明
    aisFace->SetDisplayMode(AIS_Shaded);
    
    // 显示高亮的面
    m_context->Display(aisFace, Standard_False);
    
    // 添加到选中面列表
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
    ClearOpFace(); // 先清理旧的

    // 确保当前有选中的边以及对应的父实体
    if (m_selectedEdges.empty() || m_edgeParentShapes.empty()) return;

    TopoDS_Edge currentEdge = m_selectedEdges[0];
    cad_core::ShapePtr parentShape = m_edgeParentShapes[0]; // 直接使用现有的父形状记录

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
        // 复用 HighlightFace 渲染风格
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
     * @brief 将草图局部 2D 点转换为世界 3D 点
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

    // 根据元素类型生成对应的 OCC 拓扑形状
    static TopoDS_Shape MakeShapeFromSketchElement(const cad_sketch::SketchElementPtr& elem, const gp_Ax3& cs) {
        if (!elem) return TopoDS_Shape();
        
        if (auto point = std::dynamic_pointer_cast<cad_sketch::SketchPoint>(elem)) {
            gp_Pnt worldPt = Sketch2DToWorld(point, cs);
            return BRepBuilderAPI_MakeVertex(worldPt); // 生成 OCC 的顶点
        }
        // 1. 如果是直线
        else if (auto line = std::dynamic_pointer_cast<cad_sketch::SketchLine>(elem)) {
            gp_Pnt p1 = Sketch2DToWorld(line->GetStartPoint(), cs);
            gp_Pnt p2 = Sketch2DToWorld(line->GetEndPoint(), cs);
            if (p1.Distance(p2) > Precision::Confusion()) {
                return BRepBuilderAPI_MakeEdge(p1, p2);
            }
        }
        // 2. 如果是圆
        else if (auto circle = std::dynamic_pointer_cast<cad_sketch::SketchCircle>(elem)) {
            gp_Pnt center = Sketch2DToWorld(circle->GetCenter(), cs);
            double radius = circle->GetRadius();
            if (radius > Precision::Confusion()) {
                // 用当前草图的法线方向和 X 轴方向构造一个平面坐标系
                gp_Ax2 ax2(center, cs.Direction(), cs.XDirection());
                gp_Circ gpCirc(ax2, radius);
                return BRepBuilderAPI_MakeEdge(gpCirc); // OCC 生成圆形边
            }
        }
        // 3. 如果是圆弧
        else if (auto arc = std::dynamic_pointer_cast<cad_sketch::SketchArc>(elem)) {
            gp_Pnt center = Sketch2DToWorld(arc->GetCenter(), cs);
            double radius = arc->GetRadius();
            if (radius > Precision::Confusion()) {
                // 用当前草图的法线方向和 X 轴方向构造一个平面坐标系 
                gp_Ax2 ax2(center, cs.Direction(), cs.XDirection());
                gp_Circ gpCirc(ax2, radius); // 构造几何基础圆

                // 获取圆弧的起止弧度 
                double startAngle = arc->GetStartAngle();
                double endAngle = arc->GetEndAngle();

                // OCC 创建圆弧边：沿着基础圆，从起始参数逆时针绘制到终止参数
                return BRepBuilderAPI_MakeEdge(gpCirc, startAngle, endAngle);
            }
        }
        return TopoDS_Shape();
    }
   
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
            emit SketchModeEntered();
        } else {
            qDebug() << "Failed to enter sketch mode";
        }
    }
    catch (const std::exception& e) {
        qDebug() << "Exception in EnterSketchMode:" << e.what();
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

// 启动草图工具
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


// 高亮选中的草图面
void QtOccView::HighlightSketchFace(const TopoDS_Face& face) {
    if (m_context.IsNull() || face.IsNull()) return;

    // 1. 清理可能存在的旧高亮
    ClearSketchFaceHighlight();

    // 2. 将选中的拓扑面包装成可显示的 AIS_Shape
    Handle(AIS_Shape) aisFace = new AIS_Shape(face);

    // 3. 设置视觉效果 (浅蓝色 + 半透明)
    aisFace->SetColor(Quantity_NOC_LIGHTSKYBLUE1); // 浅天蓝色
    aisFace->SetTransparency(0.6);                

    // 4. 强化面的边界线 (Boundary Draw)，让边缘更清晰
    Handle(Prs3d_Drawer) drawer = aisFace->Attributes();
    drawer->SetFaceBoundaryDraw(Standard_True);
    drawer->FaceBoundaryAspect()->SetColor(Quantity_NOC_BLUE1); // 边界线用深蓝色
    drawer->FaceBoundaryAspect()->SetWidth(2.0);                // 稍微加粗

    // 5. 为了防止与原模型发生严重的深度冲突，将其提至顶层
    aisFace->SetZLayer(Graphic3d_ZLayerId_Topmost);

    // 6. 显示并保存引用
    m_context->Display(aisFace, Standard_False);
    m_context->Deactivate(aisFace);
    m_highlightedFace = aisFace;

    m_view->Redraw();
}

void QtOccView::ClearSketchFaceHighlight() {
    if (m_context.IsNull() || m_highlightedFace.IsNull()) return;

    m_context->Remove(m_highlightedFace, Standard_False);
    m_highlightedFace.Nullify(); // 清空句柄
    m_view->Redraw();
}

// 显示预览线（通常用于鼠标移动时的青色反馈）
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

// 添加正式草图线（点击完成后确认留在屏幕上的线条）
void QtOccView::AddSketchElements(const std::vector<cad_sketch::SketchElementPtr>& elements, const gp_Ax3& sketchCS) {
    if (m_context.IsNull()) return;

    for (const auto& elem : elements) {
        TopoDS_Shape shape = MakeShapeFromSketchElement(elem, sketchCS);
        if (shape.IsNull()) continue;

        Handle(AIS_Shape) aisLine = new AIS_Shape(shape);
        aisLine->SetColor(Quantity_NOC_RED);
        aisLine->SetWidth(2.0);
        aisLine->SetZLayer(Graphic3d_ZLayerId_Topmost);
        m_context->Display(aisLine, Standard_False); 
        m_sketchElementMap[aisLine] = elem;
        m_sketchObjects.push_back(aisLine);
    }
    m_view->Redraw();
}

// 仅清理预览对象
void QtOccView::ClearSketchPreview() {
    if (m_context.IsNull()) return;

    for (auto& obj : m_sketchPreviewObjects) {
        m_context->Remove(obj, Standard_False);
    }
    m_sketchPreviewObjects.clear();
}
// 移除高亮的草图元素（通常在取消选择或切换工具时调用）
void QtOccView::UnhighlightSketchElement() {
    // 如果高亮存在，就把它从屏幕上彻底移除并销毁
    if (!m_sketchHighlightAIS.IsNull() && !m_context.IsNull()) {
        m_context->Remove(m_sketchHighlightAIS, Standard_False);
        m_sketchHighlightAIS.Nullify();
    }
}

// 清理所有正式草图几何体
void QtOccView::ClearSketchObjects() {
    if (m_context.IsNull()) return;

    for (auto& obj : m_sketchObjects) {
        m_context->Remove(obj, Standard_False);
    }
    m_sketchObjects.clear();
    m_sketchElementMap.clear();
    m_view->Redraw();
}

// 显示吸附辅助图形
void QtOccView::ShowSnapIndicator(const gp_Pnt& pnt, cad_sketch::SnapType snapType) {
    if (m_context.IsNull()) return;

    // 1. 根据底层传来的捕捉类型，选择 OCC 中对应的图标形状
    Aspect_TypeOfMarker markerType = Aspect_TOM_RING1; // 默认用圆圈
    Quantity_NameOfColor markerColor = Quantity_NOC_MAGENTA1; // 默认紫红色

    switch (snapType) {
    case cad_sketch::SnapType::Endpoint:
        markerType = Aspect_TOM_PLUS;       // 端点：十字 
        markerColor = Quantity_NOC_GREEN;   // 端点用绿色
        break;
    case cad_sketch::SnapType::Midpoint:
        markerType = Aspect_TOM_O_STAR;     // 中点：圆圈里带星号 
        markerColor = Quantity_NOC_CYAN1;   // 中点用青色
        break;
    case cad_sketch::SnapType::Center:
        markerType = Aspect_TOM_RING1;      // 圆心：空心圆 
        markerColor = Quantity_NOC_MAGENTA1;// 圆心用紫红色
        break;
    case cad_sketch::SnapType::Nearest:
        markerType = Aspect_TOM_X;          // 最近点：X型
        markerColor = Quantity_NOC_BLACK;  //  最近点用黄色
        break;
    case cad_sketch::SnapType::Grid:
        markerType = Aspect_TOM_POINT;      // 网格：实心小点
        markerColor = Quantity_NOC_ORANGE;  // 网格用橙色
        break;
    default:
        break;
    }

    // 2. 创建或更新图标
    if (m_snapIndicator.IsNull()) {
        Handle(Geom_CartesianPoint) geomPt = new Geom_CartesianPoint(pnt);
        Handle(AIS_Point) aisPt = new AIS_Point(geomPt);

        aisPt->SetMarker(markerType);
        aisPt->SetColor(markerColor);
        aisPt->SetZLayer(Graphic3d_ZLayerId_Topmost); // 确保不被模型遮挡
        m_snapIndicator = aisPt;
    }
    else {
        Handle(AIS_Point) aisPt = Handle(AIS_Point)::DownCast(m_snapIndicator);
        aisPt->SetMarker(markerType); // 动态更新图标形状
        aisPt->SetColor(markerColor); // 动态更新图标颜色

        Handle(Geom_CartesianPoint) geomPt = Handle(Geom_CartesianPoint)::DownCast(aisPt->Component());
        geomPt->SetPnt(pnt);
        m_context->Redisplay(m_snapIndicator, Standard_False);
    }

    m_context->Display(m_snapIndicator, Standard_False);
    m_view->Redraw();
}

// 隐藏小圆圈
void QtOccView::HideSnapIndicator() {
    if (!m_context.IsNull() && !m_snapIndicator.IsNull()) {
        m_context->Remove(m_snapIndicator, Standard_False);
        m_snapIndicator.Nullify();
        m_view->Redraw();
    }
}


// 获取当前被选中的草图元素 
std::vector<cad_sketch::SketchElementPtr> QtOccView::GetSelectedSketchElements() {
    std::vector<cad_sketch::SketchElementPtr> result;
    if (m_context.IsNull()) return result;

    // 遍历 OCC 上下文中所有被选中的对象
    for (m_context->InitSelected(); m_context->MoreSelected(); m_context->NextSelected()) {
        Handle(AIS_InteractiveObject) obj = m_context->SelectedInteractive();
        // 如果在我们的映射表里能找到，就提取出来
        if (m_sketchElementMap.find(obj) != m_sketchElementMap.end()) {
            result.push_back(m_sketchElementMap[obj]);
        }
    }
    return result;
}

// 从屏幕上抹除指定的草图元素
void QtOccView::RemoveSketchElements(const std::vector<cad_sketch::SketchElementPtr>& elements) {
    if (m_context.IsNull()) return;

    // 反向查找：通过元素指针找到对应的 AIS 对象并移除
    for (const auto& elem : elements) {
        for (auto it = m_sketchElementMap.begin(); it != m_sketchElementMap.end(); ++it) {
            if (it->second == elem) {
                if (m_currentSelectedAIS == it->first) {
                    UnhighlightSketchElement();       // 移除蓝色的高亮克隆体
                    m_currentSelectedAIS.Nullify();   // 清空选中记录
                    m_currentSelectedShape.reset();
                    m_context->ClearSelected(Standard_False); // 顺便清空 OCC 底层选中池
                }
                m_context->Remove(it->first, Standard_False);
                m_sketchElementMap.erase(it); // 从映射表中注销
                break; // 找到了就跳出内层循环
            }
        }
    }
    m_context->UpdateCurrentViewer(); // 刷新屏幕
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

//草图历史记录管理
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

} // namespace cad_ui

#include "QtOccView.moc"
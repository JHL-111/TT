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
    setAutoFillBackground(false);  // Don't fill background to reduce flicker
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
        
        // 设置统一的高亮样式
        // 1. 全局选中样式（普通实体点击后的状态）
        Handle(Prs3d_Drawer) selectedDrawer = m_context->HighlightStyle(Prs3d_TypeOfHighlight_Selected);
        if (!selectedDrawer.IsNull()) {
            selectedDrawer->SetColor(Quantity_NOC_RED);
            selectedDrawer->SetDisplayMode(1);     // Shaded
            selectedDrawer->SetTransparency(0.0f); // 不透明，作为最终选中状态
        }

        // 2. 全局悬停样式（普通实体 hover 预览）
        Handle(Prs3d_Drawer) dynamicDrawer = m_context->HighlightStyle(Prs3d_TypeOfHighlight_Dynamic);
        if (!dynamicDrawer.IsNull()) {
            dynamicDrawer->SetColor(Quantity_NOC_LIGHTSKYBLUE1);
            dynamicDrawer->SetDisplayMode(1);      // Shaded
            dynamicDrawer->SetTransparency(0.15f); // 比选中更轻，作为预览状态
        }

        // 3. 局部选中样式（Face / Edge / Vertex 点击后的状态）
        Handle(Prs3d_Drawer) localSelectedDrawer = m_context->HighlightStyle(Prs3d_TypeOfHighlight_LocalSelected);
        if (!localSelectedDrawer.IsNull()) {
            localSelectedDrawer->SetColor(Quantity_NOC_RED);
            localSelectedDrawer->SetDisplayMode(1);      // 强制填充显示
            localSelectedDrawer->SetTransparency(0.25f); // 轻微透明，便于看清模型
        }

        // 4. 局部悬停样式（Face / Edge / Vertex hover 预览）
        Handle(Prs3d_Drawer) localDynamicDrawer = m_context->HighlightStyle(Prs3d_TypeOfHighlight_LocalDynamic);
        if (!localDynamicDrawer.IsNull()) {
            localDynamicDrawer->SetColor(Quantity_NOC_LIGHTBLUE);
            localDynamicDrawer->SetDisplayMode(1);      // 强制填充显示
            localDynamicDrawer->SetTransparency(0.25f); // 作为预览，不要过重
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

    // 在映射表中找到这个 shape 对应的 AIS 显示对象
    auto it = m_shapeToAIS.find(shape);
    if (it != m_shapeToAIS.end()) {
        Handle(AIS_Shape) aisShape = it->second;
        if (!aisShape.IsNull()) {
            if (visible) {
                m_context->Display(aisShape, Standard_False); // 显示
            }
            else {
                // 如果刚好是被选中的状态，先取消选中
                if (m_currentSelectedAIS == aisShape) {
                    m_context->SetSelected(aisShape, Standard_False);
                    m_currentSelectedAIS.Nullify();
                    m_currentSelectedShape.reset();
                }
                m_context->Erase(aisShape, Standard_False); // 隐藏 (Erase)
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

    // 如果正在预览，点击左键就“确认”这个平面
    if (m_sweepInteractionState == SweepInteractionMode::PreviewingPathPlane && event->button() == Qt::LeftButton) {

        // 1. 退出预览状态
        m_sweepInteractionState = SweepInteractionMode::None;

        // 2. 销毁那个半透明的海绿色预览面
        if (!m_sweepPlanePreview.IsNull()) {
            m_context->Remove(m_sweepPlanePreview, Standard_False);
            m_sweepPlanePreview.Nullify();
        }

        // 3. 正式切入草图模式（相机瞬间摆正）
        EnterSketchMode(m_currentSweepPathCS);

        return; 
    }

    // 优先处理草图模式 
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

    // 非草图模式（或处于临时3D视图中）：左键按下时只记录状态，不立刻旋转
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

    // 如果处于预览平面状态，实时计算并旋转平面
    if (m_sweepInteractionState == SweepInteractionMode::PreviewingPathPlane) {

        // 1. 将 3D 的绝对质心，转换为电脑屏幕上的 2D 像素坐标 (cx, cy)
        Standard_Integer cx, cy;
        m_view->Convert(m_sweepCentroid.X(), m_sweepCentroid.Y(), m_sweepCentroid.Z(), cx, cy);

        // 2. 计算鼠标当前位置，相对于质心屏幕坐标的差值
        double dx = currentPos.x() - cx;
        double dy = currentPos.y() - cy;

        // 3. 防抖机制：如果鼠标离中心点太近（小于 5 像素），就不转，防止角度疯狂跳动
        if (std::sqrt(dx * dx + dy * dy) > 5.0) {

            // 4. 计算纯粹的 2D 屏幕旋转夹角 (弧度)
            // atan2 能够完美覆盖 -180度 到 180度 的死角
            double angle = std::atan2(dy, dx);

            // 5. 以截面的法线为旋转轴，从“0度基准面”开始叠加这个角度
            gp_Trsf rot;
            rot.SetRotation(gp_Ax1(m_sweepCentroid, m_sweepProfileNormal), -angle); // 负号用于匹配鼠标直觉方向

            m_currentSweepPathCS = m_baseSweepPathCS; // 每次都从 0 度重新转
            m_currentSweepPathCS.Transform(rot);

            // 6. 更新并重绘面片
            gp_Pln updatedPln(m_currentSweepPathCS);
            TopoDS_Face updatedFace = BRepBuilderAPI_MakeFace(updatedPln, -5.0, 5.0, -5.0, 5.0).Face();

            Handle(AIS_Shape) aisPlane = Handle(AIS_Shape)::DownCast(m_sweepPlanePreview);
            if (!aisPlane.IsNull()) {
                aisPlane->SetShape(updatedFace);
                m_context->Redisplay(aisPlane, Standard_False);
            }
            m_view->Redraw();
        }
        return; // 拦截事件，不再往下走
    }

    // 优先处理草图模式
    if (IsInSketchMode() && !m_sketchMode->IsTemporary3DViewActive()) {
        m_sketchMode->HandleMouseMove(event);

        if (!HasActiveSketchTool() && !m_context.IsNull()) {
            m_context->MoveTo(currentPos.x(), currentPos.y(), m_view, Standard_True);

            // 已选中的对象不再显示 hover 预览颜色
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

    // 非草图模式：鼠标悬停检测
    if (m_currentMouseButton == Qt::NoButton && !m_context.IsNull()) {
        m_context->MoveTo(currentPos.x(), currentPos.y(), m_view, Standard_False);

        // 已经选中的对象，不再显示 hover 预览颜色
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

    // 左键按住拖动才旋转
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
    // 优先处理草图模式
    if (IsInSketchMode() && !m_sketchMode->IsTemporary3DViewActive()) {
        m_sketchMode->HandleMouseRelease(event);
        m_currentMouseButton = Qt::NoButton;
        m_isLeftDragging = false;
        return;
    }

    // 非草图模式：左键释放时，如果没有拖动，则作为一次点击选择
    if (event->button() == Qt::LeftButton) {
        if (!m_isLeftDragging) {
            HandleSelection(event->pos());
        }
        m_isLeftDragging = false;
    }

    m_currentMouseButton = Qt::NoButton;
}

void QtOccView::keyPressEvent(QKeyEvent* event) {
    
    // 1. 优先拦截空格键，用于开启临时 3D 视角
    if (event->key() == Qt::Key_Space) {
        if (!event->isAutoRepeat() && m_sketchMode && m_sketchMode->IsInSketchMode()) {
            m_sketchMode->StartTemporary3DView();
        }
        event->accept();
        return; // 拦截完毕直接返回，不要往下传了
    }

    // 转发事件给草图模式
    if (m_sketchMode && m_sketchMode->IsInSketchMode()) {
        m_sketchMode->HandleKeyPress(event);
    }

    // 3. 原有的父类调用
    QWidget::keyPressEvent(event);
}


void QtOccView::keyReleaseEvent(QKeyEvent* event) {
 
    // 如果松开的是空格键
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


// 从 OCC 当前选择里提取对象
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

// 专门清理旧状态
void QtOccView::ClearPreviousSelectionState() {
    if (m_currentSelectionMode != 2) { // 边模式支持多选，不在这里清边
        UnhighlightAllVertices();
        UnhighlightAllFaces();
        UnhighlightSketchElement();

        // 普通单选模式下，清掉之前记录的 shape 选中状态
        if (!m_currentSelectedAIS.IsNull()) {
            m_context->SetSelected(m_currentSelectedAIS, Standard_False);
            m_currentSelectedAIS.Nullify();
            m_currentSelectedShape.reset();
        }

        // face / vertex / 普通单选模式下，清空 OCC 的当前选择池
        m_context->ClearSelected(Standard_False);
    }
}

// =========================================================================
// 总入口：处理选择分发
// =========================================================================
void QtOccView::HandleSelection(const QPoint& point) {
    if (m_context.IsNull()) return;

    qDebug() << "HandleSelection called, current selection mode:" << m_currentSelectionMode;

    // 1. 清理阶段：统一清除之前的高亮和选中状态
    ClearPreviousSelectionState();

    // 2. 鼠标位置检测
    m_context->MoveTo(point.x(), point.y(), m_view, Standard_True);

    if (m_context->HasDetected()) {
        // 让 OCC 底层真实选中（触发内部选取逻辑，无论是哪种模式都需要）
        m_context->Select(Standard_True);

        // 3. 按模式分发处理
        switch (m_currentSelectionMode) {
        case 2: // Edge
            ProcessEdgeSelection();
            break;
        case 1: // Vertex
            ProcessVertexSelection();
            break;
        case 4: // Face
            ProcessFaceSelection();
            break;
        default: // 0: Shape 或草图模式
            ProcessShapeOrSketchSelection();
            break;
        }
    }
    else {
        // 4. 点击了空白区域
        qDebug() << "No object detected, clearing all selections";
        if (m_currentSelectionMode != 2) {
            UnhighlightAllEdges(); // 非边模式下清空边高亮
        }
        ClearCentroid();
    }

    // 5. 刷新屏幕
    m_view->Redraw();
    emit ViewChanged();
}

// 子模式处理函数
void QtOccView::ProcessEdgeSelection() {
    qDebug() << "Edge selection mode detected, attempting to select edge...";

    int selectedCount = 0;
    // 边模式可能涉及多选，因此保留循环遍历
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

    // 顶点一般单选，直接利用工具函数获取第一个对象
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

    // 面一般单选，直接利用工具函数获取第一个对象
    Handle(AIS_Shape) aisShape = Handle(AIS_Shape)::DownCast(GetFirstSelectedObject());
    if (!aisShape.IsNull()) {
        Handle(StdSelect_BRepOwner) anOwner = GetFirstSelectedOwner();
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
    Handle(AIS_InteractiveObject) selectedObj = GetFirstSelectedObject();

    if (!selectedObj.IsNull()) {
        Handle(AIS_Shape) aisShape = Handle(AIS_Shape)::DownCast(selectedObj);

        // 1. 点中的是草图闭合轮廓 (Sketch Profile)
        if (m_sketchProfileMap.find(selectedObj) != m_sketchProfileMap.end()) {
            cad_core::ShapePtr profileShape = m_sketchProfileMap[selectedObj];

            m_currentSelectedAIS = aisShape;
            m_currentSelectedShape = profileShape;

            // Sweep 动态平面预览逻辑 
   
            if (m_sweepInteractionState == SweepInteractionMode::SelectingProfile) {

                if (profileShape && !profileShape->GetOCCTShape().IsNull()) {
                    m_sweepCentroid = profileShape->GetCentroid();

                    TopoDS_Face face = TopoDS::Face(profileShape->GetOCCTShape());
                    Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
                    Handle(Geom_Plane) plane = Handle(Geom_Plane)::DownCast(surface);

                    if (!plane.IsNull()) {
                        gp_Ax3 profileCS = plane->Pln().Position();
                        m_sweepProfileNormal = profileCS.Direction(); // 记录原截面法线
                        gp_Dir profileX = profileCS.XDirection();

                        // 切换交互状态为：预览路径平面 (Previewing Path Plane)
                        m_sweepInteractionState = SweepInteractionMode::PreviewingPathPlane;

                        // 生成初始的路径坐标系 (以质心为原点，原法线为X轴，原X轴为法线)
                        m_baseSweepPathCS = gp_Ax3(m_sweepCentroid, profileX, m_sweepProfileNormal);
                        m_currentSweepPathCS = m_baseSweepPathCS;

                        // 生成一个面片用来做视觉预览
                        gp_Pln previewPln(m_currentSweepPathCS);
                        // 缩小 UV 边界范围，让面片变小
                        TopoDS_Face previewFace = BRepBuilderAPI_MakeFace(previewPln, -5.0, 5.0, -5.0, 5.0).Face();

                        // 包装成可显示的 AIS 对象 (AIS_Shape)
                        Handle(AIS_Shape) aisPlane = new AIS_Shape(previewFace);
                        aisPlane->SetColor(Quantity_NOC_LIGHTSEAGREEN); // 海绿色，具有科技感
                        aisPlane->SetTransparency(0.6);                 // 半透明，不遮挡模型
                        aisPlane->SetDisplayMode(AIS_Shaded);           // 实体着色模式
                        aisPlane->SetZLayer(Graphic3d_ZLayerId_Topmost);// 确保显示在最顶层，不被截面遮挡

                        m_sweepPlanePreview = aisPlane;
                        m_context->Display(m_sweepPlanePreview, Standard_False);
                        m_view->Redraw();

                        qDebug() << "Entered Sweep Plane Preview State.";
                    }
                }
            }

            // 发射信号给 UI 面板
            emit ShapeSelected(profileShape);
            qDebug() << "Sketch Profile selected natively.";
        }

        // 2. 点中的是草图元素 
        else if (m_sketchElementMap.find(selectedObj) != m_sketchElementMap.end()) {
            // 先清掉旧的草图高亮覆盖层，避免重复叠加
            UnhighlightSketchElement();

            // 克隆一个临时形状盖在草图元素上方
            m_sketchHighlightAIS = new AIS_Shape(aisShape->Shape());
            m_sketchHighlightAIS->SetColor(Quantity_NOC_BLUE1);
            m_sketchHighlightAIS->SetWidth(4.0);
            m_sketchHighlightAIS->SetPolygonOffsets(Aspect_POM_Line, 1.0f, -2.0f);

            // 仅显示覆盖层，不加入选中池
            m_context->Display(m_sketchHighlightAIS, Standard_False);

            // 记录真实本体
            m_currentSelectedAIS = aisShape;
            qDebug() << "Sketch element selected natively and highlighted with overlay.";
        }

        // 3. 点中的是普通 3D 实体 
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


// 仅供边多选或特殊业务预览使用
// 普通 hover / 选中优先使用 OCC 自带 LocalDynamic / LocalSelected
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


// 仅供特殊点选择或调试预览使用
// 普通 hover / 选中优先使用 OCC 自带 LocalDynamic / LocalSelected
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


// 仅供特殊业务预览使用（如相邻面预览、辅助提示）
// 不再作为普通点击选面的默认高亮方式
void QtOccView::HighlightFace(const TopoDS_Face& face) {
    if (m_context.IsNull()) return;
    
    // 使用临时AIS对象显示高亮的面
    Handle(AIS_Shape) aisFace = new AIS_Shape(face);
    
    // 设置面的高亮属性 - 半透明红色
    aisFace->SetColor(Quantity_NOC_RED);
    aisFace->SetTransparency(0.3); // 半透明
    aisFace->SetDisplayMode(AIS_Shaded);
    
    aisFace->SetZLayer(Graphic3d_ZLayerId_Topmost);

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

        // 4. 如果是样条曲线
        else if (auto curve = std::dynamic_pointer_cast<cad_sketch::SketchCurve>(elem)) {
            const auto& rawPoints = curve->GetControlPoints();

            // 1. 数据清洗：过滤掉距离过近的点
            std::vector<gp_Pnt> validPoints;
            const double MIN_DISTANCE = 1e-5; // 设定一个最小容差距离

            for (const auto& pt : rawPoints) {
                gp_Pnt worldPt = Sketch2DToWorld(pt, cs);
                // 如果是第一个点，或者当前点与上一个有效点的距离大于最小容差，才加入有效列表
                if (validPoints.empty() || validPoints.back().Distance(worldPt) > MIN_DISTANCE) {
                    validPoints.push_back(worldPt);
                }
            }

            // 2. 至少需要两个有效点才能进行插值 
            if (validPoints.size() >= 2) {
                // OCC 的数组索引是从 1 开始的
                Handle(TColgp_HArray1OfPnt) occPoints = new TColgp_HArray1OfPnt(1, validPoints.size());

                for (size_t i = 0; i < validPoints.size(); ++i) {
                    occPoints->SetValue(i + 1, validPoints[i]);
                }

                // 3. 捕获 OCC 底层抛出的异常 
                try {
                    GeomAPI_Interpolate interpolator(occPoints, Standard_False, Precision::Confusion());
                    interpolator.Perform();

                    if (interpolator.IsDone()) {
                        Handle(Geom_BSplineCurve) splineCurve = interpolator.Curve();
                        return BRepBuilderAPI_MakeEdge(splineCurve);
                    }
                }
                catch (...) { // 捕获 Standard_Failure 或其他底层异常
                    qDebug() << "Curve interpolation failed due to invalid geometric input.";
                    // 发生异常时返回空形状，只丢弃这一帧的渲染，绝不让程序闪退
                    return TopoDS_Shape();
                }
            }
        }
        return TopoDS_Shape();
    }

    // 用于追踪每个草图拥有的蓝色轮廓面，防止互相误删
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
                    m_context->Deactivate();   // 禁用普通模型选择
                    // 重新启用当前草图元素的选择能力
                    for (const auto& obj : m_sketchObjects) {
                        if (!obj.IsNull()) {
                            m_context->SetSelectionModeActive(obj, 0, Standard_True);
                        }
                    }

                    // 草图模式下默认按单对象选择处理
                    m_currentSelectionMode = 0;
                }
                emit SketchModeEntered();
            } else {
                qDebug() << "Failed to enter sketch mode";
            }
        }
        catch (const std::exception& e) {
            qDebug() << "Exception in EnterSketchMode:" << e.what();
        }
    }

    // 基于数学坐标系进入草图 
    void QtOccView::EnterSketchMode(const gp_Ax3& customCS) {
        // 懒加载初始化
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
                m_context->Deactivate(); // 禁用普通模型选择
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

    void QtOccView::StartCurveTool() {
        if (m_sketchMode && m_sketchMode->IsInSketchMode()) {
            m_sketchMode->StartCurveTool();
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

        // 5. 显示并保存引用
        m_context->Display(aisFace, Standard_False);
        m_context->Deactivate(aisFace);
        m_highlightedFace = aisFace;
        aisFace->SetPolygonOffsets(Aspect_POM_Fill, 1.0f, -2.0f);
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
            aisLine->SetPolygonOffsets(Aspect_POM_Line, 1.0f, -2.0f);
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

    // 设置草图的可见性
    void QtOccView::SetSketchVisibility(const std::shared_ptr<cad_sketch::Sketch>& sketch, bool visible) {
        if (!sketch || m_context.IsNull()) return;

        // 1. 控制属于该草图的红线 (Elements)
        for (const auto& elem : sketch->GetElements()) {
            for (const auto& pair : m_sketchElementMap) {
                if (pair.second == elem) {
                    if (visible) m_context->Display(pair.first, Standard_False);
                    else m_context->Erase(pair.first, Standard_False);
                }
            }
        }

        // 2. 控制属于该草图的浅蓝色面 (Profiles)
        for (const auto& profile : sketch->GetProfiles()) {
            TopoDS_Face face = profile->GetFace();
            for (const auto& pair : m_sketchProfileMap) {
                // 利用 OCC 底层的 IsSame 精准比对拓扑形状
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

        // 1. 删除该草图对应的所有元素 AIS
        for (const auto& elem : sketch->GetElements()) {
            for (auto it = m_sketchElementMap.begin(); it != m_sketchElementMap.end(); ) {
                if (it->second == elem) {
                    if (m_currentSelectedAIS == it->first) {
                        UnhighlightSketchElement();
                        m_currentSelectedAIS.Nullify();
                        m_currentSelectedShape.reset();
                    }

                    m_context->Remove(it->first, Standard_False);

                    // 从渲染池中安全剔除
                    auto vecIt = std::find(m_sketchObjects.begin(), m_sketchObjects.end(), it->first);
                    if (vecIt != m_sketchObjects.end()) m_sketchObjects.erase(vecIt);

                    it = m_sketchElementMap.erase(it);
                }
                else {
                    ++it;
                }
            }
        }

        // 2. 精准删除该草图对应的所有轮廓面
        cad_sketch::Sketch* sketchKey = sketch.get();
        auto cacheIt = s_sketchProfileCache.find(sketchKey);
        if (cacheIt != s_sketchProfileCache.end()) {
            // 利用精准缓存快速移除面
            for (auto& ais : cacheIt->second) {
                m_context->Remove(ais, Standard_False);
                auto vecIt = std::find(m_sketchProfileObjects.begin(), m_sketchProfileObjects.end(), ais);
                if (vecIt != m_sketchProfileObjects.end()) m_sketchProfileObjects.erase(vecIt);
                m_sketchProfileMap.erase(ais);
            }
            s_sketchProfileCache.erase(cacheIt); // 释放缓存
        }
        else {
            // 缓存中没有（如读档恢复的数据），走遍历删除
            for (const auto& profile : sketch->GetProfiles()) {
                TopoDS_Face face = profile->GetFace();
                for (auto it = m_sketchProfileMap.begin(); it != m_sketchProfileMap.end(); ) {
                    if (it->second && it->second->GetOCCTShape().IsSame(face)) {
                        m_context->Remove(it->first, Standard_False);
                        auto vecIt = std::find(m_sketchProfileObjects.begin(), m_sketchProfileObjects.end(), it->first);
                        if (vecIt != m_sketchProfileObjects.end()) m_sketchProfileObjects.erase(vecIt);
                        it = m_sketchProfileMap.erase(it);
                    }
                    else {
                        ++it;
                    }
                }
            }
        }

        m_context->ClearSelected(Standard_False);
        m_context->UpdateCurrentViewer();
        m_view->Redraw();
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

    // 移动元素后的刷新
    // 移动元素后的刷新 (Update visuals after dragging)
    void QtOccView::UpdateSketchElementVisuals(const cad_sketch::SketchElementPtr& elem) {
        if (m_context.IsNull() || !m_sketchMode) return;

        // 遍历映射表，寻找受拖拽影响的所有图元
        for (const auto& pair : m_sketchElementMap) {
            bool shouldUpdate = false;

            // 1. 如果是当前拖拽的本体，必须更新
            if (pair.second == elem) {
                shouldUpdate = true;
            }
            // 2. 如果拖拽的是一个“点”，我们需要检查有没有直线或曲线依赖这个点
            else if (elem->GetType() == cad_sketch::SketchElementType::Point) {
                // 如果图元是直线，检查它的起点或终点是不是当前拖拽的点
                if (auto line = std::dynamic_pointer_cast<cad_sketch::SketchLine>(pair.second)) {
                    if (line->GetStartPoint() == elem || line->GetEndPoint() == elem) {
                        shouldUpdate = true;
                    }
                }
                // 如果图元是曲线，遍历它的控制点，看看包不包含当前拖拽的点
                else if (auto curve = std::dynamic_pointer_cast<cad_sketch::SketchCurve>(pair.second)) {
                    for (const auto& pt : curve->GetControlPoints()) {
                        if (pt == elem) {
                            shouldUpdate = true;
                            break;
                        }
                    }
                }
            }
            // 3. 反过来，如果拖拽的是一整条曲线，底下的控制点视觉也要跟着平移
            else if (elem->GetType() == cad_sketch::SketchElementType::Curve && pair.second->GetType() == cad_sketch::SketchElementType::Point) {
                auto curve = std::dynamic_pointer_cast<cad_sketch::SketchCurve>(elem);
                for (const auto& pt : curve->GetControlPoints()) {
                    if (pt == pair.second) {
                        shouldUpdate = true;
                        break;
                    }
                }
            }

            // 统一执行重绘
            if (shouldUpdate) {
                Handle(AIS_Shape) aisShape = Handle(AIS_Shape)::DownCast(pair.first);
                if (!aisShape.IsNull()) {
                    // 重新生成拓扑形状并刷新
                    TopoDS_Shape newShape = MakeShapeFromSketchElement(pair.second, m_sketchMode->GetSketchCS());
                    aisShape->SetShape(newShape);
                    m_context->Redisplay(aisShape, Standard_False);

                    // 如果这玩意儿正处于蓝色高亮选中状态，高亮层也要跟着刷
                    if (m_currentSelectedAIS == pair.first && !m_sketchHighlightAIS.IsNull()) {
                        m_sketchHighlightAIS->SetShape(newShape);
                        m_context->Redisplay(m_sketchHighlightAIS, Standard_False);
                    }
                }
            }
        }
        m_view->Redraw(); // 强制刷新屏幕
    }

    std::shared_ptr<cad_sketch::Sketch> QtOccView::GetActiveSketch() const {
        return m_sketchMode ? m_sketchMode->GetCurrentSketch() : nullptr;
    }

    TopoDS_Face QtOccView::GetSketchFace() const {
        if (m_sketchMode) {
            return m_sketchMode->GetSketchFace(); // 调用在 SketchMode 中加好的接口
        }
        return TopoDS_Face(); // 如果草图模式未初始化，返回一个空的面
    }

    TopoDS_Shape QtOccView::GetSelectedSubShape() const {
        if (!m_context.IsNull()) {
            m_context->InitSelected();
            if (m_context->MoreSelected() && m_context->HasSelectedShape()) {
                return m_context->SelectedShape(); // 返回精确选中的 Face / Edge 等
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

    // 渲染草图闭合线框
    void QtOccView::RenderSketchProfiles(const std::vector<cad_sketch::SketchProfilePtr>& profiles) {
        if (m_context.IsNull()) return;

        // 1. 获取当前正在活跃编辑的草图
        auto activeSketch = GetActiveSketch();
        cad_sketch::Sketch* sketchKey = activeSketch ? activeSketch.get() : nullptr;

        // 2. 精准清理当前活跃草图的旧轮廓面，不要碰其他草图的
        if (sketchKey) {
            auto it = s_sketchProfileCache.find(sketchKey);
            if (it != s_sketchProfileCache.end()) {
                for (auto& ais : it->second) {
                    m_context->Remove(ais, Standard_False);
                    // 从全局渲染池中剔除
                    auto vecIt = std::find(m_sketchProfileObjects.begin(), m_sketchProfileObjects.end(), ais);
                    if (vecIt != m_sketchProfileObjects.end()) m_sketchProfileObjects.erase(vecIt);
                    m_sketchProfileMap.erase(ais);
                }
                s_sketchProfileCache.erase(it);
            }
        }
        else {
            // 如果没有活跃草图（如文件刚加载时），走全局清理兜底
            ClearSketchProfiles();
        }

        // 3. 生成并显示新的轮廓面
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

            // 4. 将新面注册到当前草图的缓存中，供下次精准清理
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
        m_sketchProfileMap.clear(); // 清空映射表防止内存泄漏或野指针
        m_view->Redraw();
    }

    void QtOccView::HideSingleSketchProfile(const cad_core::ShapePtr& profileShape) {
        if (!profileShape || m_context.IsNull()) return;

        // 遍历草图轮廓映射表，寻找匹配的业务对象
        for (auto it = m_sketchProfileMap.begin(); it != m_sketchProfileMap.end(); ++it) {
            if (it->second == profileShape) {
                Handle(AIS_InteractiveObject) aisObj = it->first;

                // 1. 从 3D 视图中隐藏该轮廓 (Erase)
                m_context->Erase(aisObj, Standard_False);

                // 2. 将其从对象渲染池中剔除
                auto vecIt = std::find(m_sketchProfileObjects.begin(), m_sketchProfileObjects.end(), aisObj);
                if (vecIt != m_sketchProfileObjects.end()) {
                    m_sketchProfileObjects.erase(vecIt);
                }

                // 3. 从映射表中移除，防止野指针
                m_sketchProfileMap.erase(it);
                break; // 找到了就退出循环
            }
        }
        m_view->Redraw();
    }


    void QtOccView::DrawCentroid(const gp_Pnt& pnt) {
        if (m_context.IsNull()) return;

        ClearCentroid();

        // 视图层只负责“画”，不管数学逻辑
        TopoDS_Shape sphereShape = BRepPrimAPI_MakeSphere(pnt,0.1).Shape();
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

    // Sweep 执行逻辑
    bool QtOccView::ExecuteSweep(double twistAngle, double scaleFactor, bool keepOrientation) {
        if (!m_sketchMode || !m_currentSelectedShape) {
            return false;
        }

        auto pathSketch = m_sketchMode->GetCurrentSketch();
        if (!pathSketch) return false;

        try {
            BRepBuilderAPI_MakeWire wireMaker;

            // 遍历并组装线框
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

            if (!wireMaker.IsDone()) return false;

            auto pathShape = std::make_shared<cad_core::Shape>(wireMaker.Wire());

            cad_feature::SweepFeature sweep;
            sweep.SetProfileShape(m_currentSelectedShape);
            sweep.SetPathShape(pathShape);

            // 应用从 UI 面板传过来的高级参数 
            sweep.SetTwistAngle(twistAngle);
            sweep.SetScaleFactor(scaleFactor);
            sweep.SetKeepOriginalOrientation(keepOrientation);

            auto resultBody = sweep.CreateShape();

            if (resultBody && resultBody->IsValid()) {
                // 1. 渲染金色的 3D Sweep 实体
                Handle(AIS_Shape) aisResult = new AIS_Shape(resultBody->GetOCCTShape());
                aisResult->SetColor(Quantity_NOC_GOLDENROD);
                aisResult->SetMaterial(Graphic3d_NOM_PLASTIC);
                aisResult->SetDisplayMode(AIS_Shaded);
                m_context->Display(aisResult, Standard_False); // 先不重绘，等清理完一起刷

                // 清理掉被“消耗”的路径草图线 
                for (const auto& elem : pathSketch->GetElements()) {
                    if (elem) {
                        for (auto it = m_sketchElementMap.begin(); it != m_sketchElementMap.end(); ) {
                            if (it->second == elem) {
                                // 找到屏幕上对应的渲染线段
                                Handle(AIS_InteractiveObject) aisObj = Handle(AIS_InteractiveObject)::DownCast(it->first);
                                if (!aisObj.IsNull()) {
                                    // 从 OCC 渲染上下文中彻底移除这条线
                                    m_context->Remove(aisObj, Standard_False);
                                }
                                // 从映射表中注销它
                                it = m_sketchElementMap.erase(it);
                            }
                            else {
                                ++it;
                            }
                        }
                    }
                }

                // 2. 打扫战场：退出草图模式，清空预览状态
                m_sketchMode->ExitSketchMode();
                m_sweepInteractionState = SweepInteractionMode::None;
                ClearCentroid();

                // 3. 统一刷新屏幕
                m_view->Redraw();
                return true;
            }
        }
        catch (...) {
            return false;
        }
        return false;
    }
    
    // Sweep 交互状态控制 
    void QtOccView::StartSweepInteraction() {
        // 解锁：进入等待选择截面的状态
        m_sweepInteractionState = SweepInteractionMode::SelectingProfile;
        ClearSelection();
    }

    void QtOccView::CancelSweepInteraction() {
        // 如果不是 Sweep 状态，什么都不做
        if (m_sweepInteractionState == SweepInteractionMode::None) return;

        // 重新上锁
        m_sweepInteractionState = SweepInteractionMode::None;

        // 销毁可能存在的半透明预览面片
        if (!m_sweepPlanePreview.IsNull()) {
            m_context->Remove(m_sweepPlanePreview, Standard_False);
            m_sweepPlanePreview.Nullify();
        }

        // 如果已经切进了草图，强行退出来
        if (IsInSketchMode()) {
            m_sketchMode->ExitSketchMode();
        }

        m_view->Redraw();
        qDebug() << "Sweep interaction cancelled and cleaned up.";
    }

    //开关 Sweep 路径绘制工具
    void QtOccView::ToggleSweepPathTool(bool enableDrawing) {
        if (!m_sketchMode || !IsInSketchMode()) {
            qDebug() << "Cannot toggle tool: Not in sketch mode yet.";
            return;
        }

        if (enableDrawing) {
            // 激活曲线工具，并钉死在质心起点
            m_sketchMode->StartCurveTool();
            auto curveTool = dynamic_cast<cad_ui::SketchCurveTool*>(m_sketchMode->GetCurrentTool());
            if (curveTool) {
                curveTool->InjectStartPoint(0.0, 0.0);
            }
        }
        else {
            // 停止当前工具，退回到默认的“选择模式” (此时可以选中线段并删除)
            m_sketchMode->StopCurrentTool();
        }
    }

} // namespace cad_ui


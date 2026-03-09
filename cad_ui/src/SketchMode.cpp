#include "cad_ui/SketchMode.h"
#include "cad_ui/QtOccView.h"
#include "cad_sketch/SketchPoint.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDebug>

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
    // SketchToolBase Implementation (工具基类实现)
    // =============================================================================
    gp_Pnt SketchToolBase::ScreenToSketchPlane(const QPoint& screenPoint) {
        if (m_view.IsNull()) return gp_Pnt(0, 0, 0);

        Standard_Real X, Y, Z, dX, dY, dZ;
        // 将 2D 屏幕坐标转换为 3D 空间中的一条射线 (Ray)
        m_view->ConvertWithProj(screenPoint.x(), screenPoint.y(), X, Y, Z, dX, dY, dZ);

        gp_Pnt rayOrigin(X, Y, Z);  // 射线起点 (摄像机位置)
        gp_Dir rayDir(dX, dY, dZ);  // 射线方向 (视线方向)
        gp_Lin ray(rayOrigin, rayDir); // 构造射线

        // 计算射线与草图平面 (Sketch Plane) 的解析交点 (Intersection)
        IntAna_IntConicQuad intersection(ray, m_sketchPlane, Precision::Angular(), Precision::Confusion());
        if (intersection.IsDone() && intersection.NbPoints() > 0) {
            return intersection.Point(1); // 返回唯一的交点坐标
        }
        return gp_Pnt(0, 0, 0); // 降级返回原点
    }

    void SketchToolBase::GetSnappedCoordinate(const QPoint& screenPoint, Standard_Real& u, Standard_Real& v) {
        // 1. 正常的屏幕到 3D 平面投影
        gp_Pnt p3d = ScreenToSketchPlane(screenPoint);
        ElSLib::Parameters(m_sketchPlane, p3d, u, v);

        // 2. 如果绑定了捕捉管理器，进行智能吸附
        if (m_snappingManager && m_existingElements) {
            // 将 UV 坐标封装为底层数学核心库认得的 Point
            cad_core::Point inputPt(u, v, 0);

            // 让管理器去寻找附近有没有端点/中点/网格点
            cad_sketch::SnapResult snapRes = m_snappingManager->FindSnapPoint(inputPt, *m_existingElements);

            if (snapRes.found) {
                // 如果找到了！强行篡改原始的鼠标坐标，将其“吸附”过去
                u = snapRes.snapPoint.X();
                v = snapRes.snapPoint.Y();

            }
        }
    }
    // =============================================================================
    // SketchRectangleTool Implementation (矩形工具实现)
    // =============================================================================
    SketchRectangleTool::SketchRectangleTool(QObject* parent) : SketchToolBase(parent) {}

    void SketchRectangleTool::StartDrawing(const QPoint& startPoint) {
        m_isDrawing = true;
        m_startPoint = startPoint;
        m_currentPoint = startPoint;
        m_currentElements.clear(); // 清空上一笔的数据
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
        UpdateDrawing(endPoint); // 最后更新一次位置
        m_isDrawing = false;
        emit elementsCreated(m_currentElements); // 提交最终形状
    }

    void SketchRectangleTool::CancelDrawing() {
        if (!m_isDrawing) return;
        m_isDrawing = false;
        m_currentElements.clear();
        emit drawingCancelled();
    }

    std::vector<cad_sketch::SketchElementPtr> SketchRectangleTool::CreateRectangleLines(Standard_Real u1, Standard_Real v1, Standard_Real u2, Standard_Real v2) {
        std::vector<cad_sketch::SketchElementPtr> elements;

        // 面积过小保护
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
    // SketchLineTool Implementation (直线工具实现)
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
        // 分别获取起点和终点的“吸附后”坐标
        GetSnappedCoordinate(m_startPoint, u1, v1);
        GetSnappedCoordinate(currentPoint, u2, v2);

        // 安全检查：防止生成长度为 0 的线
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
    // SketchMode Implementation (草图模式主控逻辑)
    // =============================================================================

    SketchMode::SketchMode(QtOccView* viewer, QObject* parent)
        : QObject(parent), m_viewer(viewer), m_isActive(false) {
    }

    bool SketchMode::EnterSketchMode(const TopoDS_Face& face) {
        if (m_isActive) ExitSketchMode(); // 防止重复进入
        if (face.IsNull() || !m_viewer) return false;

        try {
            // 1. 备份当前 3D 视口的摄像机状态 (Camera State Backup)
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

            // 2. 初始化草图平面和坐标系
            m_sketchFace = face;
            SetupSketchPlane(face);
            m_isActive = true;

            // 3. 实例化草图数据模型 (Data Model)
            m_currentSketch = std::make_shared<cad_sketch::Sketch>("Sketch_001");

            // 4. 将摄像机切换到正交的草图视角 (Orthographic Sketch View)
            SetupSketchView();

            if (m_viewer) m_viewer->HighlightSketchFace(face);
            emit sketchModeEntered();
            emit statusMessageChanged("Enter Sketch Mode");
            return true;
        }
        catch (...) { return false; }
    }

    void SketchMode::ExitSketchMode() {
        if (!m_isActive) return;
        StopCurrentTool();

        // 恢复摄像机视角 (Restore Camera View)
        RestoreView();

        // 清理并重置数据
        m_currentSketch.reset();
        m_sketchFace = TopoDS_Face();
        m_isActive = false;
        m_viewer->ClearSketchFaceHighlight();
        emit sketchModeExited();
        emit statusMessageChanged("Exited sketch mode");
    }

    void SketchMode::StartRectangleTool() {
        if (!m_isActive) return;
        StopCurrentTool(); // 切换工具前先停止当前工具

        // 实例化工具并注入上下文 (Instantiation and Dependency Injection)
        m_currentTool = std::make_unique<SketchRectangleTool>(this);
        m_currentTool->SetSketchPlane(m_sketchPlane);
        m_currentTool->SetView(m_viewer->GetView());
        m_currentTool->SetSnappingContext(&m_snappingManager, &(m_currentSketch->GetElements()));

        // 绑定信号槽 (Signal and Slot Connections)
        connect(m_currentTool.get(), &SketchToolBase::previewUpdated, this, &SketchMode::OnPreviewUpdated);
        connect(m_currentTool.get(), &SketchToolBase::elementsCreated, this, &SketchMode::OnElementsCreated);
        connect(m_currentTool.get(), &SketchToolBase::drawingCancelled, this, &SketchMode::OnDrawingCancelled);

        emit statusMessageChanged("Started rectangle tool");
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

        emit statusMessageChanged("Started line tool");
    }

    void SketchMode::StopCurrentTool() {
        if (m_currentTool && m_currentTool->IsDrawing()) {
            m_currentTool->CancelDrawing();
        }
    }

    // 以下三个函数将鼠标事件从 View 委托 (Delegate) 给当前激活的工具
    void SketchMode::HandleMousePress(QMouseEvent* event) {
        if (m_isActive && m_currentTool && event->button() == Qt::LeftButton) {
            m_currentTool->StartDrawing(event->pos());
        }
    }

    void SketchMode::HandleMouseMove(QMouseEvent* event) {
        if (m_currentTool && m_currentTool->IsDrawing()) {
            m_currentTool->UpdateDrawing(event->pos());
        }
    }

    void SketchMode::HandleMouseRelease(QMouseEvent* event) {
        if (m_isActive && m_currentTool && event->button() == Qt::LeftButton && m_currentTool->IsDrawing()) {
            m_currentTool->FinishDrawing(event->pos());
        }
    }

    void SketchMode::HandleKeyPress(QKeyEvent* event) {
        if (!m_isActive) return;
        if (event->key() == Qt::Key_Escape) {
            if (m_currentTool && m_currentTool->IsDrawing()) {
                m_currentTool->CancelDrawing();
            }
            else {
                ExitSketchMode(); // 没在画图时按 Esc 则退出草图模式
            }
        }
    }

    void SketchMode::OnElementsCreated(const std::vector<cad_sketch::SketchElementPtr>& elements) {
        if (m_viewer && m_isActive) {
            m_viewer->ClearSketchPreview();

            // 类型安全向下转换 (Type-safe Downcasting)：
            // 兼容原有的 Viewer 接口逻辑，将通用的 Element 转换为具体的 Line
            std::vector<cad_sketch::SketchLinePtr> lines;
            for (auto elem : elements) {
                if (auto line = std::dynamic_pointer_cast<cad_sketch::SketchLine>(elem)) {
                    lines.push_back(line);
                }
            }
            m_viewer->AddSketchLines(lines, m_sketchCS);

            // 将生成的元素录入到数据模型中
            if (m_currentSketch) {
                for (const auto& elem : elements) {
                    m_currentSketch->AddElement(elem);
                    emit sketchElementCreated(elem);
                }
            }
        }
        emit statusMessageChanged(tr("Shape created."));
    }

    void SketchMode::OnDrawingCancelled() {
        emit statusMessageChanged("Drawing cancelled");
    }

    void SketchMode::OnPreviewUpdated(const std::vector<cad_sketch::SketchElementPtr>& elements) {
        if (m_viewer && m_isActive) {
            std::vector<cad_sketch::SketchLinePtr> lines;
            for (auto elem : elements) {
                if (auto line = std::dynamic_pointer_cast<cad_sketch::SketchLine>(elem)) {
                    lines.push_back(line);
                }
            }
            m_viewer->ShowSketchPreviewLines(lines, m_sketchCS); // 调用渲染层进行动态预览更新
        }
    }

    void SketchMode::SetupSketchPlane(const TopoDS_Face& face) {
        // 提取拓扑面对应的几何曲面 (Geometric Surface)
        Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
        Handle(Geom_Plane) plane = Handle(Geom_Plane)::DownCast(surface);

        if (!plane.IsNull()) {
            m_sketchPlane = plane->Pln(); // 获取基础数学平面

            // 计算面的 UV 边界域 (UV Bounds)
            Standard_Real uMin, uMax, vMin, vMax;
            BRepTools::UVBounds(face, uMin, uMax, vMin, vMax);
            Standard_Real uMid = (uMin + uMax) / 2.0;
            Standard_Real vMid = (vMin + vMax) / 2.0;

            // 计算面中心的法向量 (Normal Vector)
            GeomLProp_SLProps props(surface, uMid, vMid, 1, Precision::Confusion());
            gp_Dir normal = props.Normal();

            // 修正拓扑朝向：确保法向量指向实体外部
            if (face.Orientation() == TopAbs_REVERSED) normal.Reverse();

            // 基于正确的外法线重新构建局部坐标系 (Local Coordinate System)
            gp_Ax3 correctCS(m_sketchPlane.Location(), normal, m_sketchPlane.XAxis().Direction());
            m_sketchPlane.SetPosition(correctCS);
            m_sketchCS = m_sketchPlane.Position();
        }
    }

    void SketchMode::SetupSketchView() {
        if (!m_viewer || m_viewer->GetView().IsNull()) return;
        Handle(V3d_View) view = m_viewer->GetView();
        Handle(Graphic3d_Camera) camera = view->Camera();

        // 计算摄像机位置：位于平面法线正上方 100 单位处
        gp_Pnt planeOrigin = m_sketchPlane.Location();
        gp_Dir planeNormal = m_sketchPlane.Axis().Direction();
        gp_Pnt eyePosition = planeOrigin.XYZ() + planeNormal.XYZ() * 100.0;
        gp_Dir yDir = m_sketchCS.YDirection(); // 使得 Y 轴向上

        camera->SetEye(eyePosition);
        camera->SetCenter(planeOrigin);
        camera->SetUp(yDir);
        camera->OrthogonalizeUp(); // 强制使 Up 向量与视线方向垂直

        // 切换为正交投影 (Orthographic Projection)，避免近大远小的透视影响绘图
        camera->SetProjectionType(Graphic3d_Camera::Projection_Orthographic);

        view->FitAll(); // 自动缩放适应屏幕
        view->ZFitAll(); // 调整 Z 深度剪裁平面 (Z Clipping Planes)
    }

    void SketchMode::RestoreView() {
        if (!m_viewer || m_viewer->GetView().IsNull()) return;
        Handle(V3d_View) view = m_viewer->GetView();
        Handle(Graphic3d_Camera) camera = view->Camera();

        // 恢复摄像机备份参数
        camera->SetProjectionType(m_savedProjectionType);
        camera->SetEye(m_savedEye);
        camera->SetCenter(m_savedAt);
        camera->SetUp(m_savedUp);
        camera->OrthogonalizeUp();
        camera->SetScale(m_savedScale);

        view->AutoZFit();
        view->Redraw(); // 触发重绘
    }

    void SketchMode::CreateSketchCoordinateSystem() {
        m_sketchCS = gp_Ax3(m_sketchPlane.Location(), m_sketchPlane.Axis().Direction(), m_sketchPlane.XAxis().Direction());
    }

    gp_Pln SketchMode::ExtractPlaneFromFace(const TopoDS_Face& face) {
        if (face.IsNull()) return gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)); // 默认 XY 平面
        Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
        Handle(Geom_Plane) plane = Handle(Geom_Plane)::DownCast(surface);
        if (!plane.IsNull()) return plane->Pln();
        return gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
    }

} // namespace cad_ui
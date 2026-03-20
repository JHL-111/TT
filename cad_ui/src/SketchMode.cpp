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

    // 1. 三参数函数（直接调用四参数版本，丢弃类型即可，这样就不需要改画图代码了）
    bool SketchToolBase::GetSnappedCoordinate(const QPoint& screenPoint, Standard_Real& u, Standard_Real& v) {
        cad_sketch::SnapType dummyType;
        return GetSnappedCoordinate(screenPoint, u, v, dummyType);
    }

    // 2. 四参数的吸附函数，能把捕捉类型也抓出来
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
        cad_sketch::SnapType snapType; // 准备一个变量来接收类型

        // 调用吸附函数
        if (GetSnappedCoordinate(currentPoint, u, v, snapType)) {
            gp_Pnt p3d = m_sketchPlane.Location().Translated(
                gp_Vec(m_sketchPlane.XAxis().Direction()) * u +
                gp_Vec(m_sketchPlane.YAxis().Direction()) * v
            );
            // 发送信号时，把坐标和类型一起打包发送
            emit snapPointDetected(p3d, snapType);
        }
        else {
            emit snapPointLost();
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
    // SketchPointTool Implementation (点工具实现)
    // =============================================================================
    SketchPointTool::SketchPointTool(QObject* parent) : SketchToolBase(parent) {}

    void SketchPointTool::StartDrawing(const QPoint& startPoint) {
        Standard_Real u, v;
        // 获取带吸附的 2D 坐标
        GetSnappedCoordinate(startPoint, u, v);

        auto pt = std::make_shared<cad_sketch::SketchPoint>(u, v);

        m_currentElements.clear();
        m_currentElements.push_back(pt);

        // 单击即完成绘制，直接发送创建信号
        emit elementsCreated(m_currentElements);

        m_currentElements.clear();
        m_isDrawing = false;
    }

    // 因为点不需要拖拽预览，所以 Update 和 Finish 留空即可
    void SketchPointTool::UpdateDrawing(const QPoint& currentPoint) { Q_UNUSED(currentPoint); }
    void SketchPointTool::FinishDrawing(const QPoint& endPoint) { Q_UNUSED(endPoint); }

    void SketchPointTool::CancelDrawing() {
        m_isDrawing = false;
        m_currentElements.clear();
        emit drawingCancelled();
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
// SketchCircleTool Implementation (圆工具实现)
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
        // 获取圆心和当前鼠标所在点的吸附坐标
        GetSnappedCoordinate(m_centerPoint, u1, v1);
        GetSnappedCoordinate(currentPoint, u2, v2);

        // 计算半径 (Radius)
        double dx = u2 - u1;
        double dy = v2 - v1;
        double radius = std::sqrt(dx * dx + dy * dy);

        // 安全检查：半径不能为 0
        if (radius < Precision::Confusion()) {
            return;
        }

        // 构建圆的数据模型
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
    // SketchArcTool Implementation (圆弧工具实现)
    // =============================================================================
    SketchArcTool::SketchArcTool(QObject* parent) : SketchToolBase(parent), m_state(Init) {}

    void SketchArcTool::StartDrawing(const QPoint& startPoint) {
        if (m_state == Init) {
            // 第一次点击：设置圆心 (Set Center)
            m_isDrawing = true;
            m_centerPoint = startPoint;
            m_state = CenterSet;
            m_currentElements.clear();
        }
        else if (m_state == StartSet) {
            // 第三次交互（点击）：确认终点角并结束 (Confirm End Angle and Finish)
            m_isDrawing = true;
            UpdateDrawing(startPoint);
            emit elementsCreated(m_currentElements); // 提交最终图元

            // 重置状态准备下一次绘制
            m_state = Init;
            m_isDrawing = false;
            m_currentElements.clear();
        }
    }

    void SketchArcTool::UpdateDrawing(const QPoint& currentPoint) {
        Standard_Real u1, v1, u2, v2;
        GetSnappedCoordinate(m_centerPoint, u1, v1);

        if (m_state == CenterSet) {
            // 阶段一：正在拖拽确认半径，使用完整圆进行预览 (Previewing radius with a circle)
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
            // 阶段二：半径已定，正在确认终止角，实时绘制圆弧预览 (Previewing arc based on angles)
            Standard_Real uStart, vStart;
            GetSnappedCoordinate(m_startPoint, uStart, vStart);
            double radius = std::hypot(uStart - u1, vStart - v1);

            GetSnappedCoordinate(currentPoint, u2, v2);
            // 计算起止点的角度 (Calculate start and end angles)
            double startAngle = std::atan2(vStart - v1, uStart - u1);
            double endAngle = std::atan2(v2 - v1, u2 - u1);

            // 将角度归一化到 [0, 2π] 范围 (Normalize angles)
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
            // 用户完成第一次拖拽并松开鼠标，记录起点 (Record Start Point)
            m_startPoint = endPoint;

            Standard_Real u1, v1, u2, v2;
            GetSnappedCoordinate(m_centerPoint, u1, v1);
            GetSnappedCoordinate(m_startPoint, u2, v2);

            // 安全检查：半径不能过小
            if (std::hypot(u2 - u1, v2 - v1) > Precision::Confusion()) {
                m_state = StartSet; // 推进状态机
            }
            else {
                CancelDrawing();
                return;
            }
            m_isDrawing = false; // 结束拖拽状态，进入悬停检测状态
        }
        else if (m_state == StartSet) {
            // 如果用户在最后阶段是以拖拽结束的（备用逻辑）
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
        // 调用基类方法保持吸附检测 (Keep snapping detection from base class)
        SketchToolBase::HoverMove(currentPoint);

        // 悬停鼠标要更新圆弧预览
        if (m_state == CenterSet || m_state == StartSet) {
            UpdateDrawing(currentPoint);
        }
    }

    // =============================================================================
// SketchCurveTool Implementation (曲线工具实现)
// =============================================================================
    SketchCurveTool::SketchCurveTool(QObject* parent) : SketchToolBase(parent) {}

    void SketchCurveTool::StartDrawing(const QPoint& startPoint) {
        m_isDrawing = true;
        Standard_Real u, v;
        GetSnappedCoordinate(startPoint, u, v);

        // 防止在同一个位置重复点击 (Prevent duplicate points)
        if (!m_points.empty()) {
            auto lastPt = m_points.back();
            if (std::hypot(lastPt->GetX() - u, lastPt->GetY() - v) < Precision::Confusion()) {
                return;
            }
        }

        // 每次点击加入一个新控制点 (Add a new control point)
        m_points.push_back(std::make_shared<cad_sketch::SketchPoint>(u, v));
    }

    void SketchCurveTool::UpdateDrawing(const QPoint& currentPoint) {
        if (!m_isDrawing || m_points.empty()) return;

        Standard_Real u, v;
        GetSnappedCoordinate(currentPoint, u, v);

        // 构造动态预览曲线 (Dynamic Preview Curve)
        auto previewCurve = std::make_shared<cad_sketch::SketchCurve>();
        for (const auto& pt : m_points) {
            previewCurve->AddControlPoint(pt);
        }
        // 加入当前鼠标位置作为临时终点
        previewCurve->AddControlPoint(std::make_shared<cad_sketch::SketchPoint>(u, v));

        m_currentElements.clear();
        m_currentElements.push_back(previewCurve);
        emit previewUpdated(m_currentElements);
    }

    void SketchCurveTool::FinishDrawing(const QPoint& endPoint) {
        // 曲线的结束不由鼠标松开(Release)决定，而是由确认方法(ConfirmDrawing)决定
        // 所以这里留空
        Q_UNUSED(endPoint);
    }

    void SketchCurveTool::CancelDrawing() {
        m_isDrawing = false;
        m_points.clear();
        m_currentElements.clear();
        emit drawingCancelled();
    }

    void SketchCurveTool::HoverMove(const QPoint& currentPoint) {
        SketchToolBase::HoverMove(currentPoint); // 保持捕捉提示
        UpdateDrawing(currentPoint);             // 移动时更新曲线拖拽预览
    }

    void SketchCurveTool::ConfirmDrawing() {
        // 需要至少2个点才能成线
        if (!m_isDrawing || m_points.size() < 2) {
            CancelDrawing();
            return;
        }

        auto finalCurve = std::make_shared<cad_sketch::SketchCurve>();
        m_currentElements.clear();

        // 将所有控制点也加入草图图元列表
        // 这样这些点就会在屏幕上渲染为独立的顶点，并且能被鼠标左键选中并拖拽
        for (const auto& pt : m_points) {
            finalCurve->AddControlPoint(pt);
            m_currentElements.push_back(pt);
        }

        // 最后把整条曲线加进去
        m_currentElements.push_back(finalCurve);

        emit elementsCreated(m_currentElements); // 提交所有图元

        m_isDrawing = false;
        m_points.clear();
        m_currentElements.clear();
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
            // 1. 备份当前 3D 视口状态
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
            m_undoStack.clear();
            m_redoStack.clear();

            // 3. 如果有未销毁的草图数据则复用，否则才实例化
            if (!m_currentSketch) {
                m_currentSketch = std::make_shared<cad_sketch::Sketch>("Sketch_001");
            }

            // 4. 将摄像机切换到正交的草图视角
            SetupSketchView();

            // 重新进入时，立刻刷新视口，显示已存在的草图元素
            RefreshSketchView();

            if (m_viewer) m_viewer->HighlightSketchFace(face);
            emit sketchModeEntered();
            emit statusMessageChanged("Enter Sketch Mode");
            return true;
        }
        catch (...) { return false; }
    }

    void SketchMode::ExitSketchMode() {
        if (!m_isActive) return;
        if (m_viewer) m_viewer->HideSnapIndicator();

        StopCurrentTool();

        // 恢复摄像机视角 
        RestoreView();

        // 清理并数据
        m_isActive = false;
        m_viewer->ClearSketchFaceHighlight();
        m_currentSketch.reset();
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

        // 捕捉上下文，让画圆也能吸附
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
        StopCurrentTool(); // 切换工具前先停止当前工具

        m_currentTool = std::make_unique<SketchArcTool>(this);
        m_currentTool->SetSketchPlane(m_sketchPlane);
        m_currentTool->SetView(m_viewer->GetView());

        // 注入捕捉上下文 (Dependency Injection for Snapping Context)
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

        // 发送信号，通知 UI 没有任何工具在运行
        emit toolChanged("None");
    }

    // 以下三个函数将鼠标事件从 View 委托 (Delegate) 给当前激活的工具
    void SketchMode::HandleMousePress(QMouseEvent* event) {
        if (!m_isActive) return;

        // 1. 如果当前有被激活的绘图工具
        if (m_currentTool) {
            if (event->button() == Qt::LeftButton) {
                // 左键：开始绘制或添加控制点
                m_currentTool->StartDrawing(event->pos());
            }
            else if (event->button() == Qt::RightButton) {
                // 右键：针对多次点击的工具（如曲线）进行确认，或其他工具取消
                auto curveTool = dynamic_cast<SketchCurveTool*>(m_currentTool.get());
                if (curveTool) {
                    curveTool->ConfirmDrawing(); // 确认并生成曲线
                }
                else {
                    m_currentTool->CancelDrawing(); // 其他单次点击工具如果按右键则取消
                }
            }
        }
        // 2. 如果当前没有激活绘图工具，并且按下了左键（进入选择或修改模式）
        else if (!m_currentTool && event->button() == Qt::LeftButton) {
            auto selected = m_viewer->GetSelectedSketchElements();
            if (!selected.empty()) {
                m_draggedElements = selected;

                // 按下 Ctrl 键，进入旋转模式 
                if (event->modifiers() & Qt::ControlModifier) {
                    m_isRotating = true;
                    m_isFirstRotation = true; // 标记这是旋转的第一帧
                    GetPlaneCoordinate(event->pos(), m_rotCenterU, m_rotCenterV);
                    m_lastAngle = 0.0;
                }
                // 否则进入平移模式 
                else {
                    m_isDragging = true;
                    GetPlaneCoordinate(event->pos(), m_lastDragU, m_lastDragV);
                }
            }
        }
    }

    void SketchMode::HandleMouseMove(QMouseEvent* event) {
        if (!m_isActive) return;

        if (m_currentTool) {
            m_currentTool->HoverMove(event->pos());
            if (m_currentTool->IsDrawing()) {
                m_currentTool->UpdateDrawing(event->pos());
            }
        }
        else {
            // --- 旋转逻辑 ---
            if (m_isRotating && !m_draggedElements.empty()) {
                double currentU = 0.0, currentV = 0.0;
                GetPlaneCoordinate(event->pos(), currentU, currentV);

                // 计算当前鼠标角度
                double currentAngle = std::atan2(currentV - m_rotCenterV, currentU - m_rotCenterU);

                // 如果是刚按下鼠标的第一帧拖拽，只记录初始角度，不发生旋转
                if (m_isFirstRotation) {
                    m_lastAngle = currentAngle;
                    m_isFirstRotation = false;
                    return;
                }

                // 计算角度差值
                double deltaAngle = currentAngle - m_lastAngle;

                for (auto& elem : m_draggedElements) {
                    elem->Rotate(m_rotCenterU, m_rotCenterV, deltaAngle);
                    m_viewer->UpdateSketchElementVisuals(elem);
                }

                m_lastAngle = currentAngle;
            }
            // --- 平移逻辑 ---
            else if (m_isDragging && !m_draggedElements.empty()) {
                double currentU = 0.0, currentV = 0.0;
                GetPlaneCoordinate(event->pos(), currentU, currentV);

                double dx = currentU - m_lastDragU;
                double dy = currentV - m_lastDragV;

                for (auto& elem : m_draggedElements) {
                    elem->Translate(dx, dy);
                    m_viewer->UpdateSketchElementVisuals(elem);
                }

                m_lastDragU = currentU;
                m_lastDragV = currentV;
            }
        }
    }

    void SketchMode::HandleMouseRelease(QMouseEvent * event) {
        if (!m_isActive) return;

        // 如果当前有激活的绘图工具，交由它完成绘制
        if (m_currentTool && event->button() == Qt::LeftButton && m_currentTool->IsDrawing()) {
            m_currentTool->FinishDrawing(event->pos());
        }

        // 结束平移拖拽
        else if (!m_currentTool && event->button() == Qt::LeftButton) {
            if (m_isDragging || m_isRotating) {
                m_isDragging = false;
                m_isRotating = false;
                m_draggedElements.clear();

                // 拖拽松开后，线条位置发生变化，重新计算并渲染轮廓
                if (m_currentSketch) {
                    m_currentSketch->UpdateProfiles(m_sketchCS);
                    m_viewer->RenderSketchProfiles(m_currentSketch->GetProfiles());
                }

                // 触发历史记录更新，点亮撤销按钮（可将此次平移计入 Undo）
                emit sketchHistoryChanged();
            }
        }
        
    }

    void SketchMode::HandleKeyPress(QKeyEvent* event) {
        if (!m_isActive) return;

        // 回车键确认曲线
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            if (m_currentTool) {
                auto curveTool = dynamic_cast<SketchCurveTool*>(m_currentTool.get());
                if (curveTool) {
                    curveTool->ConfirmDrawing();
                }
            }
            return;
        }

        // 拦截 Delete 键或退格键 Backspace
        if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
            // 如果当前没有在画图（不在拖拽中），就执行删除操作
            if (!m_currentTool || !m_currentTool->IsDrawing()) {
                DeleteSelectedElements();
            }
            return;
        }

        // Esc 键逻辑
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
            m_viewer->HideSnapIndicator();
            m_viewer->ClearSketchPreview();
            // 直接把泛型元素传给 Viewer
            m_viewer->AddSketchElements(elements, m_sketchCS);

            if (m_currentSketch) {
                for (const auto& elem : elements) {
                    m_currentSketch->AddElement(elem);

                    m_undoStack.push_back({ SketchHistoryStep::ADD, elements });
                    m_redoStack.clear();
                    emit sketchHistoryChanged();
                }

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

    // 一个不带吸附的坐标获取函数，专门用于平移计算差值
    void SketchMode::GetPlaneCoordinate(const QPoint& screenPos, double& u, double& v) {
        if (!m_viewer || m_viewer->GetView().IsNull()) return;

        // 1. 使用 ConvertWithProj 获取 3D 空间点 (X, Y, Z) 和视角投影方向 (Vx, Vy, Vz)
        Standard_Real X, Y, Z;
        Standard_Real Vx, Vy, Vz;
        m_viewer->GetView()->ConvertWithProj(screenPos.x(), screenPos.y(), X, Y, Z, Vx, Vy, Vz);

        gp_Pnt p1(X, Y, Z);
        gp_Vec dir(Vx, Vy, Vz);

        // 防止方向向量为零导致崩溃
        if (dir.SquareMagnitude() < Precision::Confusion()) {
            return;
        }

        // 2. 构造一条射线 (Ray)
        gp_Lin ray(p1, dir);

        // 3. 构造草图所在的平面 (Plane)
        gp_Pln pln(m_sketchCS);

        // 4. 计算射线与平面的交点 (Intersection)
        Standard_Real u_param, v_param;
        IntAna_IntConicQuad intersection(ray, pln, Precision::Angular(), Precision::Confusion());
        if (intersection.IsDone() && intersection.NbPoints() > 0) {
            gp_Pnt pt = intersection.Point(1); // 获取 3D 交点
            ElSLib::Parameters(pln, pt, u_param, v_param); // 将 3D 交点转换为平面的 2D (U, V) 坐标
            u = u_param;
            v = v_param;
        }
    }


    void SketchMode::DeleteSelectedElements() {
        if (!m_viewer) return;

        auto selected = m_viewer->GetSelectedSketchElements();
        if (selected.empty()) return; // 没选中东西直接返回

        // 1. 从屏幕上擦除 (Erase from Screen)
        m_viewer->RemoveSketchElements(selected);

        // 2. 从底层数据中删除 (Remove from Data Model)
        if (m_currentSketch) {
            for (auto& elem : selected) {
                m_currentSketch->RemoveElement(elem);
            }
            m_currentSketch->UpdateProfiles(m_sketchCS);
            m_viewer->RenderSketchProfiles(m_currentSketch->GetProfiles());
        }

        // 3. 记录这是一个“删除 (REMOVE)”操作
        m_undoStack.push_back({ SketchHistoryStep::REMOVE, selected });
        m_redoStack.clear();

        emit sketchHistoryChanged(); // 更新撤销/重做按钮状态
        emit statusMessageChanged(tr("Deleted selected element(s)"));
    }

    void SketchMode::RefreshSketchView() {
        if (!m_viewer) return;

        if (m_currentSketch) {
            m_viewer->RemoveSketch(m_currentSketch); // 精准擦除当前草图的旧图元

            m_viewer->AddSketchElements(m_currentSketch->GetElements(), m_sketchCS);
            m_currentSketch->UpdateProfiles(m_sketchCS);
            m_viewer->RenderSketchProfiles(m_currentSketch->GetProfiles());
        }
    }

    void SketchMode::Undo() {
        if (m_undoStack.empty()) return;

        auto step = m_undoStack.back();
        m_undoStack.pop_back();
        m_redoStack.push_back(step); // 压入重做栈

        if (m_currentSketch) {
            if (step.type == SketchHistoryStep::ADD) {
                // 撤销“添加” = 删除它
                for (auto& elem : step.elements) m_currentSketch->RemoveElement(elem);
            }
            else if (step.type == SketchHistoryStep::REMOVE) {
                // 撤销“删除” = 加回它
                for (auto& elem : step.elements) m_currentSketch->AddElement(elem);
            }
        }
        RefreshSketchView(); // 刷新屏幕显示
        emit sketchHistoryChanged();
    }

    void SketchMode::Redo() {
        if (m_redoStack.empty()) return;

        auto step = m_redoStack.back();
        m_redoStack.pop_back();
        m_undoStack.push_back(step); // 压回撤销栈

        if (m_currentSketch) {
            if (step.type == SketchHistoryStep::ADD) {
                // 重做“添加” = 重新加上去
                for (auto& elem : step.elements) m_currentSketch->AddElement(elem);
            }
            else if (step.type == SketchHistoryStep::REMOVE) {
                // 重做“删除” = 再次删掉它
                for (auto& elem : step.elements) m_currentSketch->RemoveElement(elem);
            }
        }
        RefreshSketchView(); // 刷新屏幕显示
        emit sketchHistoryChanged();
    }

} // namespace cad_ui
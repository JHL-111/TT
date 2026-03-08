#include "cad_ui/SketchMode.h"
#include "cad_ui/QtOccView.h"
#include "cad_sketch/SketchPoint.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDebug>
#include <BRep_Tool.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <gp_Pln.hxx>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <Geom_Plane.hxx>
#include <V3d_View.hxx>
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
// SketchRectangleTool Implementation
// =============================================================================

SketchRectangleTool::SketchRectangleTool(QObject* parent)
    : QObject(parent), m_isDrawing(false) {
}

void SketchRectangleTool::StartDrawing(const QPoint& startPoint) {
    m_isDrawing = true;
    m_startPoint = startPoint;
    m_currentPoint = startPoint;
    m_currentLines.clear();
    
    qDebug() << "Rectangle tool: Started drawing at" << startPoint;
}

void SketchRectangleTool::UpdateDrawing(const QPoint& currentPoint) {
    if (!m_isDrawing) return;

    m_currentPoint = currentPoint;

    // 1. 将屏幕鼠标坐标转换为草图平面上的 3D 点
    gp_Pnt p1 = ScreenToSketchPlane(m_startPoint);
    gp_Pnt p2 = ScreenToSketchPlane(m_currentPoint);

    // 2. 生成 2D 局部坐标系的线段集合 (SketchLinePtr)
    // CreateRectangleLines 内部应该处理从 3D Plane 到 2D Local 的投影
    m_currentLines = CreateRectangleLines(p1, p2);

    // 3. 发出预览信号，这个信号会被 SketchMode 接收，然后转交给 Viewer 渲染
    emit previewUpdated(m_currentLines);
}

void SketchRectangleTool::FinishDrawing(const QPoint& endPoint) {
    if (!m_isDrawing) return;

    // 最后的更新
    UpdateDrawing(endPoint);
    m_isDrawing = false;

    // 发出创建确认信号 
    emit rectangleCreated(m_currentLines);
}


void SketchRectangleTool::CancelDrawing() {
    if (!m_isDrawing) {
        return;
    }
    
    m_isDrawing = false;
    m_currentLines.clear();
    
    emit drawingCancelled();
    
    qDebug() << "Rectangle tool: Drawing cancelled";
}

void SketchRectangleTool::SetSketchPlane(const gp_Pln& plane) {
    m_sketchPlane = plane;
}

void SketchRectangleTool::SetView(Handle(V3d_View) view) {
    m_view = view;
}

std::vector<cad_sketch::SketchLinePtr> SketchRectangleTool::GetCurrentRectangle() const {
    return m_currentLines;
}

gp_Pnt SketchRectangleTool::ScreenToSketchPlane(const QPoint& screenPoint) {
    if (m_view.IsNull()) {
        return gp_Pnt(0, 0, 0);
    }

    // 1. 发射射线：使用 ConvertWithProj 获取从相机穿过鼠标位置的 3D 射线 (起点 + 方向)
    Standard_Real X, Y, Z, dX, dY, dZ;
    m_view->ConvertWithProj(screenPoint.x(), screenPoint.y(), X, Y, Z, dX, dY, dZ);

    gp_Pnt rayOrigin(X, Y, Z);
    gp_Dir rayDir(dX, dY, dZ);
    gp_Lin ray(rayOrigin, rayDir); // 构建一条 3D 直线(射线)

    // 2. 求交点：计算射线与草图平面 (m_sketchPlane) 的几何交点
    IntAna_IntConicQuad intersection(ray, m_sketchPlane, Precision::Angular(), Precision::Confusion());

    // 3. 如果求交成功且有交点（视线没有平行于平面）
    if (intersection.IsDone() && intersection.NbPoints() > 0) {
        // 返回这唯一的精确交点
        return intersection.Point(1);
    }

    // 降级保护
    return gp_Pnt(0, 0, 0);
}

std::vector<cad_sketch::SketchLinePtr> SketchRectangleTool::CreateRectangleLines(const gp_Pnt& p1, const gp_Pnt& p2) {
    std::vector<cad_sketch::SketchLinePtr> lines;

    // 检查 4: 零长度检查
    // Precision::Confusion() 是 OpenCASCADE 定义的默认最小精度 (通常是 1e-7)
    if (p1.Distance(p2) < Precision::Confusion()) {
        return lines; // 直接返回空集合，不进行任何 U, V 计算
    }

    Standard_Real u1, v1, u2, v2;
    ElSLib::Parameters(m_sketchPlane, p1, u1, v1);
    ElSLib::Parameters(m_sketchPlane, p2, u2, v2);

    // 检查 5: 投影后的坐标差
    // 如果鼠标只水平或垂直移动了极小距离，也无法构成矩形
    if (Abs(u1 - u2) < Precision::PConfusion() || Abs(v1 - v2) < Precision::PConfusion()) {
        return lines;
    }

    // 只有通过了以上检查，才创建点和线
    auto pt1 = std::make_shared<cad_sketch::SketchPoint>(u1, v1);
    auto pt2 = std::make_shared<cad_sketch::SketchPoint>(u2, v1);
    auto pt3 = std::make_shared<cad_sketch::SketchPoint>(u2, v2);
    auto pt4 = std::make_shared<cad_sketch::SketchPoint>(u1, v2);

    lines.push_back(std::make_shared<cad_sketch::SketchLine>(pt1, pt2));
    lines.push_back(std::make_shared<cad_sketch::SketchLine>(pt2, pt3));
    lines.push_back(std::make_shared<cad_sketch::SketchLine>(pt3, pt4));
    lines.push_back(std::make_shared<cad_sketch::SketchLine>(pt4, pt1));

    return lines;
}
// =============================================================================
// SketchMode Implementation
// =============================================================================

SketchMode::SketchMode(QtOccView* viewer, QObject* parent)
    : QObject(parent), m_viewer(viewer), m_isActive(false) {
    
    // 创建绘制工具
    m_rectangleTool = std::make_unique<SketchRectangleTool>(this);
    
    // 连接信号槽
    // 当矩形正在画（鼠标移动）时，工具发出 2D 线段数据
    connect(m_rectangleTool.get(), &SketchRectangleTool::previewUpdated,
        this, &SketchMode::OnPreviewUpdated);

    // 当矩形画完（鼠标抬起）时，工具发出最终数据
    connect(m_rectangleTool.get(), &SketchRectangleTool::rectangleCreated,
        this, &SketchMode::OnRectangleCreated);

    connect(m_rectangleTool.get(), &SketchRectangleTool::drawingCancelled,
        this, &SketchMode::OnDrawingCancelled);

}

bool SketchMode::EnterSketchMode(const TopoDS_Face& face) {
    if (m_isActive) {
        qDebug() << "Already in sketch mode, exiting first";
        ExitSketchMode();
    }
    
    // 检查参数有效性
    if (face.IsNull()) {
        qDebug() << "Error: Cannot enter sketch mode with null face";
        return false;
    }
    
    if (!m_viewer) {
        qDebug() << "Error: No viewer available";
        return false;
    }
    
    try {
        // 保存当前视图状态
        if (!m_viewer->GetView().IsNull()) {
            Handle(Graphic3d_Camera) camera = m_viewer->GetView()->Camera();
            if (!camera.IsNull()) {
                m_savedEye = camera->Eye();
                m_savedAt = camera->Center();
                m_savedUp = camera->Up();
                m_savedScale = camera->Scale();
                m_savedProjectionType = camera->ProjectionType();
            } else {
                qDebug() << "Warning: Camera is null, using default values";
                m_savedEye = gp_Pnt(0, 0, 100);
                m_savedAt = gp_Pnt(0, 0, 0);
                m_savedUp = gp_Dir(0, 1, 0);
                m_savedScale = 1.0;
            }
        } else {
            qDebug() << "Warning: View is null, using default values";
            m_savedEye = gp_Pnt(0, 0, 100);
            m_savedAt = gp_Pnt(0, 0, 0);
            m_savedUp = gp_Dir(0, 1, 0);
            m_savedScale = 1.0;
        }
        
        // 设置草图信息
        m_sketchFace = face;
        SetupSketchPlane(face);
        
        m_isActive = true;

        // 创建新的草图
        m_currentSketch = std::make_shared<cad_sketch::Sketch>("Sketch_001");
    

        // 设置草图视图
        SetupSketchView();
        
        // 设置绘制工具
        m_rectangleTool->SetSketchPlane(m_sketchPlane);
        m_rectangleTool->SetView(m_viewer->GetView());
        
        m_isActive = true;

        if (m_viewer) {
            m_viewer->HighlightSketchFace(face);
        }
        emit sketchModeEntered();
        emit statusMessageChanged("Enter Sketch Mode - Click the tool to start drawing");
        
        qDebug() << "Entered sketch mode successfully";
        return true;
    }
    catch (const std::exception& e) {
        qDebug() << "Failed to enter sketch mode:" << e.what();
        return false;
    }
}

void SketchMode::ExitSketchMode() {
    if (!m_isActive) {
        return;
    }
    
    // 停止当前工具
    StopCurrentTool();
    
    // 恢复视图
    RestoreView();
    
    // 清理草图数据
    m_currentSketch.reset();
    m_sketchFace = TopoDS_Face();
    
    m_isActive = false;

    m_viewer->ClearSketchFaceHighlight();

    emit sketchModeExited();
    emit statusMessageChanged("Exited sketch mode");
    
    qDebug() << "Exited sketch mode";
}

void SketchMode::StartRectangleTool() {
    if (!m_isActive) {
        return;
    }
    
    StopCurrentTool();
    emit statusMessageChanged("Started rectangle tool");
    
    qDebug() << "Started rectangle tool";
}

void SketchMode::StopCurrentTool() {
    if (m_rectangleTool && m_rectangleTool->IsDrawing()) {
        m_rectangleTool->CancelDrawing();
    }
}

void SketchMode::HandleMousePress(QMouseEvent* event) {
    if (!m_isActive) {
        return;
    }
    
    if (event->button() == Qt::LeftButton) {
        m_rectangleTool->StartDrawing(event->pos());
    }
}

void SketchMode::HandleMouseMove(QMouseEvent* event) {
    if (m_rectangleTool && m_rectangleTool->IsDrawing()) {
        // 工具内部计算 2D 坐标后，会 emit previewUpdated 信号
        m_rectangleTool->UpdateDrawing(event->pos());
    }
}



void SketchMode::HandleMouseRelease(QMouseEvent* event) {
    if (!m_isActive) {
        return;
    }
    
    if (event->button() == Qt::LeftButton && m_rectangleTool->IsDrawing()) {
        m_rectangleTool->FinishDrawing(event->pos());
    }
}

void SketchMode::HandleKeyPress(QKeyEvent* event) {
    if (!m_isActive) {
        return;
    }
    
    if (event->key() == Qt::Key_Escape) {
        if (m_rectangleTool->IsDrawing()) {
            m_rectangleTool->CancelDrawing();
        } else {
            ExitSketchMode();
        }
    }
}

void SketchMode::OnRectangleCreated(const std::vector<cad_sketch::SketchLinePtr>& lines) {
    if (m_viewer && m_isActive) {
        // 1. 先让渲染器清理掉青色的预览线
        m_viewer->ClearSketchPreview();

        // 2. 调用渲染器显示正式的草图线条（黄色），同样传入坐标系
        m_viewer->AddSketchLines(lines, m_sketchCS);

        // 3. 业务层逻辑：存入数据模型
        if (m_currentSketch) {
            for (const auto& line : lines) {
                m_currentSketch->AddElement(line);
                emit sketchElementCreated(line);
            }
        }
    }
    emit statusMessageChanged(tr("Rectangle created."));
}

void SketchMode::OnDrawingCancelled() {
    emit statusMessageChanged("Drawing cancelled");
}

void SketchMode::SetupSketchPlane(const TopoDS_Face& face) {
    Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
    Handle(Geom_Plane) plane = Handle(Geom_Plane)::DownCast(surface);

    if (!plane.IsNull()) {
        // 1. 获取面的基础数学平面
        m_sketchPlane = plane->Pln();

        // 2. 找到这个面在 U 和 V 方向上的边界，计算中心点
        Standard_Real uMin, uMax, vMin, vMax;
        BRepTools::UVBounds(face, uMin, uMax, vMin, vMax);
        Standard_Real uMid = (uMin + uMax) / 2.0;
        Standard_Real vMid = (vMin + vMax) / 2.0;

        // 3. 计算该面上中心点的几何法向量
        GeomLProp_SLProps props(surface, uMid, vMid, 1, Precision::Confusion());
        gp_Dir normal = props.Normal();

        // 4. 结合拓扑方向，算出“真正朝外的法线”
        // 如果面是反向的，说明几何法线指向内部，我们需要把它翻转过来
        if (face.Orientation() == TopAbs_REVERSED) {
            normal.Reverse();
        }

        // 5. 使用算出的向外法线，重新构建草图坐标系 (gp_Ax3)
        // 参数：原点(Location)，Z轴(朝外的法线 normal)，X轴(保持原平面的X轴不变)
        gp_Ax3 correctCS(m_sketchPlane.Location(), normal, m_sketchPlane.XAxis().Direction());

        // 将正确的坐标系赋值给草图平面和成员变量
        m_sketchPlane.SetPosition(correctCS);
        m_sketchCS = m_sketchPlane.Position();

    }
    else {
        emit statusMessageChanged(tr("Selected face is not a plane!"));
    }
}

void SketchMode::SetupSketchView() {
    if (!m_viewer || m_viewer->GetView().IsNull()) {
        qDebug() << "Warning: Cannot setup sketch view - viewer or view is null";
        return;
    }
    
    try {
        Handle(V3d_View) view = m_viewer->GetView();
        Handle(Graphic3d_Camera) camera = view->Camera();
        
        if (camera.IsNull()) {
            qDebug() << "Warning: Camera is null in SetupSketchView";
            return;
        }
        
        // 获取草图平面的法向量和位置
        gp_Pnt planeOrigin = m_sketchPlane.Location();
        gp_Dir planeNormal = m_sketchPlane.Axis().Direction();
        
        // 验证方向向量 (gp_Dir已经是标准化的单位向量)
        try {
            gp_Dir testNormal = planeNormal;
        } catch (...) {
            qDebug() << "Warning: Invalid plane normal, using default Z direction";
            planeNormal = gp_Dir(0, 0, 1);
        }
        
        // 设置视图方向（正对草图平面）
        gp_Pnt eyePosition = planeOrigin.XYZ() + planeNormal.XYZ() * 100.0;
        
        // 验证坐标系Y方向
        gp_Dir yDir;
        try {
            yDir = m_sketchCS.YDirection();
        } catch (...) {
            qDebug() << "Warning: Invalid Y direction, using default";
            yDir = gp_Dir(0, 1, 0);
        }
        
        camera->SetEye(eyePosition);
        camera->SetCenter(planeOrigin);
        camera->SetUp(yDir);
        camera->OrthogonalizeUp();
        // 设置正交投影（对草图更合适）
        camera->SetProjectionType(Graphic3d_Camera::Projection_Orthographic);
        
        // 调整视图大小以适应面
        view->FitAll();
        view->ZFitAll();
        
        qDebug() << "Setup sketch view - Eye:" << eyePosition.X() << eyePosition.Y() << eyePosition.Z();
    }
    catch (const std::exception& e) {
        qDebug() << "Error in SetupSketchView:" << e.what();
    }
}

void SketchMode::RestoreView() {
    if (!m_viewer || m_viewer->GetView().IsNull()) {
        return;
    }

    Handle(V3d_View) view = m_viewer->GetView();
    Handle(Graphic3d_Camera) camera = view->Camera();

    // 1. 恢复原始的投影模式
    camera->SetProjectionType(m_savedProjectionType);

    // 2. 恢复保存的视图状态
    camera->SetEye(m_savedEye);
    camera->SetCenter(m_savedAt);
    camera->SetUp(m_savedUp);

    // 3. 正交化。告诉 OCC：强制修正 Up 向量，使它与视线方向绝对垂直。如果不做这一步，AIS_ViewCube 读取相机矩阵时会直接失效。
    camera->OrthogonalizeUp();

    // 4. 恢复缩放比例
    camera->SetScale(m_savedScale);

    // 5. 重新计算深度剪裁面，防止图形卡在远近平面之外
    view->AutoZFit();
    view->Redraw();

    qDebug() << "Restored view successfully";
}

void SketchMode::OnPreviewUpdated(const std::vector<cad_sketch::SketchLinePtr>& lines) {
    // 只有当视图指针存在且处于草图模式时才渲染
    if (m_viewer && m_isActive) {
        // 调用 QtOccView 的新接口，传入 2D 线段集合和当前平面的坐标系
        m_viewer->ShowSketchPreviewLines(lines, m_sketchCS);
    }
}

void SketchMode::CreateSketchCoordinateSystem() {
    try {
        // 基于草图平面创建坐标系
        gp_Pnt origin = m_sketchPlane.Location();
        gp_Dir zAxis = m_sketchPlane.Axis().Direction();
        gp_Dir xAxis = m_sketchPlane.XAxis().Direction();
        
        // 验证方向向量有效性 (gp_Dir已经是单位向量，检查是否为有效方向)
        try {
            // gp_Dir构造时会自动标准化，这里简单验证即可
            gp_Dir testZ = zAxis;
            gp_Dir testX = xAxis;
        } catch (...) {
            qDebug() << "Warning: Invalid axis directions, using default coordinate system";
            m_sketchCS = gp_Ax3(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
            return;
        }
        
        m_sketchCS = gp_Ax3(origin, zAxis, xAxis);
        qDebug() << "Sketch coordinate system created successfully";
    }
    catch (const std::exception& e) {
        qDebug() << "Error creating sketch coordinate system:" << e.what();
        // 使用默认坐标系
        m_sketchCS = gp_Ax3(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1), gp_Dir(1, 0, 0));
    }
}

gp_Pln SketchMode::ExtractPlaneFromFace(const TopoDS_Face& face) {
    try {
        if (face.IsNull()) {
            qDebug() << "Error: Face is null, using default XY plane";
            return gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
        }
        
        Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
        if (surface.IsNull()) {
            qDebug() << "Error: Surface is null, using default XY plane";
            return gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
        }
        
        Handle(Geom_Plane) plane = Handle(Geom_Plane)::DownCast(surface);
        
        if (!plane.IsNull()) {
            gp_Pln result = plane->Pln();
            qDebug() << "Successfully extracted plane from face";
            return result;
        }
        
        // 如果不是平面，创建一个默认的XY平面
        qDebug() << "Warning: Selected face is not a plane, using XY plane";
        return gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
    }
    catch (const std::exception& e) {
        qDebug() << "Error extracting plane from face:" << e.what();
        return gp_Pln(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1));
    }
}

} // namespace cad_ui


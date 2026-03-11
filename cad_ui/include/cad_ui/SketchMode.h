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
#include "cad_sketch/SnappingManager.h"

// === 开始定义自定义命名空间 ===
namespace cad_ui {

    class QtOccView; 

    /**
     * @class SketchToolBase
     * @brief 草图绘制工具的抽象基类 (Abstract Base Class)
     * * 运用了多态，将各种绘图工具（线、矩形、圆）的通用行为统一管理。
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

        // 将草图平面和视图传递给工具
        void SetSketchPlane(const gp_Pln& plane) { m_sketchPlane = plane; }
        void SetView(Handle(V3d_View) view) { m_view = view; }
        // 注入捕捉上下文（传入管理器和当前草图已有元素）
        void SetSnappingContext(cad_sketch::SnappingManager* manager, const std::vector<cad_sketch::SketchElementPtr>* elements) {
            m_snappingManager = manager;
            m_existingElements = elements;
        }

    signals:
        // 绘制时的动态预览信号
        void previewUpdated(const std::vector<cad_sketch::SketchElementPtr>& elements);
        // 绘制完成的确认信号 
        void elementsCreated(const std::vector<cad_sketch::SketchElementPtr>& elements);
        // 取消绘制信号 
        void drawingCancelled();
        // 检测吸附小圆
        void snapPointDetected(const gp_Pnt& pnt, cad_sketch::SnapType snapType);
        void snapPointLost();

    protected:
        // 将二维的屏幕鼠标坐标转换到三维空间的草图平面上
        gp_Pnt ScreenToSketchPlane(const QPoint& screenPoint);
        // 统一获取智能吸附后的 2D 坐标
        
        // 供真正画图时使用（不需要知道捕捉类型）
        bool GetSnappedCoordinate(const QPoint& screenPoint, Standard_Real& u, Standard_Real& v);
        // 专门供悬停提示使用（能把捕捉类型传出来）
        bool GetSnappedCoordinate(const QPoint& screenPoint, Standard_Real& u, Standard_Real& v, cad_sketch::SnapType& outSnapType);

        bool m_isDrawing;            // 标记当前是否正在绘制中
        gp_Pln m_sketchPlane;        // 当前依附的草图平面 (Sketch Plane)
        Handle(V3d_View) m_view;     // OCC 的视图句柄 (View Handle)

        // 捕捉相关的成员变量
        cad_sketch::SnappingManager* m_snappingManager = nullptr;
        const std::vector<cad_sketch::SketchElementPtr>* m_existingElements = nullptr;
    };

    /**
     * @class SketchRectangleTool
     * @brief 矩形绘制工具 (Rectangle Drawing Tool)
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
        QPoint m_startPoint;   // 起点屏幕坐标
        QPoint m_currentPoint; // 当前鼠标屏幕坐标
        std::vector<cad_sketch::SketchElementPtr> m_currentElements; // 当前正在绘制的图元合集

        // 直接接收处理好的 2D UV 坐标
        std::vector<cad_sketch::SketchElementPtr> CreateRectangleLines(Standard_Real u1, Standard_Real v1, Standard_Real u2, Standard_Real v2);
    };

    /**
     * @class SketchPointTool
     * @brief 草图点绘制工具 (Point Drawing Tool)
     * 只需要单次点击即可生成一个点。
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
     * @brief 直线绘制工具 (Line Drawing Tool)
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
 * @brief 圆绘制工具 (Circle Drawing Tool)
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
     * @brief 圆弧绘制工具 (Arc Drawing Tool)
     * 使用三点交互法：圆心 -> 起点(决定半径和起始角) -> 终点(决定终止角)
     */
    class SketchArcTool : public SketchToolBase {
        Q_OBJECT
    public:
        explicit SketchArcTool(QObject* parent = nullptr);

        void StartDrawing(const QPoint& startPoint) override;
        void UpdateDrawing(const QPoint& currentPoint) override;
        void FinishDrawing(const QPoint& endPoint) override;
        void CancelDrawing() override;

        // 重写以支持鼠标未按下时的预览 (Preview on Hover)
        void HoverMove(const QPoint& currentPoint) override;

    private:
        // 交互状态 (Interaction States)
        enum State { Init, CenterSet, StartSet };
        State m_state;

        QPoint m_centerPoint;
        QPoint m_startPoint;
        std::vector<cad_sketch::SketchElementPtr> m_currentElements;
    };

    /**
     * @class SketchMode
     * @brief 草图模式管理器 (Sketch Mode Manager)
     * * 负责管理草图环境的进入、退出、视图切换，以及分发事件给当前激活的绘制工具。
     */
    class SketchMode : public QObject {
        Q_OBJECT
    public:
        explicit SketchMode(QtOccView* viewer, QObject* parent = nullptr);
        ~SketchMode() = default;

        // 草图生命周期管理
        bool EnterSketchMode(const TopoDS_Face& face); // 进入草图模式
        void ExitSketchMode();                         // 退出草图模式
        bool IsInSketchMode() const { return m_isActive; }
        void Undo();
        void Redo();
        bool CanUndo() const { return !m_undoStack.empty(); }
        bool CanRedo() const { return !m_redoStack.empty(); }

        // 获取草图的上下文数据 (Context Data)
        const cad_sketch::SketchPtr& GetCurrentSketch() const { return m_currentSketch; }
        const gp_Pln& GetSketchPlane() const { return m_sketchPlane; }
        const gp_Ax3& GetSketchCoordinateSystem() const { return m_sketchCS; }
        const TopoDS_Face& GetSketchFace() const { return m_sketchFace; }

        // 绘图工具控制 (Tool Control)
        void StartRectangleTool();
		void StartPointTool();
        void StartLineTool();
        void StartCircleTool();
		void StartArcTool();
        void StopCurrentTool();

        // 交互事件处理 (Event Handling)
        void HandleMousePress(QMouseEvent* event);
        void HandleMouseMove(QMouseEvent* event);
        void HandleMouseRelease(QMouseEvent* event);
        void HandleKeyPress(QKeyEvent* event);


    signals:
        void sketchModeEntered();
        void sketchModeExited();
        void sketchElementCreated(cad_sketch::SketchElementPtr element);
        void sketchHistoryChanged(); // 通知 UI 更新 Undo/Redo 按钮状态
        void statusMessageChanged(const QString& message);

    private slots:
        // 接收工具发出的信号，桥接给渲染器进行显示
        void OnPreviewUpdated(const std::vector<cad_sketch::SketchElementPtr>& elements);
        void OnElementsCreated(const std::vector<cad_sketch::SketchElementPtr>& elements);
        void OnDrawingCancelled();
        void OnSnapPointDetected(const gp_Pnt& pnt, cad_sketch::SnapType snapType);
        void OnSnapPointLost();

    private:
        QtOccView* m_viewer; // 视图渲染器指针
        bool m_isActive;     // 是否处于激活状态

        // 草图核心数据模型
        cad_sketch::SketchPtr m_currentSketch; // 存储绘制的线段与约束
        TopoDS_Face m_sketchFace;              // 依附的三维拓扑面 (Topological Face)
        gp_Pln m_sketchPlane;                  // 提取出的数学平面 (Mathematical Plane)
        gp_Ax3 m_sketchCS;                     // 局部坐标系 (Local Coordinate System, LCS)
        std::vector<std::vector<cad_sketch::SketchElementPtr>> m_undoStack;
        std::vector<std::vector<cad_sketch::SketchElementPtr>> m_redoStack;
        void RefreshSketchView();


        // 视口状态保存 (用于退出草图时恢复原视角)
        gp_Pnt m_savedEye;     // 摄像机位置
        gp_Pnt m_savedAt;      // 观察目标点
        gp_Dir m_savedUp;      // 摄像机向上的向量
        double m_savedScale;   // 缩放比例
        Graphic3d_Camera::Projection m_savedProjectionType; // 投影类型 (透视或正交)

        // 利用多态智能指针管理当前激活的工具 (Current Active Tool)
        std::unique_ptr<SketchToolBase> m_currentTool;

		// 捕捉管理器 (Snapping Manager)
        cad_sketch::SnappingManager m_snappingManager;

        // 内部辅助方法 (Helper Methods)
        void SetupSketchPlane(const TopoDS_Face& face);
        void SetupSketchView();
        void RestoreView();
        void CreateSketchCoordinateSystem();
        gp_Pln ExtractPlaneFromFace(const TopoDS_Face& face);
    };

} // namespace cad_ui

#endif // SKETCHMODE_H
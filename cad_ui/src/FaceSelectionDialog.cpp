#include "cad_ui/FaceSelectionDialog.h"
#include "cad_ui/QtOccView.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QMessageBox>
#include <QDebug>
#include <AIS_ListOfInteractive.hxx>

namespace cad_ui {

FaceSelectionDialog::FaceSelectionDialog(QtOccView* viewer, QWidget* parent)
    : QDialog(parent), m_viewer(viewer), m_isSelecting(false) {
    
    setWindowTitle("Select a face and enter the sketch mode");
    setModal(false);  // 设为非模态，允许与3D视图交互
    setFixedSize(400, 250);
    
    // 设置窗口始终置顶
    setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint);
    
    // 初始化定时器
    m_selectionTimer = new QTimer(this);
    m_selectionTimer->setSingleShot(true);
    connect(m_selectionTimer, &QTimer::timeout, this, &FaceSelectionDialog::OnSelectionTimeout);
    
    SetupUI();
    
    // 连接viewer的face selection信号
    if (m_viewer) {
        connect(m_viewer, &QtOccView::FaceSelected, this, &FaceSelectionDialog::OnFaceSelected);
    }
}

void FaceSelectionDialog::SetupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);
    
    // 说明标签
    m_instructionLabel = new QLabel("Please select a surface in the 3D view to create the sketch plane.");
    m_instructionLabel->setWordWrap(true);
    m_instructionLabel->setStyleSheet("QLabel { font-size: 14px; color: #333; }");
    mainLayout->addWidget(m_instructionLabel);
    
    // 提示标签
    QLabel* hintLabel = new QLabel("hint：Create some geometric shapes (such as cubes, cylinders, etc.，then select the surfaces");
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("QLabel { font-size: 11px; color: #888; font-style: italic; margin-bottom: 10px; }");
    mainLayout->addWidget(hintLabel);
    
    // 状态标签
    m_statusLabel = new QLabel("Click the 'Start Selection' button, and then click on a surface in the 3D view.");
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("QLabel { font-size: 12px; color: #666; background: #f5f5f5; padding: 10px; border: 1px solid #ddd; border-radius: 4px; }");
    mainLayout->addWidget(m_statusLabel);
    
    // 按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    
    // 开始选择按钮
    QPushButton* startButton = new QPushButton("Select");
    startButton->setMinimumHeight(35);
    startButton->setStyleSheet("QPushButton { background: #4CAF50; color: white; border: none; border-radius: 4px; font-weight: bold; }");
    connect(startButton, &QPushButton::clicked, this, &FaceSelectionDialog::StartFaceSelection);
    buttonLayout->addWidget(startButton);
    
    // 确认按钮
    m_confirmButton = new QPushButton("confirm");
    m_confirmButton->setMinimumHeight(35);
    m_confirmButton->setEnabled(false);
    m_confirmButton->setStyleSheet("QPushButton:enabled { background: #2196F3; color: white; border: none; border-radius: 4px; font-weight: bold; } QPushButton:disabled { background: #ccc; color: #999; }");
    connect(m_confirmButton, &QPushButton::clicked, this, &FaceSelectionDialog::ConfirmSelection);
    buttonLayout->addWidget(m_confirmButton);
    
    // 取消按钮
    m_cancelButton = new QPushButton("cancel");
    m_cancelButton->setMinimumHeight(35);
    m_cancelButton->setStyleSheet("QPushButton { background: #f44336; color: white; border: none; border-radius: 4px; font-weight: bold; }");
    connect(m_cancelButton, &QPushButton::clicked, this, &FaceSelectionDialog::CancelSelection);
    buttonLayout->addWidget(m_cancelButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // 添加弹性空间
    mainLayout->addStretch();
}

void FaceSelectionDialog::StartFaceSelection() {
    if (!m_viewer) {
        QMessageBox::warning(this, "error", "3D view is unavailable");
        return;
    }
    
    // 检查是否有可用的几何体  
    auto context = m_viewer->GetContext();
    if (context.IsNull()) {
        QMessageBox::warning(this, "error", "3D view context is unavailable");
        return;
    }
    
    // 检查是否有可选择的对象
    context->DisplayedObjects(AIS_ListOfInteractive());
    AIS_ListOfInteractive objects;
    context->DisplayedObjects(objects);
    if (objects.IsEmpty()) {
        QMessageBox::information(this, "hint", "Create some geometric shapes (such as cubes, cylinders, etc.), and then select the surfaces.");
        return;
    }
    
    m_isSelecting = true;
    
    // 设置面选择模式
    EnableFaceSelectionMode();
    
    // 更新UI状态
    m_statusLabel->setText(" The face selection mode has been activated. Please click on a face in the 3D view.");
    m_statusLabel->setStyleSheet("QLabel { font-size: 12px; color: #4CAF50; background: #e8f5e8; padding: 10px; border: 1px solid #4CAF50; border-radius: 4px; }");
    
    // 启动选择超时
    m_selectionTimer->start(SELECTION_TIMEOUT_MS);
    
    qDebug() << "Face selection mode started, objects count:" << objects.Size();
}

void FaceSelectionDialog::EnableFaceSelectionMode() {
    if (!m_viewer) return;
    
    // 设置为面选择模式
    m_viewer->SetSelectionMode(4); // Face selection mode
    qDebug() << "Face selection mode enabled";
}

void FaceSelectionDialog::DisableFaceSelectionMode() {
    if (!m_viewer) return;
    
    // 恢复默认选择模式
    m_viewer->SetSelectionMode(0); // Shape selection mode
}

void FaceSelectionDialog::OnFaceSelected(const TopoDS_Face& face) {
    if (!m_isSelecting) {
        return; // 不在选择模式中，忽略
    }
    
    // 停止选择超时
    m_selectionTimer->stop();
    
    // 保存选择的面
    m_selectedFace = face;
    
    // 更新UI状态
    UpdateSelectionStatus();
    
    qDebug() << "Face selected in dialog";
}

void FaceSelectionDialog::UpdateSelectionStatus() {
    if (!m_selectedFace.IsNull()) {
        m_statusLabel->setText("A face has been selected! Click 'Confirm' to enter the sketch mode.");
        m_statusLabel->setStyleSheet("QLabel { font-size: 12px; color: #4CAF50; background: #e8f5e8; padding: 10px; border: 1px solid #4CAF50; border-radius: 4px; }");
        m_confirmButton->setEnabled(true);
    } else {
        m_statusLabel->setText("A face has been selected! Click 'Confirm' to enter the sketch mode.");
        m_statusLabel->setStyleSheet("QLabel { font-size: 12px; color: #666; background: #f5f5f5; padding: 10px; border: 1px solid #ddd; border-radius: 4px; }");
        m_confirmButton->setEnabled(false);
    }
}

void FaceSelectionDialog::OnSelectionTimeout() {
    m_statusLabel->setText("Please start over and make a new choice.");
    m_statusLabel->setStyleSheet("QLabel { font-size: 12px; color: #f44336; background: #ffeaea; padding: 10px; border: 1px solid #f44336; border-radius: 4px; }");
    
    m_isSelecting = false;
    DisableFaceSelectionMode();
}

void FaceSelectionDialog::ConfirmSelection() {
    if (m_selectedFace.IsNull()) {
        QMessageBox::warning(this, "warning", "Please select a surface first.");
        return;
    }
    
    // 禁用面选择模式
    DisableFaceSelectionMode();
    
    // 发射信号
    emit faceSelected(m_selectedFace);
    
    // 接受对话框
    accept();
}

void FaceSelectionDialog::CancelSelection() {
    // 停止选择超时
    m_selectionTimer->stop();
    
    // 禁用面选择模式
    DisableFaceSelectionMode();
    
    // 清除选择
    m_isSelecting = false;
    m_selectedFace = TopoDS_Face();
    
    // 发射取消信号
    emit selectionCancelled();
    
    // 拒绝对话框
    reject();
}

} // namespace cad_ui

#include "FaceSelectionDialog.moc"
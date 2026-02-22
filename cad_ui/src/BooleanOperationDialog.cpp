#include "cad_ui/BooleanOperationDialog.h"
#include <QApplication>
#include <QMessageBox>
#include <QSplitter>

namespace cad_ui {

BooleanOperationDialog::BooleanOperationDialog(BooleanOperationType operationType, QWidget* parent)
    : QDialog(parent), m_operationType(operationType), m_selectingTargets(false), m_selectingTools(false) {
    setupUI();
    setModal(false); // Allow interaction with main window for selection
    setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint);
    resize(400, 500);
}

void BooleanOperationDialog::setupUI() {
    setWindowTitle(getOperationTitle());
    
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(8);
    m_mainLayout->setContentsMargins(12, 12, 12, 12);
    
    // Selection group
    m_selectionGroup = new QGroupBox("selectobject", this);
    m_selectionLayout = new QGridLayout(m_selectionGroup);
    m_selectionLayout->setSpacing(8);
    
    // Target objects section
    m_targetLabel = new QLabel(getTargetLabel(), this);
    m_targetLabel->setStyleSheet("font-weight: bold; color: #333333;");
    m_selectionLayout->addWidget(m_targetLabel, 0, 0, 1, 2);
    
    // Target selection frame (UGNX style)
    m_targetFrame = new QFrame(this);
    m_targetFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    m_targetFrame->setMinimumHeight(40);
    m_targetFrame->setStyleSheet(
        "QFrame { background-color: #F8F8F8; border: 2px solid #E0E0E0; border-radius: 4px; }"
        "QFrame:hover { border-color: #009999; }"
    );
    
    m_targetFrameLayout = new QHBoxLayout(m_targetFrame);
    m_targetFrameLayout->setContentsMargins(8, 4, 8, 4);
    
    m_targetCount = new QLabel("please select an object", this);
    m_targetCount->setStyleSheet("color: #666666; font-style: italic;");
    m_targetFrameLayout->addWidget(m_targetCount);
    m_targetFrameLayout->addStretch();
    
    m_targetSelectButton = new QPushButton("select", this);
    m_targetSelectButton->setMinimumSize(60, 24);
    m_targetSelectButton->setStyleSheet(
        "QPushButton { background-color: #009999; color: white; border: none; border-radius: 3px; font-weight: bold; }"
        "QPushButton:hover { background-color: #00CCCC; }"
        "QPushButton:pressed { background-color: #007777; }"
    );
    m_targetFrameLayout->addWidget(m_targetSelectButton);
    
    m_selectionLayout->addWidget(m_targetFrame, 1, 0, 1, 2);
    
    // Target list widget (collapsed by default)
    m_targetList = new QListWidget(this);
    m_targetList->setMaximumHeight(80);
    m_targetList->setVisible(false);
    m_targetList->setStyleSheet(
        "QListWidget { border: 1px solid #E0E0E0; border-radius: 3px; background-color: white; }"
        "QListWidget::item { padding: 4px; border-bottom: 1px solid #F0F0F0; }"
        "QListWidget::item:selected { background-color: #009999; color: white; }"
    );
    m_selectionLayout->addWidget(m_targetList, 2, 0, 1, 2);
    
    // Add spacing
    m_selectionLayout->addItem(new QSpacerItem(20, 8, QSizePolicy::Minimum, QSizePolicy::Fixed), 3, 0);
    
    // Tool objects section (only for difference and intersection)
    if (m_operationType == BooleanOperationType::Difference || 
        m_operationType == BooleanOperationType::Intersection) {
        
        m_toolLabel = new QLabel(getToolLabel(), this);
        m_toolLabel->setStyleSheet("font-weight: bold; color: #333333;");
        m_selectionLayout->addWidget(m_toolLabel, 4, 0, 1, 2);
        
        // Tool selection frame
        m_toolFrame = new QFrame(this);
        m_toolFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
        m_toolFrame->setMinimumHeight(40);
        m_toolFrame->setStyleSheet(
            "QFrame { background-color: #F8F8F8; border: 2px solid #E0E0E0; border-radius: 4px; }"
            "QFrame:hover { border-color: #009999; }"
        );
        
        m_toolFrameLayout = new QHBoxLayout(m_toolFrame);
        m_toolFrameLayout->setContentsMargins(8, 4, 8, 4);
        
        m_toolCount = new QLabel("please select an object", this);
        m_toolCount->setStyleSheet("color: #666666; font-style: italic;");
        m_toolFrameLayout->addWidget(m_toolCount);
        m_toolFrameLayout->addStretch();
        
        m_toolSelectButton = new QPushButton("select", this);
        m_toolSelectButton->setMinimumSize(60, 24);
        m_toolSelectButton->setStyleSheet(
            "QPushButton { background-color: #009999; color: white; border: none; border-radius: 3px; font-weight: bold; }"
            "QPushButton:hover { background-color: #00CCCC; }"
            "QPushButton:pressed { background-color: #007777; }"
        );
        m_toolFrameLayout->addWidget(m_toolSelectButton);
        
        m_selectionLayout->addWidget(m_toolFrame, 5, 0, 1, 2);
        
        // Tool list widget
        m_toolList = new QListWidget(this);
        m_toolList->setMaximumHeight(80);
        m_toolList->setVisible(false);
        m_toolList->setStyleSheet(
            "QListWidget { border: 1px solid #E0E0E0; border-radius: 3px; background-color: white; }"
            "QListWidget::item { padding: 4px; border-bottom: 1px solid #F0F0F0; }"
            "QListWidget::item:selected { background-color: #009999; color: white; }"
        );
        m_selectionLayout->addWidget(m_toolList, 6, 0, 1, 2);
    } else {
        // Hide tool selection for union operations
        m_toolLabel = nullptr;
        m_toolFrame = nullptr;
        m_toolCount = nullptr;
        m_toolSelectButton = nullptr;
        m_toolList = nullptr;
    }
    
    m_mainLayout->addWidget(m_selectionGroup);
    m_mainLayout->addStretch();
    
    // Control buttons
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setSpacing(8);
    
    m_previewButton = new QPushButton("preview", this);
    m_previewButton->setMinimumSize(80, 32);
    m_previewButton->setEnabled(false);
    
    m_buttonLayout->addWidget(m_previewButton);
    m_buttonLayout->addStretch();
    
    m_cancelButton = new QPushButton("cancel", this);
    m_cancelButton->setMinimumSize(80, 32);
    
    m_okButton = new QPushButton("confirm", this);
    m_okButton->setMinimumSize(80, 32);
    m_okButton->setDefault(true);
    m_okButton->setEnabled(false);
    
    m_buttonLayout->addWidget(m_cancelButton);
    m_buttonLayout->addWidget(m_okButton);
    
    m_mainLayout->addLayout(m_buttonLayout);
    
    // Connect signals
    connect(m_targetSelectButton, &QPushButton::clicked, this, &BooleanOperationDialog::onTargetSelectionClicked);
    if (m_toolSelectButton) {
        connect(m_toolSelectButton, &QPushButton::clicked, this, &BooleanOperationDialog::onToolSelectionClicked);
    }
    
    connect(m_okButton, &QPushButton::clicked, [this]() {
        emit operationRequested(m_operationType, m_targetObjects, m_toolObjects);
        accept();
    });
    
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    // Style the dialog
    setStyleSheet(
        "QDialog { background-color: #F5F5F5; }"
        "QGroupBox { font-weight: bold; color: #333333; border: 1px solid #E0E0E0; border-radius: 6px; margin-top: 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px 0 4px; }"
        "QPushButton { background-color: #009999; color: white; border: none; border-radius: 4px; padding: 6px 12px; font-weight: bold; }"
        "QPushButton:hover { background-color: #00CCCC; }"
        "QPushButton:pressed { background-color: #007777; }"
        "QPushButton:disabled { background-color: #CCCCCC; color: #999999; }"
    );
}

void BooleanOperationDialog::onTargetSelectionClicked() {
    m_selectingTargets = true;
    m_selectingTools = false;
    
    m_targetSelectButton->setText("Finish");
    m_targetSelectButton->setStyleSheet(
        "QPushButton { background-color: #FF6B35; color: white; border: none; border-radius: 3px; font-weight: bold; }"
        "QPushButton:hover { background-color: #FF8C5A; }"
    );
    
    if (m_toolSelectButton) {
        m_toolSelectButton->setEnabled(false);
    }
    
    QString prompt = (m_operationType == BooleanOperationType::Union) ? 
                    "Please select the objects you want to merge in the 3D view, and then click the 'Finish' button." :
                    "Please select the operation object in the 3D view, and then click the 'Finish' button.";
    
    emit selectionModeChanged(true, prompt);
    
    // Change button to finish selection
    disconnect(m_targetSelectButton, &QPushButton::clicked, this, &BooleanOperationDialog::onTargetSelectionClicked);
    connect(m_targetSelectButton, &QPushButton::clicked, this, &BooleanOperationDialog::onSelectionFinished);
}

void BooleanOperationDialog::onToolSelectionClicked() {
    if (!m_toolSelectButton) return;
    
    m_selectingTargets = false;
    m_selectingTools = true;
    
    m_toolSelectButton->setText("Finish");
    m_toolSelectButton->setStyleSheet(
        "QPushButton { background-color: #FF6B35; color: white; border: none; border-radius: 3px; font-weight: bold; }"
        "QPushButton:hover { background-color: #FF8C5A; }"
    );
    
    m_targetSelectButton->setEnabled(false);
    
    QString prompt = "Please select the tool object in the 3D view, and then click the 'Finish' button.";
    emit selectionModeChanged(true, prompt);
    
    // Change button to finish selection
    disconnect(m_toolSelectButton, &QPushButton::clicked, this, &BooleanOperationDialog::onToolSelectionClicked);
    connect(m_toolSelectButton, &QPushButton::clicked, this, &BooleanOperationDialog::onSelectionFinished);
}

void BooleanOperationDialog::onObjectSelected(const cad_core::ShapePtr& shape) {
    if (m_selectingTargets) {
        m_targetObjects.push_back(shape);
    } else if (m_selectingTools) {
        m_toolObjects.push_back(shape);
    }
    updateSelectionDisplay();
}

void BooleanOperationDialog::onSelectionFinished() {
    m_selectingTargets = false;
    m_selectingTools = false;
    
    // Reset target button
    m_targetSelectButton->setText("select");
    m_targetSelectButton->setStyleSheet(
        "QPushButton { background-color: #009999; color: white; border: none; border-radius: 3px; font-weight: bold; }"
        "QPushButton:hover { background-color: #00CCCC; }"
        "QPushButton:pressed { background-color: #007777; }"
    );
    m_targetSelectButton->setEnabled(true);
    
    // Reset tool button
    if (m_toolSelectButton) {
        m_toolSelectButton->setText("select");
        m_toolSelectButton->setStyleSheet(
            "QPushButton { background-color: #009999; color: white; border: none; border-radius: 3px; font-weight: bold; }"
            "QPushButton:hover { background-color: #00CCCC; }"
            "QPushButton:pressed { background-color: #007777; }"
        );
        m_toolSelectButton->setEnabled(true);
    }
    
    // Reconnect original handlers
    disconnect(m_targetSelectButton, &QPushButton::clicked, this, &BooleanOperationDialog::onSelectionFinished);
    connect(m_targetSelectButton, &QPushButton::clicked, this, &BooleanOperationDialog::onTargetSelectionClicked);
    
    if (m_toolSelectButton) {
        disconnect(m_toolSelectButton, &QPushButton::clicked, this, &BooleanOperationDialog::onSelectionFinished);
        connect(m_toolSelectButton, &QPushButton::clicked, this, &BooleanOperationDialog::onToolSelectionClicked);
    }
    
    emit selectionModeChanged(false, "");
    updateSelectionDisplay();
}

void BooleanOperationDialog::updateSelectionDisplay() {
    // Update target display
    if (!m_targetObjects.empty()) {
        m_targetCount->setText(QString("%1 objects have been selected").arg(m_targetObjects.size()));
        m_targetCount->setStyleSheet("color: #009999; font-weight: bold;");
        m_targetList->setVisible(true);
        
        m_targetList->clear();
        for (size_t i = 0; i < m_targetObjects.size(); ++i) {
            m_targetList->addItem(QString("object %1").arg(i + 1));
        }
    } else {
        m_targetCount->setText("please select an object");
        m_targetCount->setStyleSheet("color: #666666; font-style: italic;");
        m_targetList->setVisible(false);
    }
    
    // Update tool display
    if (m_toolCount && m_toolList) {
        if (!m_toolObjects.empty()) {
            m_toolCount->setText(QString("%1 objects have been selected").arg(m_toolObjects.size()));
            m_toolCount->setStyleSheet("color: #009999; font-weight: bold;");
            m_toolList->setVisible(true);
            
            m_toolList->clear();
            for (size_t i = 0; i < m_toolObjects.size(); ++i) {
                m_toolList->addItem(QString("toolobject %1").arg(i + 1));
            }
        } else {
            m_toolCount->setText("please select an object");
            m_toolCount->setStyleSheet("color: #666666; font-style: italic;");
            m_toolList->setVisible(false);
        }
    }
    
    // Update button states
    bool canExecute = false;
    if (m_operationType == BooleanOperationType::Union) {
        // Union: need at least 2 objects total (targets + tools)
        canExecute = (m_targetObjects.size() + m_toolObjects.size()) >= 2;
    } else {
        // Intersection/Difference: need both targets and tools
        canExecute = !m_targetObjects.empty() && !m_toolObjects.empty();
    }
    
    m_okButton->setEnabled(canExecute);
    m_previewButton->setEnabled(canExecute);
}

QString BooleanOperationDialog::getOperationTitle() const {
    switch (m_operationType) {
        case BooleanOperationType::Union:
            return "Union";
        case BooleanOperationType::Intersection:
            return "Intersection";
        case BooleanOperationType::Difference:
            return "Difference";
        default:
            return "BooleanOperation";
    }
}

QString BooleanOperationDialog::getTargetLabel() const {
    switch (m_operationType) {
        case BooleanOperationType::Union:
            return "objects:";
        case BooleanOperationType::Intersection:
        case BooleanOperationType::Difference:
            return "object:";
        default:
            return "targetobject:";
    }
}

QString BooleanOperationDialog::getToolLabel() const {
    switch (m_operationType) {
        case BooleanOperationType::Intersection:
            return "object:";
        case BooleanOperationType::Difference:
            return "toolobject:";
        default:
            return "toolobject:";
    }
}

} // namespace cad_ui
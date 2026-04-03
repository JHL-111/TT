#include "cad_ui/CreateLoftDialog.h"
#include "cad_ui/QtOccView.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>

namespace cad_ui {

    CreateLoftDialog::CreateLoftDialog(QtOccView* viewer, QWidget* parent)
        : QDialog(parent), m_viewer(viewer) {

        setWindowTitle("Create Loft Feature");
        setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
        resize(300, 200);

        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->addWidget(new QLabel("Select 2 or more profiles (Faces/Wires):"));

        m_sectionList = new QListWidget(this);
        mainLayout->addWidget(m_sectionList);

        QHBoxLayout* btnLayout = new QHBoxLayout();
        m_clearBtn = new QPushButton("Clear Selection", this);
        m_confirmBtn = new QPushButton("Create Loft", this);
        m_confirmBtn->setEnabled(false); // 必须选够两个才能点

        btnLayout->addWidget(m_clearBtn);
        btnLayout->addWidget(m_confirmBtn);
        mainLayout->addLayout(btnLayout);

        connect(m_clearBtn, &QPushButton::clicked, this, &CreateLoftDialog::OnClear);
        connect(m_confirmBtn, &QPushButton::clicked, this, &CreateLoftDialog::OnConfirm);

        if (m_viewer) {
            m_viewer->SetSelectionMode(0);
        }
    }

    void CreateLoftDialog::SetSelectedShape(cad_core::ShapePtr shape) {
        if (!shape) return;

        // 防抖：避免同一个面被重复添加
        for (const auto& existing : m_selectedSections) {
            if (existing->GetOCCTShape().IsSame(shape->GetOCCTShape())) {
                return;
            }
        }

        m_selectedSections.push_back(shape);
        m_sectionList->addItem(QString("Profile %1 Selected").arg(m_selectedSections.size()));

        if (m_selectedSections.size() >= 2) {
            m_confirmBtn->setEnabled(true);
        }
    }

    void CreateLoftDialog::OnClear() {
        m_selectedSections.clear();
        m_sectionList->clear();
        m_confirmBtn->setEnabled(false);
        if (m_viewer) m_viewer->ClearSelection();
    }

    void CreateLoftDialog::OnConfirm() {
        // 默认生成实体 
        emit loftRequested(m_selectedSections, true);
        accept();
    }

} // namespace cad_ui
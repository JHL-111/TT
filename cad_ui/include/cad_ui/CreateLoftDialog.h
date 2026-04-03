#pragma once

#include <QDialog>
#include <vector>
#include "cad_core/Shape.h"

class QListWidget;
class QPushButton;

namespace cad_ui {

    class QtOccView;

    class CreateLoftDialog : public QDialog {
        Q_OBJECT
    public:
        explicit CreateLoftDialog(QtOccView* viewer, QWidget* parent = nullptr);
        ~CreateLoftDialog() = default;

        // 接收从 MainWindow 传来的选中实体
        void SetSelectedShape(cad_core::ShapePtr shape);

    signals:
        void loftRequested(const std::vector<cad_core::ShapePtr>& sections, bool isSolid);

    private slots:
        void OnConfirm();
        void OnClear();

    private:
        QtOccView* m_viewer;
        std::vector<cad_core::ShapePtr> m_selectedSections;

        QListWidget* m_sectionList;
        QPushButton* m_confirmBtn;
        QPushButton* m_clearBtn;
    };

} // namespace cad_ui

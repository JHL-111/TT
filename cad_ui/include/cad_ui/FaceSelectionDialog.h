#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <TopoDS_Face.hxx>
#include <memory>

namespace cad_ui {

    class QtOccView;

    class FaceSelectionDialog : public QDialog {
        Q_OBJECT

    public:
        explicit FaceSelectionDialog(QtOccView* viewer, QWidget* parent = nullptr);
        ~FaceSelectionDialog() = default;

        // Get the selected face
        TopoDS_Face GetSelectedFace() const { return m_selectedFace; }
        bool HasValidSelection() const { return !m_selectedFace.IsNull(); }

    public slots:
        void StartFaceSelection();
        void CancelSelection();
        void ConfirmSelection();

    private slots:
        void OnFaceSelected(const TopoDS_Face& face);
        void OnSelectionTimeout();

    signals:
        void faceSelected(const TopoDS_Face& face);
        void selectionCancelled();

    private:
        void SetupUI();
        void EnableFaceSelectionMode();
        void DisableFaceSelectionMode();
        void UpdateSelectionStatus();

        QtOccView* m_viewer;

        // UI components
        QLabel* m_instructionLabel;
        QLabel* m_statusLabel;
        QPushButton* m_confirmButton;
        QPushButton* m_cancelButton;

        // Selection state
        TopoDS_Face m_selectedFace;
        bool m_isSelecting;

        // Timeout handling
        QTimer* m_selectionTimer;
        static const int SELECTION_TIMEOUT_MS = 30000; // 30-second timeout
    };

} // namespace cad_ui
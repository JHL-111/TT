#pragma once

#include "cad_core/OCAFDocument.h"
#include "cad_core/Shape.h"
#include <memory>
#include <string>
#include <vector>

namespace cad_core {

    class OCAFManager {
    public:
        OCAFManager();
        ~OCAFManager();

        // Initialise the manager
        bool Initialize();

        // Document operations
        bool NewDocument();
        bool OpenDocument(const std::string& filename);
        bool SaveDocument(const std::string& filename);

        // Shape operations
        bool AddShape(const ShapePtr& shape, const std::string& name = "");
        bool RemoveShape(const std::string& name);
        bool RemoveShape(const ShapePtr& shape);         // Remove by shape pointer
        bool ReplaceShape(const ShapePtr& oldShape, const ShapePtr& newShape);  // Replace a shape
        ShapePtr GetShape(const std::string& name) const;
        std::vector<std::string> GetAllShapeNames() const;
        std::vector<ShapePtr> GetAllShapes() const;

        // Undo/redo operations
        bool Undo();
        bool Redo();
        bool CanUndo() const;
        bool CanRedo() const;


        // Transaction operations
        void StartTransaction(const std::string& name = "Operation");
        void CommitTransaction();
        void AbortTransaction();

        // Get document
        std::shared_ptr<OCAFDocument> GetDocument() const { return m_document; }

    private:
        std::shared_ptr<OCAFDocument> m_document;
        bool m_isInitialized;

        // Helper methods
        TDF_Label FindShapeByName(const std::string& name) const;
        std::string GenerateUniqueName(const std::string& baseName) const;
    };

} // namespace cad_core
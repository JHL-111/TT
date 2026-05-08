#include "cad_core/OCAFManager.h"
#include <sstream>
#include <algorithm>


namespace cad_core {

    OCAFManager::OCAFManager() : m_isInitialized(false) {
        m_document = std::make_shared<OCAFDocument>();
    }

    OCAFManager::~OCAFManager() {
        if (m_document) {
            m_document->AbortTransaction();
        }
    }

    bool OCAFManager::Initialize() {
        if (m_isInitialized) {
            return true;
        }

        if (!m_document->Initialize()) {
            return false;
        }

        m_isInitialized = true;
        return true;
    }

    bool OCAFManager::NewDocument() {
        if (!m_document) {
            return false;
        }

        return m_document->NewDocument();
    }

    bool OCAFManager::OpenDocument(const std::string& filename) {
        if (!m_document) {
            return false;
        }

        return m_document->OpenDocument(filename);
    }

    bool OCAFManager::SaveDocument(const std::string& filename) {
        if (!m_document) {
            return false;
        }

        return m_document->SaveDocument(filename);
    }

    bool OCAFManager::AddShape(const ShapePtr& shape, const std::string& name) {
        if (!m_document || !shape) {
            return false;
        }

        std::string uniqueName = name.empty() ? GenerateUniqueName("Shape") : name;

        // Check whether the name already exists
        if (!FindShapeByName(uniqueName).IsNull()) {
            uniqueName = GenerateUniqueName(uniqueName);
        }

        TDF_Label label = m_document->AddShape(shape, uniqueName);
        return !label.IsNull();
    }

    bool OCAFManager::RemoveShape(const std::string& name) {
        if (!m_document || name.empty()) {
            return false;
        }

        TDF_Label label = FindShapeByName(name);
        if (label.IsNull()) {
            return false;
        }

        return m_document->RemoveShape(label);
    }

    bool OCAFManager::RemoveShape(const ShapePtr& shape) {
        if (!m_document || !shape) {
            return false;
        }

        // Find the label corresponding to this shape
        std::vector<TDF_Label> labels = m_document->GetAllShapes();
        for (const auto& label : labels) {
            ShapePtr labelShape = m_document->GetShape(label);
            if (labelShape && labelShape->GetOCCTShape().IsSame(shape->GetOCCTShape())) {
                return m_document->RemoveShape(label);
            }
        }

        return false; // Shape not found
    }

    bool OCAFManager::ReplaceShape(const ShapePtr& oldShape, const ShapePtr& newShape) {
        if (!m_document || !oldShape || !newShape) {
            return false;
        }

        // Find the label corresponding to the old shape
        std::vector<TDF_Label> labels = m_document->GetAllShapes();
        for (const auto& label : labels) {
            ShapePtr labelShape = m_document->GetShape(label);
            if (labelShape && labelShape->GetOCCTShape().IsSame(oldShape->GetOCCTShape())) {
                // Preserve the original name
                std::string name = m_document->GetName(label);

                // Remove the old shape
                if (m_document->RemoveShape(label)) {
                    // Add the new shape under the same name
                    TDF_Label newLabel = m_document->AddShape(newShape, name);
                    return !newLabel.IsNull();
                }
                return false;
            }
        }

        return false; // Old shape not found
    }

    ShapePtr OCAFManager::GetShape(const std::string& name) const {
        if (!m_document || name.empty()) {
            return nullptr;
        }

        TDF_Label label = FindShapeByName(name);
        if (label.IsNull()) {
            return nullptr;
        }

        return m_document->GetShape(label);
    }

    std::vector<std::string> OCAFManager::GetAllShapeNames() const {
        std::vector<std::string> names;

        if (!m_document) {
            return names;
        }

        std::vector<TDF_Label> labels = m_document->GetAllShapes();
        for (const auto& label : labels) {
            std::string name = m_document->GetName(label);
            if (!name.empty()) {
                names.push_back(name);
            }
        }

        return names;
    }

    std::vector<ShapePtr> OCAFManager::GetAllShapes() const {
        std::vector<ShapePtr> shapes;

        if (!m_document) {
            return shapes;
        }

        std::vector<TDF_Label> labels = m_document->GetAllShapes();
        for (const auto& label : labels) {
            ShapePtr shape = m_document->GetShape(label);
            if (shape) {
                shapes.push_back(shape);
            }
        }

        return shapes;
    }

    bool OCAFManager::Undo() {
        if (!m_document) {
            return false;
        }

        return m_document->Undo();
    }

    bool OCAFManager::Redo() {
        if (!m_document) {
            return false;
        }

        return m_document->Redo();
    }

    bool OCAFManager::CanUndo() const {
        if (!m_document) {
            return false;
        }

        return m_document->CanUndo();
    }

    bool OCAFManager::CanRedo() const {
        if (!m_document) {
            return false;
        }

        return m_document->CanRedo();
    }

    void OCAFManager::StartTransaction(const std::string& name) {
        if (!m_document) {
            return;
        }

        m_document->StartTransaction(name);
    }

    void OCAFManager::CommitTransaction() {
        if (!m_document) {
            return;
        }

        m_document->CommitTransaction();
    }

    void OCAFManager::AbortTransaction() {
        if (!m_document) {
            return;
        }

        m_document->AbortTransaction();
    }

    TDF_Label OCAFManager::FindShapeByName(const std::string& name) const {
        if (!m_document || name.empty()) {
            return TDF_Label();
        }

        std::vector<TDF_Label> labels = m_document->GetAllShapes();
        for (const auto& label : labels) {
            if (m_document->GetName(label) == name) {
                return label;
            }
        }

        return TDF_Label();
    }

    std::string OCAFManager::GenerateUniqueName(const std::string& baseName) const {
        std::vector<std::string> existingNames = GetAllShapeNames();

        // If the base name does not exist, use it as-is
        if (std::find(existingNames.begin(), existingNames.end(), baseName) == existingNames.end()) {
            return baseName;
        }

        // Otherwise, append an incrementing number
        int counter = 1;
        std::string uniqueName;
        do {
            std::stringstream ss;
            ss << baseName << "_" << counter;
            uniqueName = ss.str();
            counter++;
        } while (std::find(existingNames.begin(), existingNames.end(), uniqueName) != existingNames.end());

        return uniqueName;
    }

} // namespace cad_core
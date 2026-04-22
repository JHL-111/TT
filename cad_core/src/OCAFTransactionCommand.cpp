#include "cad_core/OCAFTransactionCommand.h"
#include "cad_core/OCAFManager.h"

namespace cad_core {

    OCAFTransactionCommand::OCAFTransactionCommand(OCAFManager* ocaf, const std::string& name)
        : m_ocaf(ocaf), m_name(name), m_executed(false) {
    }

    bool OCAFTransactionCommand::Execute() {
        m_executed = true;
        return true;
    }

    bool OCAFTransactionCommand::Undo() {
        if (!m_executed) return false;
        m_ocaf->Undo();
        m_executed = false;
        return true;
    }

    bool OCAFTransactionCommand::Redo() {
        if (m_executed) return true;
        m_ocaf->Redo();
        m_executed = true;
        return true;
    }

    const char* OCAFTransactionCommand::GetName() const {
        return m_name.c_str();
    }

} // namespace cad_core
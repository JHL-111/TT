#pragma once

#include "cad_core/ICommand.h"
#include <string>

namespace cad_core {

    class OCAFManager;

    class OCAFTransactionCommand : public ICommand {
    public:
        OCAFTransactionCommand(OCAFManager* ocaf, const std::string& name);
        ~OCAFTransactionCommand() override = default;

        bool Execute() override;
        bool Undo() override;
        bool Redo() override;
        const char* GetName() const override;

    private:
        OCAFManager* m_ocaf;
        std::string m_name;
        bool m_executed;
    };

} // namespace cad_core#pragma once

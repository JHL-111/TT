/**
 * @file ICommand.h
 * @brief Command interface for CAD operations
 * 
 */

#pragma once

#include <memory>

namespace cad_core {

/**
 * @class ICommand
 * @brief Command interface
 * 
 * Basic command interface for executing operations in the CAD application.
 * - Execute
 * - Undo
 * - Redo
 * - GetName
 * 

 */
class ICommand {
public:
    
    virtual ~ICommand() = default;
    
    virtual bool Execute() = 0;
    
    virtual bool Undo() = 0;
    
    virtual bool Redo() = 0;
    
    virtual const char* GetName() const = 0;
};


using CommandPtr = std::shared_ptr<ICommand>;

} // namespace cad_core
#pragma once
#include <string>

namespace Weaver {

    class IEditorCommand {
    public:
        virtual ~IEditorCommand() = default;
        virtual void        Execute()            = 0;
        virtual void        Undo()               = 0;
        virtual std::string GetDescription() const = 0;
    };

} // namespace Weaver

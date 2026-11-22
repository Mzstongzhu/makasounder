#pragma once

#include <string>

namespace mk {

struct AssociationStatus {
    bool registered = false;
    bool floatMode = false;
    std::wstring command;
};

AssociationStatus QueryAssociation();
bool RegisterAssociation(bool doubleClickFloat, std::wstring& error);
bool RemoveAssociation(std::wstring& error);

}  // namespace mk

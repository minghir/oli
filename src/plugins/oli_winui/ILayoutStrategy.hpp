#pragma once
#include "vControl.hpp" 
#include <map>
#include <memory>

// Definim tipul complex pe care îl primești din getChildren()
//using ChildrenMap = const std::map<std::string, std::unique_ptr<vControl>>;

class vContainer;

class ILayoutStrategy {
public:
    virtual ~ILayoutStrategy() = default;
    virtual void applyLayout(vContainer& container) = 0;
};

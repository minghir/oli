#ifndef IXLAYOUT_STRATEGY_HPP
#define IXLAYOUT_STRATEGY_HPP

#pragma once

class xContainer; // Forward declaration

class IXLayoutStrategy {
public:
    virtual ~IXLayoutStrategy() = default;
    virtual void applyLayout(xContainer& container) = 0;
};

// Aliasing în caz că ai fișiere care caută vechiul ILayoutStrategy
using ILayoutStrategy = IXLayoutStrategy;

#endif // IXLAYOUT_STRATEGY_HPP
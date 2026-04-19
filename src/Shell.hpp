#ifndef SHELL_HPP
#define SHELL_HPP

#include "IShellEngine.hpp"


class Shell {
protected:
    IShellEngine& m_engine; // Shell-ul folosește engine-ul
    bool m_running;

public:
    Shell(IShellEngine& engine);
    virtual void run();

};

#endif
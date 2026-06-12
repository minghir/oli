#include "EventDispatcher.hpp"
#include "../../ConsoleManager.hpp"
#include "../../StringUtils.hpp"

// -----------------------------------------------------------------------------
// 1. GESTIUNEA COMPATIBILITĂȚII PENTRU APELURILE VECHI (WinAPI Masking)
// -----------------------------------------------------------------------------

void EventDispatcher::registerHandler(const std::string& id, unsigned int message, EventCallback callback) {
    m_handlers[id][message] = std::move(callback);
}

bool EventDispatcher::dispatch(const std::string& id, unsigned int message) {
    auto it = m_handlers.find(id);
    if (it != m_handlers.end()) {
        auto mit = it->second.find(message);
        if (mit != it->second.end()) {
            LOG_DEBUG(L"[EventDispatcher] Handler compatibilitate executat pentru: " + str_to_wstr(id) + L" (MSG: " + std::to_wstring(message) + L")");
            if (mit->second) {
                mit->second();
                return true;
            }
        }
    }
    return false;
}

bool EventDispatcher::emit(const std::string& id) {
    // 273 este valoarea nativă pentru WM_COMMAND în Windows. O păstrăm fixă ca fallback în engine.
    return dispatch(id, 273); 
}

// -----------------------------------------------------------------------------
// 2. EVENIMENTE GENERICE STRUCTURALE (Utilizate intens de GTK / oli_xui)
// -----------------------------------------------------------------------------

void EventDispatcher::registerHandler(const std::string& eventName, const std::string& controlId, EventCallback callback) {
    m_controlSpecificHandlers[eventName][controlId] = std::move(callback);
}

void EventDispatcher::registerHandler(const std::string& eventName, EventCallback callback) {
    m_globalHandlers[eventName].push_back(std::move(callback));
}

bool EventDispatcher::dispatch(const std::string& eventName, const std::string& controlId) {
    LOG_DEBUG(L"[EventDispatcher] Dispatch: " + str_to_wstr(eventName) + L" | ID: " + str_to_wstr(controlId));

    // 1. Încercăm mai întâi o rutare specifică către widget-ul care a emis semnalul
    if (!controlId.empty()) {
        auto itEvent = m_controlSpecificHandlers.find(eventName);
        if (itEvent != m_controlSpecificHandlers.end()) {
            auto itControl = itEvent->second.find(controlId);
            if (itControl != itEvent->second.end()) {
                LOG_DEBUG(L"   [OK] Handler SPECIFIC GTK executat.");
                if (itControl->second) {
                    itControl->second();
                    return true;
                }
            }
        }
    }

    // 2. Dacă nu există un handler dedicat, trimitem către ascultătorii globali din script
    auto itGlobal = m_globalHandlers.find(eventName);
    if (itGlobal != m_globalHandlers.end()) {
        LOG_DEBUG(L"   [Global] Execut handlere globale pentru semnal.");
        for (const auto& callback : itGlobal->second) {
            if (callback) callback();
        }
        return true;
    }

    LOG_ERROR(L"   [FAIL] Niciun handler găsit pentru semnalul GTK: " + str_to_wstr(eventName));
    return false;
}

// -----------------------------------------------------------------------------
// 3. EVENIMENTE COMPLEXE CU TRANSMISIE DE ARGUMENTE STRING
// -----------------------------------------------------------------------------

void EventDispatcher::registerHandler(const std::string& eventName, const std::string& controlId, EventCallbackWithArg callback) {
    m_controlSpecificHandlersWithArg[eventName][controlId] = callback;
}

bool EventDispatcher::dispatch(const std::string& eventName, const std::string& controlId, const std::string& argument) {
    LOG_DEBUG(L"[EventDispatcher] Dispatch Arg: " + str_to_wstr(eventName) + L" pe controlul: " + str_to_wstr(controlId));
    
    auto eventIt = m_controlSpecificHandlersWithArg.find(eventName);
    if (eventIt != m_controlSpecificHandlersWithArg.end()) {
        const auto& controlMap = eventIt->second;
        auto controlIt = controlMap.find(controlId);

        if (controlIt != controlMap.end()) {
            LOG_DEBUG(L"   -> Execut handler specific cu argument: " + str_to_wstr(argument));
            if (controlIt->second) {
                controlIt->second(argument);
                return true;
            }
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// 4. MANAGEMENT DINAMIC (Redenumiri și Eliberări la închiderea tab-urilor)
// -----------------------------------------------------------------------------

bool EventDispatcher::renameControlHandlers(const std::string& oldId, const std::string& newId) {
    if (oldId == newId) return true;
    bool foundAny = false;

    // Redenumire în registrul de mesaje brute
    auto it1 = m_handlers.find(oldId);
    if (it1 != m_handlers.end()) {
        m_handlers[newId] = std::move(it1->second);
        m_handlers.erase(it1);
        foundAny = true;
    }

    // Redenumire în registrul de semnale simple
    for (auto& eventMap : m_controlSpecificHandlers) {
        auto it2 = eventMap.second.find(oldId);
        if (it2 != eventMap.second.end()) {
            eventMap.second[newId] = std::move(it2->second);
            eventMap.second.erase(it2);
            foundAny = true;
        }
    }

    // Redenumire în registrul de semnale cu argumente
    for (auto& eventMapArg : m_controlSpecificHandlersWithArg) {
        auto it3 = eventMapArg.second.find(oldId);
        if (it3 != eventMapArg.second.end()) {
            eventMapArg.second[newId] = std::move(it3->second);
            eventMapArg.second.erase(it3);
            foundAny = true;
        }
    }

    return foundAny;
}

bool EventDispatcher::removeHandlers(const std::string& controlId) {
    if (controlId.empty()) return false;
    bool foundAny = false;

    if (m_handlers.erase(controlId) > 0) foundAny = true;

    for (auto& eventMap : m_controlSpecificHandlers) {
        if (eventMap.second.erase(controlId) > 0) foundAny = true;
    }

    for (auto& eventMapArg : m_controlSpecificHandlersWithArg) {
        if (eventMapArg.second.erase(controlId) > 0) foundAny = true;
    }

    return foundAny;
}

bool EventDispatcher::removeHandler(const std::string& eventName, const std::string& controlId) {
    auto it = m_controlSpecificHandlers.find(eventName);
    if (it != m_controlSpecificHandlers.end()) {
        if (it->second.erase(controlId) > 0) return true;
    }

    auto itArg = m_controlSpecificHandlersWithArg.find(eventName);
    if (itArg != m_controlSpecificHandlersWithArg.end()) {
        if (itArg->second.erase(controlId) > 0) return true;
    }

    return false;
}
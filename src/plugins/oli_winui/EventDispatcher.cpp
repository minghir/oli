#include "EventDispatcher.hpp"
#include "../../ConsoleManager.hpp" // Esențial pentru logare
#include "stringUtils.hpp"

// -----------------------------------------------------------------------------
// Implementări pentru evenimente legate de ID-uri de control și mesaje WinAPI specifice
// (Utilizat de obicei pentru evenimente generate de controale către părinții lor, ex: WM_COMMAND)
// -----------------------------------------------------------------------------


void EventDispatcher::registerHandler(const std::string& id, UINT message, EventCallback callback) {

	//ConsoleManager::getInstance().log(L"[EventDispatcher::registerHandler(ID, MSG)] Înregistrez handler pentru evenimentul ID-Mesaj: '" + std::wstring(id.begin(), id.end()) + L"', Mesaj: " + std::to_wstring(message));

	//ConsoleManager::getInstance().log(L"[EventDispatcher::registerHandler(ID, MSG)] Adresa instanței EventDispatcher: " + std::to_wstring(reinterpret_cast<uintptr_t>(this)));



	// Această implementare va suprascrie un handler existent pentru aceeași pereche (id, message).

	m_handlers[id][message] = std::move(callback);



	// Verificare pentru logare, pentru a confirma adăugarea.

	if (m_handlers.count(id) && m_handlers[id].count(message)) {

		//ConsoleManager::getInstance().log(L"[EventDispatcher::registerHandler(ID, MSG)] Handler adăugat cu succes pentru ID '" + std::wstring(id.begin(), id.end()) + L"', Mesaj " + std::to_wstring(message) + L".");

	}

	else {

		//ConsoleManager::getInstance().log(L"[EventDispatcher::registerHandler(ID, MSG)] EROARE: Handler-ul pentru ID '" + std::wstring(id.begin(), id.end()) + L"', Mesaj " + std::to_wstring(message) + L" NU a apărut în m_handlers după înregistrare.");

	}

}


/*
bool EventDispatcher::dispatch(const std::string& id, UINT message) {

//	ConsoleManager::getInstance().log(L"[EventDispatcher::dispatch(ID, MSG)] Încerc să declanșez evenimentul ID-Mesaj: '" + std::wstring(id.begin(), id.end()) + L"', Mesaj: " + std::to_wstring(message));

//	ConsoleManager::getInstance().log(L"[EventDispatcher::dispatch(ID, MSG)] Adresa instanței EventDispatcher: " + std::to_wstring(reinterpret_cast<uintptr_t>(this)));



	// Caută ID-ul controlului în harta principală.

	auto it = m_handlers.find(id);

	if (it != m_handlers.end()) {

		// Dacă ID-ul este găsit, caută mesajul specific în sub-hartă.

		auto mit = it->second.find(message);
		if (mit != it->second.end()) {
			//ConsoleManager::getInstance().log(L"[EventDispatcher::dispatch(ID, MSG)] Handler găsit pentru ID '" + std::wstring(id.begin(), id.end()) + L"', Mesaj " + std::to_wstring(message) + L". Execut callback-ul.");
			mit->second(); // Execută funcția callback asociată.
			return true;
		}

	}

	//ConsoleManager::getInstance().log(L"[EventDispatcher::dispatch(ID, MSG)] Niciun handler găsit pentru ID '" + std::wstring(id.begin(), id.end()) + L"', Mesaj " + std::to_wstring(message) + L".");

	return false; // Niciun handler găsit pentru perechea (ID, Mesaj).

}

*/

bool EventDispatcher::dispatch(const std::string& id, UINT message) {
//	LOG_DEBUG(L"[EventDispatcher] Dispatch WinAPI - ID: " + str_to_wstr(id) + L", MSG: " + std::to_wstring(message));

	// 1. Căutăm ID-ul controlului în harta m_handlers
	auto it = m_handlers.find(id);
	if (it != m_handlers.end()) {

		// 2. Căutăm mesajul specific (ex: WM_COMMAND = 273)
		auto mit = it->second.find(message);
		if (mit != it->second.end()) {
			LOG_DEBUG(L"   [OK] Handler WinAPI găsit pentru controlul '" + str_to_wstr(id) + L"'. Execut...");

			if (mit->second) {
				mit->second(); // Execută callback-ul
				return true;
			}
			else {
				LOG_ERROR(L"   [!] Callback-ul găsit este NULL pentru ID: " + str_to_wstr(id));
			}
		}
		else {
			LOG_WARNING(L"   [-] Controlul '" + str_to_wstr(id) + L"' este înregistrat, dar nu are handler pentru mesajul " + std::to_wstring(message));
		}
	}
	else {
	//	LOG_DEBUG(L"   [-] Nu există niciun handler înregistrat în m_handlers pentru ID-ul: " + str_to_wstr(id));
	}

	return false;
}

bool EventDispatcher::emit(const std::string& id) {

	// Această metodă este o conveniență pentru a declanșa un eveniment WM_COMMAND

	// asociat cu un anumit ID de control.

	//ConsoleManager::getInstance().log(L"[EventDispatcher::emit] Apel WM_COMMAND pentru ID: " + std::wstring(id.begin(), id.end()));

	return dispatch(id, WM_COMMAND);

}



// -----------------------------------------------------------------------------

// Implementări pentru evenimente generice, bazate doar pe numele (șirul de caractere)

// (Utilizat de obicei pentru evenimente interne ale unui control, ex: "click" pe vButton)

// -----------------------------------------------------------------------------


/*
void EventDispatcher::registerHandler(const std::string& eventName, EventCallback callback) {

	ConsoleManager::getInstance().log(L"[EventDispatcher::registerHandler(Generic)] Înregistrez handler pentru evenimentul generic: '" + std::wstring(eventName.begin(), eventName.end()) + L"'.");

	ConsoleManager::getInstance().log(L"[EventDispatcher::registerHandler(Generic)] Adresa instanței EventDispatcher: " + std::to_wstring(reinterpret_cast<uintptr_t>(this)));



	// Adaugă noul handler la vectorul de handleri pentru acest 'eventName'.

	// Aceasta permite înregistrarea mai multor handleri pentru același eveniment generic.

	m_genericHandlers[eventName].push_back(std::move(callback));



	// Verificare pentru logare, pentru a confirma adăugarea.

	if (m_genericHandlers.count(eventName)) {

		ConsoleManager::getInstance().log(L"[EventDispatcher::registerHandler(Generic)] Evenimentul '" + std::wstring(eventName.begin(), eventName.end()) + L"' are acum " + std::to_wstring(m_genericHandlers[eventName].size()) + L" handler(i).");

	}

	else {

		ConsoleManager::getInstance().log(L"[EventDispatcher::registerHandler(Generic)] EROARE: Evenimentul '" + std::wstring(eventName.begin(), eventName.end()) + L"' NU a apărut în m_genericHandlers după înregistrare.");

	}

}
*/
// Implementare pentru registerHandler(Nume Eveniment, Callback)
void EventDispatcher::registerHandler(const std::string& eventName, EventCallback callback) {
	//ConsoleManager::getInstance().log(L"[EventDispatcher::registerHandler(Generic Global)] Înregistrez handler global pentru '" +
		//std::wstring(eventName.begin(), eventName.end()) + L"'.");

	m_globalHandlers[eventName].push_back(std::move(callback));
}
/*
// Implementare pentru dispatch(Nume Eveniment, ID Control)
bool EventDispatcher::dispatch(const std::string & eventName, const std::string & controlId) {
	ConsoleManager::getInstance().log(L"[EventDispatcher::dispatch(Generic, ID)] Încerc să declanșez '" +
		std::wstring(eventName.begin(), eventName.end()) + L"' pentru controlul '" +
		std::wstring(controlId.begin(), controlId.end()) + L"'.");

	auto itEvent = m_controlSpecificHandlers.find(eventName);
	if (itEvent != m_controlSpecificHandlers.end()) {
		auto itControl = itEvent->second.find(controlId);
		if (itControl != itEvent->second.end()) {
			itControl->second(); // Execută callback-ul
			return true;
		}
	}
	return false;
}
*/

/*
bool EventDispatcher::dispatch(const std::string& eventName) {

	ConsoleManager::getInstance().log(L"[EventDispatcher::dispatch(Generic)] Încerc să declanșez evenimentul generic: '" + std::wstring(eventName.begin(), eventName.end()) + L"'.");

	ConsoleManager::getInstance().log(L"[EventDispatcher::dispatch(Generic)] Adresa instanței EventDispatcher: " + std::to_wstring(reinterpret_cast<uintptr_t>(this)));



	// Caută numele evenimentului în harta handlerilor generici.

	auto it = m_genericHandlers.find(eventName);

	if (it != m_genericHandlers.end()) {

		ConsoleManager::getInstance().log(L"[EventDispatcher::dispatch(Generic)] Handler(i) găsit(i) pentru evenimentul '" + std::wstring(eventName.begin(), eventName.end()) + L"'. Execut " + std::to_wstring(it->second.size()) + L" callback(uri)...");

		// Iterăm prin toți handlerii înregistrați pentru acest 'eventName' și îi executăm.

		for (const auto& callback : it->second) {

			if (callback) { // Asigură-te că callback-ul nu este null.

				callback();

			}

			else {

				ConsoleManager::getInstance().log(L"[EventDispatcher::dispatch(Generic)] AVERTISMENT: S-a găsit un callback null pentru evenimentul '" + std::wstring(eventName.begin(), eventName.end()) + L"'.");

			}

		}

		return true; // Cel puțin un handler a fost găsit (chiar dacă a fost null).

	}

	ConsoleManager::getInstance().log(L"[EventDispatcher::dispatch(Generic)] NICIUN HANDLER GĂSIT pentru evenimentul generic '" + std::wstring(eventName.begin(), eventName.end()) + L"'.");

	return false; // Niciun handler găsit pentru acest 'eventName'.
	
}
*/

// Implementare pentru registerHandler(Nume Eveniment, ID Control, Callback)
void EventDispatcher::registerHandler(const std::string& eventName, const std::string& controlId, EventCallback callback) {
//	ConsoleManager::getInstance().log(L"[EventDispatcher::registerHandler(Generic, ID)] Înregistrez handler pentru '" +
//		std::wstring(eventName.begin(), eventName.end()) + L"' pentru controlul '" +
//		std::wstring(controlId.begin(), controlId.end()) + L"'.");

	// Această implementare va suprascrie un handler existent pentru aceeași pereche.
	m_controlSpecificHandlers[eventName][controlId] = std::move(callback);
}

/*
// Implementare pentru dispatch(Nume Eveniment)
bool EventDispatcher::dispatch(const std::string& eventName) {
	ConsoleManager::getInstance().log(L"[EventDispatcher::dispatch(Generic Global)] Încerc să declanșez '" +
		std::wstring(eventName.begin(), eventName.end()) + L"' la nivel global.");

	auto it = m_globalHandlers.find(eventName);
	if (it != m_globalHandlers.end()) {
		for (const auto& callback : it->second) {
			if (callback) {
				callback();
			}
		}
		return true;
	}
	return false;
}
*/

// Implement the single, consolidated dispatch method
/*
bool EventDispatcher::dispatch(const std::string& eventName, const std::string& controlId) {
	LOG_DEBUG(L"[EventDispatcher] Dispatch simplu: " + str_to_wstr(eventName) + L" pentru ID: " + str_to_wstr(controlId));
	// If controlId is not empty, look for a control-specific handler
	if (!controlId.empty()) {
		//ConsoleManager::getInstance().log(L"[EventDispatcher::dispatch] Attempting to dispatch '" +
			//std::wstring(eventName.begin(), eventName.end()) + L"' for control '" +
			//std::wstring(controlId.begin(), controlId.end()) + L"'.");

		auto itEvent = m_controlSpecificHandlers.find(eventName);
		if (itEvent != m_controlSpecificHandlers.end()) {
			auto itControl = itEvent->second.find(controlId);
			if (itControl != itEvent->second.end()) {
				LOG_DEBUG(L"   [OK] Handler SPECIFIC găsit și executat.");
				itControl->second(); // Execute the callback
				return true;
			}
			else {
				LOG_WARNING(L"   [!] Evenimentul '" + str_to_wstr(eventName) + L"' există, dar NU și pentru ID-ul '" + str_to_wstr(controlId) + L"'.");
			}
		}
		else {
			LOG_DEBUG(L"   [-] Nu există nicio înregistrare pentru evenimentul generic: " + str_to_wstr(eventName));
		}
	}

	// If no controlId or no control-specific handler was found, look for global handlers
	//ConsoleManager::getInstance().log(L"[EventDispatcher::dispatch] Attempting to dispatch global '" +
		//std::wstring(eventName.begin(), eventName.end()) + L"'.");
	auto it = m_globalHandlers.find(eventName);
	if (it != m_globalHandlers.end()) {
		LOG_DEBUG(L"   [Global] Execut handlere globale pentru: " + str_to_wstr(eventName));
		for (const auto& callback : it->second) {
			if (callback) {
				callback();
			}
		}
		return true;
	}

	//ConsoleManager::getInstance().log(L"[EventDispatcher::dispatch] No handler found for '" +
		//std::wstring(eventName.begin(), eventName.end()) + L"' or its control '" +
		//std::wstring(controlId.begin(), controlId.end()) + L"'.");
	LOG_ERROR(L"   [FAIL] Niciun handler (specific sau global) găsit pentru: " + str_to_wstr(eventName));
	
	return false;
}
*/

bool EventDispatcher::dispatch(const std::string& eventName, const std::string& controlId) {
	LOG_DEBUG(L"[EventDispatcher] Dispatch: " + str_to_wstr(eventName) + L" | ID: " + str_to_wstr(controlId));

	// 1. Încercăm Handler SPECIFIC
	if (!controlId.empty()) {
		auto itEvent = m_controlSpecificHandlers.find(eventName);
		if (itEvent != m_controlSpecificHandlers.end()) {
			auto itControl = itEvent->second.find(controlId);
			if (itControl != itEvent->second.end()) {
				LOG_DEBUG(L"   [OK] Handler SPECIFIC executat.");
				itControl->second();
				return true;
			}
		}
	}

	// 2. DACĂ NU S-A GĂSIT SPECIFIC, Încercăm GLOBAL (fără să dăm eroare încă)
	auto itGlobal = m_globalHandlers.find(eventName);
	if (itGlobal != m_globalHandlers.end()) {
		LOG_DEBUG(L"   [Global] Execut handlere globale.");
		for (const auto& callback : itGlobal->second) {
			if (callback) callback();
		}
		return true;
	}

	LOG_ERROR(L"   [FAIL] Niciun handler găsit pentru: " + str_to_wstr(eventName));
	return false;
}


void EventDispatcher::registerHandler(const std::string& eventName, const std::string& controlId, EventCallbackWithArg callback) {
	// Verifică dacă există deja o hartă pentru acest nume de eveniment.
	// Dacă nu există, o creează (utilizând []).
	auto& controlMap = m_controlSpecificHandlersWithArg[eventName];

	// Adaugă sau suprascrie handlerul pentru ID-ul specific al controlului.
	controlMap[controlId] = callback;

	// Logarea internă (opțional):
	// ConsoleManager::getInstance().log(L"[EventDispatcher] Handler înregistrat cu argument pentru evenimentul: " + str_to_wstr(eventName) + L" pe controlul: " + str_to_wstr(controlId));
}


// EventDispatcher.cpp

bool EventDispatcher::dispatch(const std::string& eventName, const std::string& controlId, const std::string& argument) {
	LOG_DEBUG(L"Dispatch: " + str_to_wstr(eventName) + L" for " + str_to_wstr(controlId));
	// 1. Caută evenimentul generic (ex: "grid_column_click") în prima hartă.
	auto eventIt = m_controlSpecificHandlersWithArg.find(eventName);
	if (eventIt == m_controlSpecificHandlersWithArg.end()) {
		// Nu există handlere înregistrate pentru acest nume de eveniment.
		LOG_DEBUG(L"Nu există handlere înregistrate pentru acest nume de eveniment.");
		return false;
	}

	// 2. Caută ID-ul controlului (ex: "mainGrid") în harta interioară.
	const auto& controlMap = eventIt->second;
	auto controlIt = controlMap.find(controlId);

	if (controlIt == controlMap.end()) {
		// Nu există un handler specific pentru acest ID de control.
		LOG_DEBUG(L"Nu există un handler specific pentru acest ID de control.");
		return false;
	}

	// 3. Execută handlerul, transmițându-i argumentul.
	LOG_DEBUG(L"-> Handler SPECIFIC găsit!");
	controlIt->second(argument);

	// Logarea internă (opțional):
	// ConsoleManager::getInstance().log(L"[EventDispatcher] Eveniment dispecerizat cu argumentul: " + str_to_wstr(eventName) + L" pe controlul: " + str_to_wstr(controlId) + L", argument: " + str_to_wstr(argument));

	return true;
}


bool EventDispatcher::renameControlHandlers(const std::string& oldId, const std::string& newId) {
	if (oldId == newId) return true; // Tehnic, e un succes deoarece starea dorită e atinsă

	bool foundAny = false;

	// 1. Mutăm din m_handlers (WinAPI messages)
	auto it1 = m_handlers.find(oldId);
	if (it1 != m_handlers.end()) {
		m_handlers[newId] = std::move(it1->second);
		m_handlers.erase(it1);
		foundAny = true;
	}

	// 2. Mutăm din m_controlSpecificHandlers (Generic events)
	for (auto& eventMap : m_controlSpecificHandlers) {
		auto it2 = eventMap.second.find(oldId);
		if (it2 != eventMap.second.end()) {
			eventMap.second[newId] = std::move(it2->second);
			eventMap.second.erase(it2);
			foundAny = true;
		}
	}

	// 3. Mutăm din m_controlSpecificHandlersWithArg (Events with string arg)
	for (auto& eventMapArg : m_controlSpecificHandlersWithArg) {
		auto it3 = eventMapArg.second.find(oldId);
		if (it3 != eventMapArg.second.end()) {
			eventMapArg.second[newId] = std::move(it3->second);
			eventMapArg.second.erase(it3);
			foundAny = true;
		}
	}

	if (foundAny) {
		//ConsoleManager::getInstance().log(L"[EventDispatcher] Handlere mutate de la " +
			//str_to_wstr(oldId) + L" la " + str_to_wstr(newId));
	}

	return foundAny;
}

bool EventDispatcher::removeHandlers(const std::string& controlId) {
	if (controlId.empty()) return false;

	bool foundAny = false;

	// 1. Curățăm m_handlers (WinAPI messages)
	if (m_handlers.erase(controlId) > 0) {
		foundAny = true;
	}

	// 2. Curățăm m_controlSpecificHandlers (Generic events)
	for (auto& eventMap : m_controlSpecificHandlers) {
		if (eventMap.second.erase(controlId) > 0) {
			foundAny = true;
		}
	}

	// 3. Curățăm m_controlSpecificHandlersWithArg (Events with string arg)
	for (auto& eventMapArg : m_controlSpecificHandlersWithArg) {
		if (eventMapArg.second.erase(controlId) > 0) {
			foundAny = true;
		}
	}

	if (foundAny) {
		//ConsoleManager::getInstance().log(L"[EventDispatcher] Handlere eliminate pentru: " +
		//	str_to_wstr(controlId));
	}
	//LOG_DEBUG(L"Dispatcher: Eliminat handlere pentru " + str_to_wstr(controlId));
	return foundAny;
}


bool EventDispatcher::removeHandler(const std::string& eventName, const std::string& controlId) {
	// 1. Verificăm handlerele simple
	auto it = m_controlSpecificHandlers.find(eventName);
	if (it != m_controlSpecificHandlers.end()) {
		if (it->second.erase(controlId) > 0) {
		//	LOG_DEBUG(L"[EventDispatcher] Șters handler: " + str_to_wstr(eventName) +
		//		L" pentru control: " + str_to_wstr(controlId));
			return true;
		}
	}

	// 2. Verificăm handlerele cu argumente
	auto itArg = m_controlSpecificHandlersWithArg.find(eventName);
	if (itArg != m_controlSpecificHandlersWithArg.end()) {
		if (itArg->second.erase(controlId) > 0) {
		//	LOG_DEBUG(L"[EventDispatcher] Șters handler (arg): " + str_to_wstr(eventName) +
		//		L" pentru control: " + str_to_wstr(controlId));
			return true;
		}
	}

	return false;
}
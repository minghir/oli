#include "../../OliEngine.hpp"
#include <unordered_map>
#include <string>
#include <vector>
#include <variant>
#include <fstream>

#if defined(_WIN32) || defined(_WIN64)
#define OLI_EXPORT extern "C" __declspec(dllexport)
#else
#define OLI_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#ifndef G_LINKED_OLI_ENGINE_DEFINED
#define G_LINKED_OLI_ENGINE_DEFINED
inline vOliEngine* g_LinkedOliEngine = nullptr;
#endif

// --- BUFFER GLOBAL DE INSTRUCȚIUNI ---
static std::vector<unsigned char> g_dos_bytecode;

// Structură care reține starea unui While activ
struct WhileContext {
    int id;               // ID unicat (ex: 0, 1, 2...)
    size_t start_offset;  // Unde începe codul în buffer
    size_t jump_patch_pos;// Unde trebuie să completăm adresa pentru ieșire
};

// O stivă globală/statică în plugin
static std::vector<WhileContext> g_while_stack;
static int g_while_counter = 0;

// Variabilă globală temporară pentru mesajul setat din script
static std::string g_CurrentDosMessage = "HELLO FROM OLI";

static long long asInt(const vData& data) {
    if (std::holds_alternative<long long>(data.value))
        return std::get<long long>(data.value);
    if (std::holds_alternative<double>(data.value))
        return static_cast<long long>(std::get<double>(data.value));
    return 0;
}

static std::string asString(const vData& data) {
    if (std::holds_alternative<std::wstring>(data.value)) {
        std::wstring w = std::get<std::wstring>(data.value);
        return std::string(w.begin(), w.end());
    }
    return "";
}

// --- FUNCȚII HELPER PENTRU BUFFERUL DE BYTECODE ---
static size_t current_bytecode_size() {
    return g_dos_bytecode.size();
}

// Plasează un placeholder pentru un salt condiționat (returnează poziția offset-ului în buffer)
static size_t emit_conditional_jump_placeholder() {
    // Exemplu x86: JE rel16 (0x74 [disp8] sau 0x0F 0x84 [disp32])
    // Pentru simplificare în 16-biți, rezervăm spațiu pentru un salt scurt sau aproape
    g_dos_bytecode.push_back(0x74); // JE scurt
    g_dos_bytecode.push_back(0x00); // Placeholder offset
    return g_dos_bytecode.size() - 1;
}

static void emit_jump(size_t target_offset) {
    g_dos_bytecode.push_back(0xEB); // JMP scurt
    // Calculăm distanța relativă
    long long rel = (long long)target_offset - (long long)(g_dos_bytecode.size() + 1);
    g_dos_bytecode.push_back((unsigned char)(rel & 0xFF));
}

static void patch_jump_address(size_t patch_pos, size_t target_offset) {
    long long rel = (long long)target_offset - (long long)(patch_pos + 1);
    if (patch_pos < g_dos_bytecode.size()) {
        g_dos_bytecode[patch_pos] = (unsigned char)(rel & 0xFF);
    }
}

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

void RegisterDOSFunctions(PluginRegistry& registry) {

    // Resetează bufferul pentru un nou program
    registry[L"DOS_CLEAR"] = [](const std::vector<vData>& a) -> vData {
        g_dos_bytecode.clear();
        g_while_stack.clear();
        g_while_counter = 0;
        return vData{ true };
    };

    // Setează modul video VGA 13h (320x200, 256 culori)
    registry[L"DOS_INIT_MODE"] = [](const std::vector<vData>& a) -> vData {
        g_dos_bytecode.push_back(0xB4); g_dos_bytecode.push_back(0x00); // MOV AH, 00h
        g_dos_bytecode.push_back(0xB0); g_dos_bytecode.push_back(0x13); // MOV AL, 13h
        g_dos_bytecode.push_back(0xCD); g_dos_bytecode.push_back(0x10); // INT 10h
        return vData{ true };
    };

    // Revine la modul text (03h)
    registry[L"DOS_TEXT_MODE"] = [](const std::vector<vData>& a) -> vData {
        g_dos_bytecode.push_back(0xB4); g_dos_bytecode.push_back(0x00); // MOV AH, 00h
        g_dos_bytecode.push_back(0xB0); g_dos_bytecode.push_back(0x03); // MOV AL, 03h
        g_dos_bytecode.push_back(0xCD); g_dos_bytecode.push_back(0x10); // INT 10h
        return vData{ true };
    };

    // Așteaptă o tastă (INT 16h)
    registry[L"DOS_WAIT_KEY"] = [](const std::vector<vData>& a) -> vData {
        g_dos_bytecode.push_back(0xB4); g_dos_bytecode.push_back(0x00); // MOV AH, 00h
        g_dos_bytecode.push_back(0xCD); g_dos_bytecode.push_back(0x16); // INT 16h
        return vData{ true };
    };

    // DOS_BUILD("nume_fisier", "type") -> Împachetează tot ce e în buffer și scrie fișierul
    registry[L"DOS_BUILD"] = [](const std::vector<vData>& a) -> vData {
        if (a.size() < 2) return vData{ false };

        std::string filename = asString(a[0]);
        std::string type = asString(a[1]); // "com" sau "exe"

        if (filename.empty()) return vData{ false };

        // Adăugăm instrucțiunea de ieșire în DOS la sfârșitul codului (INT 21h, AH=4Ch)
        g_dos_bytecode.push_back(0xB4); g_dos_bytecode.push_back(0x4C); // MOV AH, 4Ch
        g_dos_bytecode.push_back(0xCD); g_dos_bytecode.push_back(0x21); // INT 21h

        std::vector<unsigned char> final_binary;

        if (type == "exe") {
            std::vector<unsigned char> mz_header = {
                0x4D, 0x5A, 0x20, 0x00, 0x02, 0x00, 0x00, 0x00,
                0x02, 0x00, 0x10, 0x00, 0xFF, 0xFF, 0x00, 0x00,
                0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
            };
            final_binary = mz_header;
            final_binary.resize(0x40, 0x00);
            final_binary.insert(final_binary.end(), g_dos_bytecode.begin(), g_dos_bytecode.end());
        } else {
            final_binary = g_dos_bytecode;
        }

        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) return vData{ false };

        file.write(reinterpret_cast<char*>(final_binary.data()), final_binary.size());
        file.close();

        return vData{ true };
    };

    // --- STRUCTURI DE CONTROL: WHILE ---
    registry[L"DOS_BEGIN_WHILE"] = [](const std::vector<vData>& a) -> vData {
        WhileContext ctx;
        ctx.id = g_while_counter++;
        ctx.start_offset = current_bytecode_size(); 
        ctx.jump_patch_pos = emit_conditional_jump_placeholder(); 

        g_while_stack.push_back(ctx);
        return vData{ true };
    };

    registry[L"DOS_END_WHILE"] = [](const std::vector<vData>& a) -> vData {
        if (g_while_stack.empty()) return vData{ false };

        WhileContext ctx = g_while_stack.back();
        g_while_stack.pop_back();

        emit_jump(ctx.start_offset);
        patch_jump_address(ctx.jump_patch_pos, current_bytecode_size());

        return vData{ true };
    };
	
	// Citește o tastă prin INT 16h și o afișează pe ecran cu INT 10h (Teletype)
    // Dacă tasta este ESC (27), sare direct la sfârșitul programului (sau sparge bucla)
    registry[L"DOS_ECHO_KEY_LOOP"] = [](const std::vector<vData>& a) -> vData {
        // Această funcție va emite opcodurile x86 pentru bucla interactivă:
        std::vector<unsigned char> key_loop_code = {
            0xB4, 0x00,          // MOV AH, 00h (Așteaptă tastă)
            0xCD, 0x16,          // INT 16h -> AL conține codul ASCII al tastei
            
            0x3C, 0x1B,          // CMP AL, 1B0h (Verifică dacă e tasta ESC - 27)
            0x74, 0x0A,          // JE +10 (Dacă e ESC, sare direct la ieșirea din buclă)

            0xB4, 0x0E,          // MOV AH, 0Eh (Funcție afișare caracter BIOS)
            0xCD, 0x10,          // INT 10h (Afișează caracterul din AL pe ecran)
            
            0xEB, 0xF0           // JMP înapoi la începutul buclei (-16 octeți)
        };

        g_dos_bytecode.insert(g_dos_bytecode.end(), key_loop_code.begin(), key_loop_code.end());
        return vData{ true };
    };
	
}

OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry, void* enginePtr) {
    if (g_LinkedOliEngine == nullptr) {
        g_LinkedOliEngine = static_cast<vOliEngine*>(enginePtr);
    }
    RegisterDOSFunctions(registry);
}
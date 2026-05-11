#include "../../OliEngine.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>

// --- INTEGRARE MINIAUDIO ---
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#ifdef _WIN32
#define OLI_EXPORT extern "C" __declspec(dllexport)
#else
#define OLI_EXPORT extern "C"
#endif

using PluginRegistry = std::unordered_map<std::wstring, OliFunctionHandler>;

// Structură pentru a gestiona starea sunetului în plugin
struct SoundState {
    ma_engine engine;
    bool isInitialized = false;
    std::unordered_map<int, ma_sound*> soundMap;
    int nextId = 1;
} g_Audio;

// Helper pentru conversia vData -> double (identic cu cel din Canvas)
inline double toDouble(const vData& v) {
    if (std::holds_alternative<double>(v.value)) return std::get<double>(v.value);
    if (std::holds_alternative<long long>(v.value)) return static_cast<double>(std::get<long long>(v.value));
    return 0.0;
}

// Helper pentru conversia wstring -> string (necesar pentru miniaudio/căi de fișiere)
std::string toUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

OLI_EXPORT void LoadOliPlugin(PluginRegistry& registry) {

    // --- SND_INIT() ---
    // Inițializează motorul audio de sistem
    registry[L"SND_INIT"] = [](const std::vector<vData>&) -> vData {
        if (g_Audio.isInitialized) return vData{ 1LL };

        if (ma_engine_init(NULL, &g_Audio.engine) != MA_SUCCESS) {
            return vData{ 0LL }; // Eșec la inițializare device audio
        }

        g_Audio.isInitialized = true;
        return vData{ 1LL };
        };

    // --- SND_LOAD(path) -> returns ID ---
    // Încarcă un fișier (wav, mp3, flac) și returnează un handle numeric
    registry[L"SND_LOAD"] = [](const std::vector<vData>& args) -> vData {
        if (!g_Audio.isInitialized || args.empty()) return vData{ 0LL };

        std::wstring wPath = std::get<std::wstring>(args[0].value);
        std::string path = toUtf8(wPath);

        ma_sound* newSound = new ma_sound();
        // MA_SOUND_FLAG_DECODE: sunetul este decodat în RAM (bun pentru sunete scurte, efecte)
        ma_result result = ma_sound_init_from_file(&g_Audio.engine, path.c_str(), 0, NULL, NULL, newSound);

        if (result != MA_SUCCESS) {
            delete newSound;
            return vData{ 0LL };
        }

        int id = g_Audio.nextId++;
        g_Audio.soundMap[id] = newSound;
        return vData{ (long long)id };
        };

    // --- SND_PLAY(id) ---
    registry[L"SND_PLAY"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData{ 0LL };
        int id = (int)toDouble(args[0]);

        if (g_Audio.soundMap.count(id)) {
            // Dacă sunetul s-a terminat, ma_sound_start îl reia de la început
            ma_sound_start(g_Audio.soundMap[id]);
            return vData{ 1LL };
        }
        return vData{ 0LL };
        };

    // --- SND_STOP(id) ---
    registry[L"SND_STOP"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData{ 0LL };
        int id = (int)toDouble(args[0]);

        if (g_Audio.soundMap.count(id)) {
            ma_sound_stop(g_Audio.soundMap[id]);
            ma_sound_seek_to_pcm_frame(g_Audio.soundMap[id], 0); // Resetăm la început
            return vData{ 1LL };
        }
        return vData{ 0LL };
        };

    // --- SND_SET_VOL(id, volume) ---
    // volum: 0.0 (mut) -> 1.0 (normal) -> + (amplificat)
    registry[L"SND_SET_VOL"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{ 0LL };
        int id = (int)toDouble(args[0]);
        float vol = (float)toDouble(args[1]);

        if (g_Audio.soundMap.count(id)) {
            ma_sound_set_volume(g_Audio.soundMap[id], vol);
            return vData{ 1LL };
        }
        return vData{ 0LL };
        };

    // --- SND_IS_PLAYING(id) ---
    registry[L"SND_IS_PLAYING"] = [](const std::vector<vData>& args) -> vData {
        if (args.empty()) return vData{ 0LL };
        int id = (int)toDouble(args[0]);

        if (g_Audio.soundMap.count(id)) {
            bool playing = ma_sound_is_playing(g_Audio.soundMap[id]);
            return vData{ playing ? 1LL : 0LL };
        }
        return vData{ 0LL };
        };

    // --- SND_CLOSE() ---
    // Curățenie generală (echivalentul lui CAN_CLOSE)
    registry[L"SND_CLOSE"] = [](const std::vector<vData>&) -> vData {
        if (!g_Audio.isInitialized) return vData{ 1LL };

        // Ștergem obiectele de sunet din memorie
        for (auto const& [id, soundPtr] : g_Audio.soundMap) {
            ma_sound_uninit(soundPtr);
            delete soundPtr;
        }
        g_Audio.soundMap.clear();

        // Închidem motorul audio
        ma_engine_uninit(&g_Audio.engine);
        g_Audio.isInitialized = false;

        return vData{ 1LL };
        };


    registry[L"SND_SAVE_WAV"] = [](const std::vector<vData>& args) -> vData {
        if (args.size() < 2) return vData{ 0LL };

        // 1. Calea fișierului
        std::wstring wPath = std::get<std::wstring>(args[0].getTrueData().value);
        std::string path = toUtf8(wPath);

        // 2. Extragem Array-ul (care este un shared_ptr în vData)
        const vData& actualArg = args[1].getTrueData();
        if (!actualArg.isArray()) {
            std::wcerr << L"[ERROR] SND_SAVE_WAV: Argumentul 2 trebuie sa fie un Array." << std::endl;
            return vData{ 0LL };
        }

        // Aici era eroarea: trebuie să cerem vDataArray (care e shared_ptr)
        vDataArray arrayPtr = std::get<vDataArray>(actualArg.value);
        if (!arrayPtr) return vData{ 0LL };

        // Referință către vectorul real din interiorul shared_ptr-ului
        const std::vector<vData>& oliArray = *arrayPtr;

        int sampleRate = (args.size() > 2) ? (int)args[2].toInt() : 44100;
        int numSamples = (int)oliArray.size();

        // --- SCRIERE FIȘIER (Restul logicii rămâne identică) ---
        std::ofstream f(path, std::ios::binary);
        if (!f.is_open()) return vData{ 0LL };

        auto write_str = [&](const char* s) { f.write(s, 4); };
        auto write_u32 = [&](uint32_t v) { f.write(reinterpret_cast<char*>(&v), 4); };
        auto write_u16 = [&](uint16_t v) { f.write(reinterpret_cast<char*>(&v), 2); };

        write_str("RIFF");
        write_u32(36 + numSamples * 2);
        write_str("WAVE");
        write_str("fmt ");
        write_u32(16); write_u16(1); write_u16(1);
        write_u32(sampleRate);
        write_u32(sampleRate * 2);
        write_u16(2); write_u16(16);
        write_str("data");
        write_u32(numSamples * 2);

        for (const auto& v : oliArray) {
            // Folosim funcția ta toDouble() care e deja definită în vData
            float sample = (float)v.toDouble();

            if (sample > 1.0f) sample = 1.0f;
            if (sample < -1.0f) sample = -1.0f;

            int16_t s16 = static_cast<int16_t>(sample * 32767.0f);
            f.write(reinterpret_cast<char*>(&s16), 2);
        }

        f.close();
        return vData{ 1LL };
        };
}
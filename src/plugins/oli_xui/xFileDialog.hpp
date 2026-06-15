#ifndef XFILEDIALOG_HPP
#define XFILEDIALOG_HPP

#pragma once
#include <gtk/gtk.h>
#include <string>
#include <locale>
#include <codecvt>

class xFileDialog {
private:
    std::wstring m_title;
    std::wstring m_filter;
    std::wstring m_resultPath;
    std::wstring m_initialPath;

    // Funcții utilitare interne pentru conversia șirurilor de caractere (WString <-> UTF-8)
    std::string wstr_to_utf8(const std::wstring& wstr) const {
        if (wstr.empty()) return "";
        try {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            return converter.to_bytes(wstr);
        } catch (...) {
            return std::string(wstr.begin(), wstr.end());
        }
    }

    std::wstring utf8_to_wstr(const std::string& str) const {
        if (str.empty()) return L"";
        try {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            return converter.from_bytes(str);
        } catch (...) {
            return std::wstring(str.begin(), str.end());
        }
    }

    // Rutină pentru aplicarea filtrelor standard (.oli și *.*) peste dialogul GTK
    void applyFilters(GtkWidget* dialog) const {
        // 1. Filtrul pentru fișiere Oli
        GtkFileFilter* oliFilter = gtk_file_filter_new();
        gtk_file_filter_set_name(oliFilter, "Oli Files (*.oli)");
        gtk_file_filter_add_pattern(oliFilter, "*.oli");
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), oliFilter);

        // 2. Filtrul universal All Files
        GtkFileFilter* allFilter = gtk_file_filter_new();
        gtk_file_filter_set_name(allFilter, "All Files (*.*)");
        gtk_file_filter_add_pattern(allFilter, "*");
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), allFilter);
    }

public:
    xFileDialog(const std::wstring& title = L"Select File")
        : m_title(title), m_filter(L"") {
    }

    void setFilter(const std::wstring& filter) { m_filter = filter; }
    void setInitialPath(const std::wstring& path) { m_initialPath = path; }

    // Deschidere fișier (Echivalentul GetOpenFileNameW)
    bool showOpen(GtkWidget* parent = nullptr) {
        std::string titleUtf8 = wstr_to_utf8(m_title);
        
        GtkWindow* gtkParentWin = parent ? GTK_WINDOW(parent) : nullptr;

        GtkWidget* dialog = gtk_file_chooser_dialog_new(
            titleUtf8.c_str(),
            gtkParentWin,
            GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Cancel", GTK_RESPONSE_CANCEL,
            "_Open", GTK_RESPONSE_ACCEPT,
            NULL
        );

        // Aplicăm filtrele de extensie
        applyFilters(dialog);

        // Setăm folderul curent de lucru dacă nu s-a cerut o cale inițială specifică
        if (!m_initialPath.empty()) {
            std::string initPathUtf8 = wstr_to_utf8(m_initialPath);
            gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), initPathUtf8.c_str());
        } else {
            char gCurrentDir[1024];
            if (getcwd(gCurrentDir, sizeof(gCurrentDir)) != NULL) {
                gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), gCurrentDir);
            }
        }

        bool success = false;
        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            if (filename) {
                m_resultPath = utf8_to_wstr(filename);
                g_free(filename);
                success = true;
            }
        }

        gtk_widget_destroy(dialog);
        return success;
    }

    // Salvare fișier (Echivalentul GetSaveFileNameW)
    bool showSave(GtkWidget* parent = nullptr) {
        std::string titleUtf8 = wstr_to_utf8(m_title);
        
        GtkWindow* gtkParentWin = parent ? GTK_WINDOW(parent) : nullptr;

        GtkWidget* dialog = gtk_file_chooser_dialog_new(
            titleUtf8.c_str(),
            gtkParentWin,
            GTK_FILE_CHOOSER_ACTION_SAVE,
            "_Cancel", GTK_RESPONSE_CANCEL,
            "_Save", GTK_RESPONSE_ACCEPT,
            NULL
        );

        // Forțăm confirmarea de suprascriere (Echivalentul flag-ului OFN_OVERWRITEPROMPT)
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

        applyFilters(dialog);

        // Managementul căii și numelui implicit sugerat la salvare
        if (!m_initialPath.empty()) {
            std::string initPathUtf8 = wstr_to_utf8(m_initialPath);
            
            // Verificăm dacă m_initialPath conține doar un nume simplu sau o cale completă
            if (initPathUtf8.find('/') == std::string::npos) {
                gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), initPathUtf8.c_str());
            } else {
                gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), initPathUtf8.c_str());
            }
        }

        bool success = false;
        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            if (filename) {
                std::string resStr = filename;
                
                // 🔥 Auto-adăugare extensie implicită '.oli' (Echivalentul lpstrDefExt din Windows)
                if (resStr.size() < 4 || resStr.substr(resStr.size() - 4) != ".oli") {
                    resStr += ".oli";
                }

                m_resultPath = utf8_to_wstr(resStr);
                g_free(filename);
                success = true;
            }
        }

        gtk_widget_destroy(dialog);
        return success;
    }

    std::wstring getFilePath() const { return m_resultPath; }
};

#endif // XFILEDIALOG_HPP
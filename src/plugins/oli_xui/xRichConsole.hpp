#ifndef XRICHCONSOLE_HPP
#define XRICHCONSOLE_HPP

#pragma once
#include "xRichEdit.hpp"
#include "../../ConsoleManager.hpp" // Pentru ILogOutput, LogLevel
#include <gtk/gtk.h>
#include <string>
#include <locale>
#include <codecvt>

class xRichConsole : public xRichEdit, public ILogOutput {
public:
    xRichConsole(const std::string& id, int x, int y, int width, int height, EventDispatcher& dispatcher)
        : xRichEdit(id, x, y, width, height, dispatcher) {
        
        // Înregistrare automată la ConsoleManager-ul partajat din OliEngine
        ConsoleManager::getInstance().addOutput(this);
    }

    ~xRichConsole() {
        // Dezînregistrare automată la distrugerea controlului
        ConsoleManager::getInstance().removeExtraOutput(this);
    }

    // Implementarea interfeței ILogOutput cu suport pentru culori în Linux GTK
    void writeLog(const std::wstring& message, LogLevel level) override {
        std::wstring prefix = L"";
        std::string colorHex = "#DCDCDC"; // Gri deschis (implicit)

        // Mapăm fiecare nivel la prefixul său și codul de culoare HEX echivalent din Windows
        switch (level) {
            case LogLevel::DEBUG:
                prefix = L"[DEBUG] ";
                colorHex = "#3296FF"; // Albastru deschis
                break;
            case LogLevel::INFO:
                prefix = L"[INFO] ";
                colorHex = "#8C8C8C"; // Gri deschis
                break;
            case LogLevel::SUCCESS:
                prefix = L"[SUCCESS] ";
                colorHex = "#32DC32"; // Verde luminos
                break;
            case LogLevel::WARNING:
                prefix = L"[WARN] ";
                colorHex = "#FFC800"; // Galben / Portocaliu
                break;
            case LogLevel::LOG_ERROR:
                prefix = L"[ERROR] ";
                colorHex = "#FF5050"; // Roșu deschis
                break;
            case LogLevel::FATAL_ERROR:
                prefix = L"[FATAL] ";
                colorHex = "#FF0000"; // Roșu aprins
                break;
        }

        std::wstring fullLine = prefix + message + L"\n";
        
        // Convertim wstring în UTF-8 (cerut obligatoriu de GTK)
        std::string utf8Line = wstr_to_utf8_local(fullLine);

        // Apelăm funcția internă de scriere colorată în GtkTextView
        appendColoredText(utf8Line, colorHex);
    }

private:
    // Funcție locală de conversie sigură wide -> utf8
    std::string wstr_to_utf8_local(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        try {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            return converter.to_bytes(wstr);
        } catch (...) {
            return std::string(wstr.begin(), wstr.end());
        }
    }

    // Rutină nativă GTK3 pentru scriere și auto-scroll la final
    void appendColoredText(const std::string& text, const std::string& colorHex) {
        GtkWidget* baseWidget = this->getHandle(); // xControl::getHandle()
        if (!baseWidget) return;

        // În arhitectura noastră, xRichEdit poate fi înfășurat într-un GtkScrolledWindow.
        // Găsim widget-ul intern GtkTextView dedicat randării textului.
        GtkWidget* textView = baseWidget;
        if (GTK_IS_SCROLLED_WINDOW(baseWidget)) {
            textView = gtk_bin_get_child(GTK_BIN(baseWidget));
        }

        if (!textView || !GTK_IS_TEXT_VIEW(textView)) return;

        // Preluăm bufferul de text asociat consolei
        GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textView));
        if (!buffer) return;

        // Managementul tagului de culoare: verificăm dacă există deja, dacă nu, îl creăm dinamic
        GtkTextTagTable* tagTable = gtk_text_buffer_get_tag_table(buffer);
        GtkTextTag* colorTag = gtk_text_tag_table_lookup(tagTable, colorHex.c_str());
        if (!colorTag) {
            colorTag = gtk_text_buffer_create_tag(buffer, colorHex.c_str(), "foreground", colorHex.c_str(), NULL);
        }

        // Obținem iteratorul poziționat la sfârșitul absolut al consolei
        GtkTextIter endIter;
        gtk_text_buffer_get_end_iter(buffer, &endIter);

        // Inserăm textul marcat cu tag-ul de culoare ales
        gtk_text_buffer_insert_with_tags(buffer, &endIter, text.c_str(), -1, colorTag, NULL);

        // 🔥 Auto-scroll la final (WM_VSCROLL SB_BOTTOM echivalent în GTK)
        gtk_text_buffer_get_end_iter(buffer, &endIter);
        GtkTextMark* insertMark = gtk_text_buffer_get_insert(buffer);
        gtk_text_buffer_move_mark(buffer, insertMark, &endIter);
        
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(textView), insertMark, 0.0, FALSE, 0.0, 1.0);
    }
};

#endif // XRICHCONSOLE_HPP
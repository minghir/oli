#ifndef XCONTAINER_HPP
#define XCONTAINER_HPP

#pragma once
#include "xControl.hpp"            // Clasa de bază nativă GTK
#include "../../ConsoleManager.hpp"
#include <gtk/gtk.h>
#include <string>
#include <memory>

#include "IXLayoutStrategy.hpp" 

class xContainer : public xControl {
protected:
    std::unique_ptr<IXLayoutStrategy> m_layoutStrategy;
    GtkWidget* m_layoutWidget = nullptr; // Sub-containerul structural (VBox, HBox, Grid)

public:
    // Constructori curați, fără HINSTANCE de Windows
    explicit xContainer(const std::string& id, EventDispatcher& dispatcher);

    explicit xContainer(
        const std::string& id,
        int x, int y, int width, int height,
        EventDispatcher& dispatcher
    );

    virtual ~xContainer() = default;

    // Metoda de creare adaptată pentru widget-ul părinte GTK
    void create(GtkWidget* parent) override;

    // Strategii de așezare structurală
    void setLayoutStrategy(std::unique_ptr<IXLayoutStrategy> strategy) {
        m_layoutStrategy = std::move(strategy);
        applyLayout();
    }

    // Calculează și aliniază marginile și modurile FILL conform regulilor din scriptul Oli
    virtual void applyLayout();

    virtual void scale(int /*newDpi*/) {}
    IXLayoutStrategy* getLayoutStrategy() const { return m_layoutStrategy.get(); }
	
    bool setProperty(const std::wstring& name, const vData& value) override;
    vData getProperty(const std::wstring& name) const override;

    GtkWidget* getLayoutWidget() const { return m_layoutWidget ? m_layoutWidget : m_widget; }
};

// Aliasing de compatibilitate în cazul în care alte fișiere cpp folosesc vechiul nume
using vContainer = xContainer;

#endif // XCONTAINER_HPP
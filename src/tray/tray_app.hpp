#pragma once

#include <string>
#include <gtk/gtk.h>
#include <libayatana-appindicator/app-indicator.h>

class TrayApp {
public:
    static TrayApp& instance();

    int run(int argc, char** argv, const std::string& base_dir);
    void update_status_ui();
    void open_browser();
    void quit();

private:
    TrayApp() = default;
    ~TrayApp() = default;

    bool check_single_instance();
    void create_indicator();
    void build_menu();

    std::string base_dir_;
    AppIndicator* indicator_{nullptr};
    GtkWidget* menu_{nullptr};
    GtkWidget* item_status_{nullptr};
    GtkWidget* item_toggle_{nullptr};
    GtkWidget* item_restart_{nullptr};
    bool last_running_{false};
};

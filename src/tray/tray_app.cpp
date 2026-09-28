#include "tray_app.hpp"
#include "tray_process.hpp"
#include "tray_server.hpp"
#include "path_service.hpp"
#include <unistd.h>
#include <signal.h>
#include <fstream>
#include <iostream>
#include <cstdlib>

static std::string get_lock_path() {
    return PathService::expand_user("~/.denselite/tray.lock");
}

TrayApp& TrayApp::instance() {
    static TrayApp inst;
    return inst;
}

bool TrayApp::check_single_instance() {
    std::string lock_path = get_lock_path();
    std::ifstream in(lock_path);
    if (in.is_open()) {
        pid_t pid = 0;
        in >> pid;
        if (pid > 0 && kill(pid, 0) == 0) {
            std::cout << "[TrayApp] DenseLite is already running (PID " << pid << "). Activating UI.\n";
            open_browser();
            return false;
        }
    }
    std::ofstream out(lock_path);
    if (out.is_open()) out << getpid();
    return true;
}

void TrayApp::open_browser() {
    std::string url = "http://127.0.0.1:9500";
    if (!gtk_show_uri_on_window(nullptr, url.c_str(), GDK_CURRENT_TIME, nullptr)) {
        std::string cmd = "xdg-open " + url + " >/dev/null 2>&1 &";
        system(cmd.c_str());
    }
}

void TrayApp::update_status_ui() {
    bool running = TrayProcess::instance().is_running();
    if (running != last_running_ || !item_status_) {
        last_running_ = running;
        if (indicator_) {
            app_indicator_set_icon(indicator_, running ? "network-transmit-receive" : "network-idle");
        }
        if (item_status_) {
            std::string label = running ? "● DenseLite: RUNNING (:9501)" : "○ DenseLite: STOPPED";
            gtk_menu_item_set_label(GTK_MENU_ITEM(item_status_), label.c_str());
        }
        if (item_toggle_) {
            gtk_menu_item_set_label(GTK_MENU_ITEM(item_toggle_), running ? "Stop Engine" : "Start Engine");
        }
        if (item_restart_) {
            gtk_widget_set_sensitive(item_restart_, running ? TRUE : FALSE);
        }
    }
}

void TrayApp::build_menu() {
    menu_ = gtk_menu_new();

    item_status_ = gtk_menu_item_new_with_label("○ DenseLite: STOPPED");
    gtk_widget_set_sensitive(item_status_, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_), item_status_);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu_), gtk_separator_menu_item_new());

    GtkWidget* item_open = gtk_menu_item_new_with_label("Open Settings Control Panel");
    g_signal_connect_swapped(item_open, "activate", G_CALLBACK(+[](TrayApp* app) { app->open_browser(); }), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_), item_open);

    item_toggle_ = gtk_menu_item_new_with_label("Start Engine");
    g_signal_connect_swapped(item_toggle_, "activate", G_CALLBACK(+[](TrayApp* app) {
        if (TrayProcess::instance().is_running()) TrayProcess::instance().stop();
        else TrayProcess::instance().start(app->base_dir_);
        app->update_status_ui();
    }), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_), item_toggle_);

    item_restart_ = gtk_menu_item_new_with_label("Restart Engine");
    gtk_widget_set_sensitive(item_restart_, FALSE);
    g_signal_connect_swapped(item_restart_, "activate", G_CALLBACK(+[](TrayApp* app) {
        TrayProcess::instance().restart(app->base_dir_);
        app->update_status_ui();
    }), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_), item_restart_);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu_), gtk_separator_menu_item_new());

    GtkWidget* item_quit = gtk_menu_item_new_with_label("Exit DenseLite");
    g_signal_connect_swapped(item_quit, "activate", G_CALLBACK(+[](TrayApp* app) { app->quit(); }), this);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_), item_quit);

    gtk_widget_show_all(menu_);
    app_indicator_set_menu(indicator_, GTK_MENU(menu_));
}

void TrayApp::create_indicator() {
    indicator_ = app_indicator_new("denselite", "network-idle", APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    app_indicator_set_status(indicator_, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_title(indicator_, "DenseLite AI Runtime");
    build_menu();
}

void TrayApp::quit() {
    std::cout << "[TrayApp] Shutting down DenseLite..." << std::endl;
    TrayProcess::instance().stop();
    TrayServer::instance().stop();
    unlink(get_lock_path().c_str());
    gtk_main_quit();
}

int TrayApp::run(int argc, char** argv, const std::string& base_dir) {
    base_dir_ = base_dir;
    gtk_init(&argc, &argv);

    if (!check_single_instance()) return 0;

    std::signal(SIGINT, [](int) { TrayApp::instance().quit(); });
    std::signal(SIGTERM, [](int) { TrayApp::instance().quit(); });

    create_indicator();
    TrayServer::instance().start(base_dir, 9500);

    g_timeout_add(1000, [](gpointer data) -> gboolean {
        static_cast<TrayApp*>(data)->update_status_ui();
        return G_SOURCE_CONTINUE;
    }, this);

    std::cout << "[TrayApp] Tray supervisor active. Engine is IDLE.\n";
    std::cout << "[TrayApp] Control Plane: http://127.0.0.1:9500\n";
    gtk_main();
    return 0;
}

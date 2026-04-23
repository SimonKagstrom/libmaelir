#pragma once

#include "lv_event_listener.hh"
#include "timer_manager.hh"

#include <etl/vector.h>
#include <functional>
#include <string>

class MenuScreen
{
public:
    struct NumericEntryConfig
    {
        int low;
        int high;
        int step = 1;
    };

    class Page
    {
    public:
        explicit Page(MenuScreen& parent);

        Page(const Page&) = delete;
        Page& operator=(const Page&) = delete;
        Page(Page&&) = default;

        Page& AddSubPage(const char* text);


        void AddEntry(const std::string& text, const std::function<void()>& on_click);

        void AddSeparator();

        void AddBooleanEntry(const char* text,
                             bool default_value,
                             const std::function<void(bool)>& on_click);

        void AddNumericEntry(const char* text,
                             NumericEntryConfig config,
                             int default_value,
                             const std::function<void(int value)>& on_click);

        Page(MenuScreen& parent, lv_obj_t* parent_menu);

    private:
        MenuScreen& m_parent;
        lv_obj_t* m_page;

        std::vector<std::unique_ptr<Page>> m_sub_pages;
    };


    MenuScreen(os::TimerManager& timer_manager,
               lv_obj_t* screen,
               lv_indev_t* lvgl_input_dev,
               const std::function<void()>& on_close);

    virtual ~MenuScreen();

    Page& GetMainPage();
    void BumpExitTimer();

private:
    os::TimerManager& m_timer_manager;
    lv_obj_t* m_screen;
    lv_indev_t* m_lvgl_input_dev;
    std::function<void()> m_on_close;

    lv_style_t m_style_selected;
    lv_style_t m_style_numeric_roller_main_focused;
    lv_style_t m_style_numeric_roller_selected;
    lv_obj_t* m_menu;
    lv_group_t* m_input_group;

    std::vector<std::unique_ptr<LvEventListener>> m_event_listeners;

    os::TimerHandle m_exit_timer;
    std::unique_ptr<Page> m_main_page;
};

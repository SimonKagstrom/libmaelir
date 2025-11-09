#pragma once

#include "lv_event_listener.hh"
#include "timer_manager.hh"

#include <etl/vector.h>
#include <functional>
#include <string>

class MenuScreen
{
public:
    class Page
    {
    public:
        explicit Page(MenuScreen& parent);

        Page(const Page&) = delete;
        Page& operator=(const Page&) = delete;
        Page(Page&&) = default;

        void AddEntry(const std::string& text, const std::function<void(lv_event_t*)>& on_click);

        Page AddSubPage(const char* text);

        void AddSeparator();

        void AddBooleanEntry(const char* text,
                             bool default_value,
                             const std::function<void(lv_event_t*)>& on_click);

    private:
        Page(MenuScreen& parent, lv_obj_t* parent_menu);

        MenuScreen& m_parent;
        lv_obj_t* m_page;
    };


    MenuScreen(os::TimerManager& timer_manager,
               lv_indev_t* lvgl_input_dev,
               const std::function<void()>& on_close);

    virtual ~MenuScreen();

    Page& GetMainPage();

private:
    void BumpExitTimer();

    os::TimerManager& m_timer_manager;
    lv_indev_t* m_lvgl_input_dev;
    std::function<void()> m_on_close;

    lv_obj_t* m_screen;

    lv_style_t m_style_selected;
    lv_obj_t* m_menu;
    lv_group_t* m_input_group;

    std::vector<std::unique_ptr<LvEventListener>> m_event_listeners;

    os::TimerHandle m_exit_timer;
};

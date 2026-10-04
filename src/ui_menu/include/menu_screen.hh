#pragma once

#include "lv_event_listener.hh"
#include "timer_manager.hh"

#include <etl/vector.h>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

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
        Page(const Page&) = delete;
        Page& operator=(const Page&) = delete;
        Page(Page&&) = default;

        Page& AddSubPage(const char* text);


        void AddEntry(const std::string& text, const std::function<void()>& on_click);

        void AddSeparator();

        void AddBooleanEntry(const char* text,
                             bool default_value,
                             const std::function<void(bool)>& on_click);

        void AddRollerEntry(const char* text,
                            std::span<const std::string_view> values,
                            std::string_view default_value,
                            const std::function<void(int which)>& on_click);

        void AddNumericEntry(const char* text,
                             NumericEntryConfig config,
                             int default_value,
                             const std::function<void(int value)>& on_click);

        Page(MenuScreen& parent, lv_obj_t* page, std::string_view title);

    private:
        // A row (description to the left, controls to the right)
        lv_obj_t* CreateRow();

        MenuScreen& m_parent;
        lv_obj_t* m_page;
        std::string m_title;

        std::vector<std::unique_ptr<Page>> m_sub_pages;
    };


    MenuScreen(os::TimerManager& timer_manager,
               lv_obj_t* screen,
               lv_indev_t* lvgl_input_dev,
               const std::function<void()>& on_close);

    virtual ~MenuScreen();

    Page& GetMainPage();
    void BumpExitTimer();
    void ExitMenu();

private:
    // A page entered from another page (the opener), or the main page
    struct VisiblePage
    {
        lv_obj_t* page;
        std::string title;
        lv_obj_t* opener;
    };

    // A (hidden) page in the content area
    lv_obj_t* CreatePage();
    void EnterPage(lv_obj_t* page, std::string_view title, lv_obj_t* opener);
    // Back to the previous page, or close the menu from the main page
    void Back();
    void ShowPage(const VisiblePage& visible_page, lv_obj_t* focus);

    os::TimerManager& m_timer_manager;
    lv_obj_t* m_screen;
    lv_indev_t* m_lvgl_input_dev;
    std::function<void()> m_on_close;

    lv_style_t m_style_selected;
    lv_style_t m_style_row;
    lv_style_t m_style_header_button;
    lv_style_t m_style_separator;
    lv_style_t m_style_numeric_roller_main_focused;
    lv_style_t m_style_numeric_roller_selected;
    lv_obj_t* m_menu;
    lv_obj_t* m_back_button;
    lv_obj_t* m_title_label;
    lv_obj_t* m_content;
    lv_group_t* m_input_group;

    std::vector<VisiblePage> m_page_stack;

    os::TimerHandle m_exit_timer;
    std::unique_ptr<Page> m_main_page;
};

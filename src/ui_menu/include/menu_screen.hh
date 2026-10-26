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

        // on_enter is called when the sub page is entered, before focusing its first entry
        Page& AddSubPage(
            const char* text, const std::function<void()>& on_enter = []() { /* NOP */ });

        // Remove all entries and sub pages, e.g., to fill the page again with new content. If
        // the page is shown, the next added entry is focused. References to the sub pages are
        // invalid afterwards. Not from an event callback of an entry on this page, since the
        // entry is deleted
        void Clear();


        // Returns the row, e.g., to add more labels (the text label is the first child)
        lv_obj_t* AddEntry(const std::string& text, const std::function<void()>& on_click);

        void AddSeparator();

        void AddBooleanEntry(const char* text,
                             bool default_value,
                             const std::function<void(bool)>& on_click);

        // on_change is called when scrolling through the values, before selecting one
        void AddRollerEntry(const char* text,
                            std::span<const std::string_view> values,
                            std::string_view default_value,
                            const std::function<void(int which)>& on_click,
                            const std::function<void(int which)>& on_change = {});

        void AddNumericEntry(const char* text,
                             NumericEntryConfig config,
                             int default_value,
                             const std::function<void(int value)>& on_click);

        // A centered image, not selectable. Returns the lv_image, to e.g. change the source
        lv_obj_t* AddImage(const lv_image_dsc_t* image);

        Page(MenuScreen& parent, lv_obj_t* page, std::string_view title);

    private:
        // A row (description to the left, controls to the right)
        lv_obj_t* CreateRow();

        // Add a selectable object to the input group
        void AddToInputGroup(lv_obj_t* obj);

        MenuScreen& m_parent;
        lv_obj_t* m_page;
        std::string m_title;

        // Cleared while shown, so focus the next added entry
        bool m_focus_next_added {false};

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

    // E.g., while entering text, which has its own timeout. Restarted when enabled again
    void SetExitTimerEnabled(bool enabled);

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
    // The page is about to be deleted, so go back to the page before it if it's entered
    void ForgetPage(lv_obj_t* page);
    bool IsShown(lv_obj_t* page) const;
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
    bool m_exit_timer_enabled {true};
    std::unique_ptr<Page> m_main_page;
};

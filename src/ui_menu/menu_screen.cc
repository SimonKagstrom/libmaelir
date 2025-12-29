#include "menu_screen.hh"

#include "hal/i_display.hh"

MenuScreen::MenuScreen(os::TimerManager& timer_manager,
                       lv_indev_t* lvgl_input_dev,
                       const std::function<void()>& on_close)
    : m_timer_manager(timer_manager)
    , m_lvgl_input_dev(lvgl_input_dev)
    , m_on_close(on_close)
    , m_screen(lv_obj_create(nullptr))
{
    // Create a style for the selected state
    m_style_selected = lv_style_t {};

    lv_style_init(&m_style_selected);
    lv_style_set_bg_opa(&m_style_selected, LV_OPA_COVER); // Ensure the background is fully opaque
    lv_style_set_radius(&m_style_selected, 10);

    m_input_group = lv_group_create();
    m_menu = lv_menu_create(m_screen);

    lv_obj_set_size(m_menu, hal::kDisplayWidth * 0.68f, hal::kDisplayHeight * 0.80f);
    lv_obj_center(m_menu);

    lv_menu_set_mode_root_back_button(m_menu, LV_MENU_ROOT_BACK_BUTTON_ENABLED);

    auto back_button = lv_menu_get_main_header_back_button(m_menu);
    lv_obj_set_style_bg_color(m_screen, lv_obj_get_style_bg_color(m_menu, lv_part_t::LV_PART_MAIN), 0);

    // Apply the style to the back button when it is focused
    lv_obj_add_style(back_button, &m_style_selected, LV_STATE_FOCUSED);
    lv_group_add_obj(m_input_group, back_button);
    lv_group_focus_obj(back_button);
    lv_obj_add_state(back_button, LV_STATE_FOCUS_KEY);

    lv_obj_t* main_page = lv_menu_page_create(m_menu, nullptr);


    lv_menu_set_page(m_menu, main_page);
    lv_indev_set_group(m_lvgl_input_dev, m_input_group);

    // Start the exit timer (10 seconds unless input is done)
    BumpExitTimer();

    // Register events to keep the screen alive on input
    m_event_listeners.push_back(LvEventListener::Create(
        m_screen, LV_EVENT_ROTARY, [this](lv_event_t*) { BumpExitTimer(); }));
    m_event_listeners.push_back(
        LvEventListener::Create(m_screen, LV_EVENT_KEY, [this](lv_event_t*) { BumpExitTimer(); }));
    lv_screen_load(m_screen);
}

MenuScreen::~MenuScreen()
{
    if (lv_indev_get_group(m_lvgl_input_dev) == m_input_group)
    {
        lv_indev_set_group(m_lvgl_input_dev, nullptr);
    }
    lv_group_delete(m_input_group);
}

void
MenuScreen::BumpExitTimer()
{
    m_exit_timer = m_timer_manager.StartTimer(10s, [this]() {
        m_on_close();
        return std::nullopt;
    });
}


MenuScreen::Page::Page(MenuScreen& parent)
    : MenuScreen::Page(parent, parent.m_menu)
{
}

MenuScreen::Page::Page(MenuScreen& parent, lv_obj_t* parent_page)
    : m_parent(parent)
    , m_page(lv_menu_page_create(parent_page, nullptr))
{
}

void
MenuScreen::Page::AddEntry(const std::string& text,
                           const std::function<void(lv_event_t*)>& on_click)
{
    auto cont = lv_menu_cont_create(m_page);
    auto label = lv_label_create(cont);

    lv_label_set_text(label, text.c_str());
    lv_group_add_obj(m_parent.m_input_group, cont);
    lv_obj_add_style(cont, &m_parent.m_style_selected, LV_STATE_FOCUSED);
    lv_obj_set_flex_grow(label, 1);

    m_parent.m_event_listeners.push_back(LvEventListener::Create(cont, LV_EVENT_CLICKED, on_click));
}

void
MenuScreen::Page::AddBooleanEntry(const char* text,
                                  bool default_value,
                                  const std::function<void(lv_event_t*)>& on_click)
{
    auto selected_switch_color = lv_palette_main(LV_PALETTE_LIGHT_GREEN);

    auto cont = lv_menu_cont_create(m_page);
    auto label = lv_label_create(cont);

    lv_obj_add_style(cont, &m_parent.m_style_selected, LV_STATE_FOCUSED);
    lv_obj_set_flex_grow(label, 1);
    lv_label_set_text(label, text);

    auto boolean_switch = lv_switch_create(cont);
    lv_obj_add_state(boolean_switch, default_value ? lv_state_t::LV_STATE_CHECKED : lv_state_t::LV_STATE_DEFAULT);
    // Highlight the label as well
    lv_obj_add_flag(boolean_switch, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_style_bg_color(
        boolean_switch, selected_switch_color, (int)LV_PART_INDICATOR | (int)LV_STATE_CHECKED);

    lv_group_add_obj(m_parent.m_input_group, boolean_switch);

    m_parent.m_event_listeners.push_back(
        LvEventListener::Create(boolean_switch, LV_EVENT_CLICKED, on_click));
}

void
MenuScreen::Page::AddSeparator()
{
    auto separator_color = lv_palette_main(LV_PALETTE_GREY);

    auto cont = lv_menu_cont_create(m_page);

    // Create a style for the separator line
    static lv_style_t style_separator;
    lv_style_init(&style_separator);
    lv_style_set_bg_opa(&style_separator, LV_OPA_COVER); // Ensure the background is fully opaque
    lv_style_set_bg_color(&style_separator, separator_color); // Set the background color to black
    lv_style_set_height(&style_separator, 2);          // Set the height of the separator line
    lv_style_set_width(&style_separator, lv_pct(100)); // Set the height of the separator line

    // Create the separator and apply the style
    auto separator = lv_menu_separator_create(cont);
    lv_obj_add_style(separator, &style_separator, 0);
}

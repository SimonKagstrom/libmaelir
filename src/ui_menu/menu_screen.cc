#include "menu_screen.hh"

#include "hal/i_display.hh"

#include <algorithm>

namespace
{

// A plain container, without the theme styles
lv_obj_t*
CreateContainer(lv_obj_t* parent)
{
    auto obj = lv_obj_create(parent);

    lv_obj_remove_style_all(obj);
    lv_obj_set_scrollable(obj, false);

    return obj;
}

bool
IsDescendantOf(lv_obj_t* obj, lv_obj_t* ancestor)
{
    for (; obj; obj = lv_obj_get_parent(obj))
    {
        if (obj == ancestor)
        {
            return true;
        }
    }

    return false;
}

} // namespace

MenuScreen::MenuScreen(os::TimerManager& timer_manager,
                       lv_obj_t* screen,
                       lv_indev_t* lvgl_input_dev,
                       const std::function<void()>& on_close)
    : m_timer_manager(timer_manager)
    , m_screen(screen)
    , m_lvgl_input_dev(lvgl_input_dev)
    , m_on_close(on_close)
{
    // Create a style for the selected state
    m_style_selected = lv_style_t {};

    lv_style_init(&m_style_selected);
    lv_style_set_bg_opa(&m_style_selected, LV_OPA_COVER); // Ensure the background is fully opaque
    lv_style_set_bg_color(&m_style_selected, lv_theme_get_color_primary(nullptr));
    lv_style_set_text_color(&m_style_selected, lv_color_white());
    lv_style_set_radius(&m_style_selected, 10);

    lv_style_init(&m_style_row);
    lv_style_set_pad_hor(&m_style_row, 10);
    lv_style_set_pad_ver(&m_style_row, 10);
    lv_style_set_pad_column(&m_style_row, 10);

    lv_style_init(&m_style_header_button);
    lv_style_set_pad_all(&m_style_header_button, 4);

    lv_style_init(&m_style_separator);
    lv_style_set_bg_opa(&m_style_separator, LV_OPA_COVER);
    lv_style_set_bg_color(&m_style_separator, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_height(&m_style_separator, 2);
    lv_style_set_width(&m_style_separator, lv_pct(100));

    lv_style_init(&m_style_numeric_roller_main_focused);
    lv_style_set_outline_width(&m_style_numeric_roller_main_focused, 0);
    lv_style_set_shadow_width(&m_style_numeric_roller_main_focused, 0);

    lv_style_init(&m_style_numeric_roller_selected);
    lv_style_set_outline_width(&m_style_numeric_roller_selected, 0);
    lv_style_set_shadow_width(&m_style_numeric_roller_selected, 0);
    lv_style_set_border_width(&m_style_numeric_roller_selected, 0);
    lv_style_set_radius(&m_style_numeric_roller_selected, 10);

    m_input_group = lv_group_create();
    lv_group_set_wrap(m_input_group, false);

    // The menu is a column with a header (back button + title), and the page area below. Only
    // one page is visible at the time. Built from base widgets, since lv_menu is deprecated
    m_menu = lv_obj_create(m_screen);
    lv_obj_set_style_pad_all(m_menu, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(m_menu, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(m_menu, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(m_menu, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(m_menu, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scrollable(m_menu, false);
    lv_obj_set_flex_flow(m_menu, LV_FLEX_FLOW_COLUMN);

    lv_obj_set_size(m_menu, hal::kDisplayWidth * 0.68f, hal::kDisplayHeight * 0.80f);
    lv_obj_center(m_menu);

    lv_obj_set_style_bg_color(
        m_screen, lv_obj_get_style_bg_color(m_menu, lv_part_t::LV_PART_MAIN), 0);

    auto header = CreateContainer(m_menu);
    lv_obj_set_size(header, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(header, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(header, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_column(header, 10, LV_PART_MAIN);

    // Always shown, closes the menu on the main page
    m_back_button = lv_button_create(header);
    lv_obj_remove_style_all(m_back_button);
    lv_obj_add_style(m_back_button, &m_style_header_button, 0);
    lv_obj_add_style(m_back_button, &m_style_selected, LV_STATE_FOCUSED);
    auto back_label = lv_label_create(m_back_button);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    LvEventListener::Create(m_back_button, LV_EVENT_CLICKED, [this](lv_event_t*) { Back(); });

    m_title_label = lv_label_create(header);
    lv_label_set_text(m_title_label, "");

    // The pages are in here, which scrolls if they don't fit
    m_content = CreateContainer(m_menu);
    lv_obj_set_width(m_content, lv_pct(100));
    lv_obj_set_flex_grow(m_content, 1);
    lv_obj_set_scrollable(m_content, true);
    lv_obj_set_scroll_dir(m_content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(m_content, LV_SCROLLBAR_MODE_OFF);

    lv_group_add_obj(m_input_group, m_back_button);
    lv_indev_set_group(m_lvgl_input_dev, m_input_group);

    auto main_page = CreatePage();
    m_main_page = std::make_unique<Page>(*this, main_page, "");
    m_page_stack.push_back({main_page, "", nullptr});
    ShowPage(m_page_stack.back(), m_back_button);
    lv_obj_add_state(m_back_button, LV_STATE_FOCUS_KEY);

    // Start the exit timer (10 seconds unless input is done)
    BumpExitTimer();
}

MenuScreen::~MenuScreen()
{
    if (lv_indev_get_group(m_lvgl_input_dev) == m_input_group)
    {
        lv_indev_set_group(m_lvgl_input_dev, nullptr);
    }
    lv_group_delete(m_input_group);
    lv_obj_delete(m_menu);
}

lv_obj_t*
MenuScreen::CreatePage()
{
    auto page = CreateContainer(m_content);

    lv_obj_set_size(page, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(page, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_hidden(page, true);

    return page;
}

void
MenuScreen::EnterPage(lv_obj_t* page, std::string_view title, lv_obj_t* opener)
{
    m_page_stack.push_back({page, std::string(title), opener});

    // Focus the first entry on the page (objects on hidden pages are skipped by the group)
    lv_obj_t* first = m_back_button;
    for (uint32_t i = 0; i < lv_group_get_obj_count(m_input_group); i++)
    {
        auto obj = lv_group_get_obj_by_index(m_input_group, i);
        if (IsDescendantOf(obj, page))
        {
            first = obj;
            break;
        }
    }

    ShowPage(m_page_stack.back(), first);
}

void
MenuScreen::Back()
{
    if (m_page_stack.size() <= 1)
    {
        m_on_close();
        return;
    }

    // Back to the entry which opened the page
    auto opener = m_page_stack.back().opener;
    lv_obj_set_hidden(m_page_stack.back().page, true);
    m_page_stack.pop_back();

    ShowPage(m_page_stack.back(), opener ? opener : m_back_button);
}

void
MenuScreen::ShowPage(const VisiblePage& visible_page, lv_obj_t* focus)
{
    for (const auto& page : m_page_stack)
    {
        lv_obj_set_hidden(page.page, page.page != visible_page.page);
    }
    lv_label_set_text(m_title_label, visible_page.title.c_str());
    lv_obj_scroll_to_y(m_content, 0, LV_ANIM_OFF);

    lv_group_set_editing(m_input_group, false);
    lv_group_focus_obj(focus);
}

void
MenuScreen::BumpExitTimer()
{
    m_exit_timer = m_timer_manager.StartTimer(10s, [this]() {
        m_on_close();
        return std::nullopt;
    });
}

void
MenuScreen::ExitMenu()
{
    m_exit_timer = m_timer_manager.StartTimer(0s, [this]() {
        m_on_close();
        return std::nullopt;
    });
}

MenuScreen::Page&
MenuScreen::GetMainPage()
{
    return *m_main_page;
}


MenuScreen::Page::Page(MenuScreen& parent, lv_obj_t* page, std::string_view title)
    : m_parent(parent)
    , m_page(page)
    , m_title(title)
{
}

lv_obj_t*
MenuScreen::Page::CreateRow()
{
    auto row = CreateContainer(m_page);

    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_style(row, &m_parent.m_style_row, 0);

    return row;
}

MenuScreen::Page&
MenuScreen::Page::AddSubPage(const char* text)
{
    auto cont = CreateRow();
    auto label = lv_label_create(cont);
    auto next_indicator = lv_label_create(cont);

    auto page = m_parent.CreatePage();
    auto sub_page = std::make_unique<Page>(m_parent, page, text);

    lv_label_set_text(label, text);
    lv_label_set_text(next_indicator, LV_SYMBOL_RIGHT);
    lv_group_add_obj(m_parent.m_input_group, cont);
    lv_obj_add_style(cont, &m_parent.m_style_selected, LV_STATE_FOCUSED);
    lv_obj_set_flex_grow(label, 1);

    LvEventListener::Create(
        cont, LV_EVENT_FOCUSED, [cont](lv_event_t*) { lv_obj_scroll_to_view_recursive(cont, LV_ANIM_OFF); });
    LvEventListener::Create(
        cont, LV_EVENT_CLICKED, [this, page, title = std::string(text), cont](lv_event_t*) {
            m_parent.EnterPage(page, title, cont);
        });

    m_sub_pages.push_back(std::move(sub_page));
    return *m_sub_pages.back();
}

void
MenuScreen::Page::AddEntry(const std::string& text, const std::function<void()>& on_click)
{
    auto cont = CreateRow();
    auto label = lv_label_create(cont);

    lv_label_set_text(label, text.c_str());
    lv_group_add_obj(m_parent.m_input_group, cont);
    lv_obj_add_style(cont, &m_parent.m_style_selected, LV_STATE_FOCUSED);
    lv_obj_set_flex_grow(label, 1);
    LvEventListener::Create(
        cont, LV_EVENT_FOCUSED, [cont](lv_event_t*) { lv_obj_scroll_to_view_recursive(cont, LV_ANIM_OFF); });

    LvEventListener::Create(cont, LV_EVENT_CLICKED, [on_click](lv_event_t*) { on_click(); });
}

void
MenuScreen::Page::AddBooleanEntry(const char* text,
                                  bool default_value,
                                  const std::function<void(bool)>& on_click)
{
    auto selected_switch_color = lv_palette_main(LV_PALETTE_LIGHT_GREEN);

    auto cont = CreateRow();
    auto label = lv_label_create(cont);

    lv_obj_add_style(cont, &m_parent.m_style_selected, LV_STATE_FOCUSED);
    lv_obj_set_flex_grow(label, 1);
    lv_label_set_text(label, text);

    auto boolean_switch = lv_switch_create(cont);
    lv_obj_add_state(boolean_switch,
                     default_value ? lv_state_t::LV_STATE_CHECKED : lv_state_t::LV_STATE_DEFAULT);
    // Highlight the label as well
    lv_obj_set_event_bubble(boolean_switch, true);
    lv_obj_set_style_bg_color(
        boolean_switch, selected_switch_color, (int)LV_PART_INDICATOR | (int)LV_STATE_CHECKED);

    lv_group_add_obj(m_parent.m_input_group, boolean_switch);

    LvEventListener::Create(boolean_switch, LV_EVENT_FOCUSED, [cont](lv_event_t*) {
        lv_obj_scroll_to_view_recursive(cont, LV_ANIM_OFF);
    });

    LvEventListener::Create(boolean_switch, LV_EVENT_CLICKED, [on_click](lv_event_t* e) {
        auto sw = static_cast<lv_obj_t*>(lv_event_get_target(e));
        auto checked = lv_obj_has_state(sw, LV_STATE_CHECKED);
        on_click(checked);
    });
}

void
MenuScreen::Page::AddRollerEntry(const char* text,
                                 std::span<const std::string_view> values,
                                 std::string_view default_value,
                                 const std::function<void(int which)>& on_click)
{
    auto cont = CreateRow();
    auto label = lv_label_create(cont);
    auto roller = lv_roller_create(cont);

    lv_obj_add_style(cont, &m_parent.m_style_selected, LV_STATE_FOCUSED);
    lv_obj_add_style(roller,
                     &m_parent.m_style_numeric_roller_main_focused,
                     (int)LV_PART_MAIN | (int)LV_STATE_FOCUSED);
    lv_obj_add_style(roller, &m_parent.m_style_numeric_roller_selected, LV_PART_SELECTED);
    lv_obj_set_flex_grow(label, 1);
    lv_label_set_text(label, text);

    std::string options;
    int selected_index = 0;
    for (int i = 0; i < static_cast<int>(values.size()); i++)
    {
        if (!options.empty())
        {
            options += "\n";
        }

        auto v = values[i];
        options += v;
        if (v == default_value)
        {
            selected_index = i;
        }
    }

    lv_roller_set_options(roller, options.c_str(), LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(roller, selected_index, LV_ANIM_OFF);
    lv_roller_set_visible_row_count(roller, 1);

    lv_group_add_obj(m_parent.m_input_group, roller);

    // Keep the row highlighted while the roller is focused/edited.
    LvEventListener::Create(roller, LV_EVENT_FOCUSED, [cont](lv_event_t*) {
        lv_obj_add_state(cont, LV_STATE_FOCUSED);
        lv_obj_add_state(cont, LV_STATE_FOCUS_KEY);
        lv_obj_scroll_to_view_recursive(cont, LV_ANIM_OFF);
    });

    LvEventListener::Create(roller, LV_EVENT_DEFOCUSED, [cont](lv_event_t*) {
        lv_obj_remove_state(cont, LV_STATE_FOCUSED);
        lv_obj_remove_state(cont, LV_STATE_FOCUS_KEY);
    });

    LvEventListener::Create(roller, LV_EVENT_CLICKED, [on_click](lv_event_t* e) {
        auto roller = static_cast<lv_obj_t*>(lv_event_get_target(e));
        auto selected = lv_roller_get_selected(roller);

        on_click(selected);
    });
}


void
MenuScreen::Page::AddNumericEntry(const char* text,
                                  MenuScreen::NumericEntryConfig config,
                                  int default_value,
                                  const std::function<void(int value)>& on_click)
{
    assert(config.step != 0);

    if (config.low > config.high)
    {
        std::swap(config.low, config.high);
    }

    auto cont = CreateRow();
    auto label = lv_label_create(cont);
    auto roller = lv_roller_create(cont);

    lv_obj_add_style(cont, &m_parent.m_style_selected, LV_STATE_FOCUSED);
    lv_obj_add_style(roller,
                     &m_parent.m_style_numeric_roller_main_focused,
                     (int)LV_PART_MAIN | (int)LV_STATE_FOCUSED);
    lv_obj_add_style(roller, &m_parent.m_style_numeric_roller_selected, LV_PART_SELECTED);
    lv_obj_set_flex_grow(label, 1);
    lv_label_set_text(label, text);

    std::string options;
    for (int value = config.low; value <= config.high; value += config.step)
    {
        if (!options.empty())
        {
            options += "\n";
        }
        options += std::to_string(value);
    }

    const int clamped_default = std::clamp(default_value, config.low, config.high);
    const int selected_index = (clamped_default - config.low) / config.step;

    lv_roller_set_options(roller, options.c_str(), LV_ROLLER_MODE_NORMAL);
    lv_roller_set_selected(roller, selected_index, LV_ANIM_OFF);
    lv_roller_set_visible_row_count(roller, 1);

    lv_group_add_obj(m_parent.m_input_group, roller);

    // Keep the row highlighted while the roller is focused/edited.

    LvEventListener::Create(roller, LV_EVENT_FOCUSED, [cont](lv_event_t*) {
        lv_obj_add_state(cont, LV_STATE_FOCUSED);
        lv_obj_add_state(cont, LV_STATE_FOCUS_KEY);
        lv_obj_scroll_to_view_recursive(cont, LV_ANIM_OFF);
    });

    LvEventListener::Create(roller, LV_EVENT_DEFOCUSED, [cont](lv_event_t*) {
        lv_obj_remove_state(cont, LV_STATE_FOCUSED);
        lv_obj_remove_state(cont, LV_STATE_FOCUS_KEY);
    });

    LvEventListener::Create(roller, LV_EVENT_CLICKED, [on_click, config](lv_event_t* e) {
        auto roller = static_cast<lv_obj_t*>(lv_event_get_target(e));
        auto selected = lv_roller_get_selected(roller);
        int value = config.low + selected * config.step;
        on_click(value);
    });
}


void
MenuScreen::Page::AddSeparator()
{
    auto cont = CreateRow();
    auto separator = CreateContainer(cont);

    lv_obj_add_style(separator, &m_parent.m_style_separator, 0);
}

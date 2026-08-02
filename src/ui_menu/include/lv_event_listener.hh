#pragma once

#include <functional>
#include <lvgl.h>
#include <memory>

class LvEventListener
{
public:
    LvEventListener() = delete;

    static void Create(lv_obj_t* obj, lv_event_code_t event, std::function<void(lv_event_t*)> cb)
    {
        // Deleted when the parent is removed
        (void)new LvEventListener(obj, event, cb);
    }

private:
    LvEventListener(lv_obj_t* obj, lv_event_code_t event, std::function<void(lv_event_t*)> cb)
        : m_cb(cb)
    {
        lv_obj_add_event_cb(obj, EventHandler, event, this);

        // Delete this when the object is deleted
        lv_obj_add_event_cb(
            obj,
            [](lv_event_t* e) {
                auto p = reinterpret_cast<LvEventListener*>(lv_event_get_user_data(e));
                delete p;
            },
            LV_EVENT_DELETE,
            this);
    }

    static void EventHandler(lv_event_t* e)
    {
        auto p = reinterpret_cast<LvEventListener*>(lv_event_get_user_data(e));
        p->m_cb(e);
    }

    std::function<void(lv_event_t*)> m_cb;
};

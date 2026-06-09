#include <stdlib.h>
#include <stdio.h>
#include "lvgl/lvgl.h"
#include "ui_helpers.h"


lv_obj_t *create_label(const char *text, lv_obj_t *view) {
    lv_obj_t *label = lv_label_create(view);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);

    return label;
}

lv_obj_t *create_icon(const void *src, lv_obj_t *view, int x, int y, int scaling) {
    lv_obj_t *icon = lv_image_create(view);
    lv_obj_set_pos(icon, x, y);
    lv_image_set_src(icon, src);
    lv_image_set_scale(icon, scaling);
    lv_image_set_antialias(icon, false);

    return icon;
}

lv_obj_t *create_settings_category(lv_obj_t *view, char text[64], int x, int y) {
    lv_obj_t *label = create_label(text, view);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_34, 0);

    return label;
}


lv_obj_t *create_settings_part(
    lv_obj_t *view,
    char description[128],
    char element_text[64],
    SettingsFormType settings_type,
    void (*callback)(lv_event_t*),
    int action_id
) {
    lv_obj_t *description_label = create_label(description, view);
    lv_obj_set_style_text_font(description_label, &lv_font_montserrat_18, 0);

    lv_obj_set_width(description_label, 196);
    lv_label_set_long_mode(description_label, LV_LABEL_LONG_WRAP);

    lv_obj_t *settings_element;

    if(settings_type == CLICK) {
        settings_element = lv_button_create(view);
        lv_obj_add_event_cb(settings_element, callback, LV_EVENT_CLICKED, action_id);
        lv_obj_t *button_text = create_label(element_text, settings_element);
        lv_obj_set_style_text_font(button_text, &lv_font_montserrat_18, 0);
    } else if(settings_type == TRUEFALSE) {
        settings_element = lv_switch_create(view);
        lv_obj_add_event_cb(settings_element, callback, LV_EVENT_VALUE_CHANGED, action_id);
    } else if(settings_type == NUMBER || settings_type == TEXT) {
        settings_element = lv_textarea_create(view);
        lv_textarea_set_one_line(settings_element, true);
        lv_textarea_set_placeholder_text(settings_element, element_text);
        lv_obj_add_event_cb(settings_element, callback, LV_EVENT_VALUE_CHANGED, action_id);
        lv_obj_set_width(settings_element, 190);
    }

    return settings_element;
}

lv_obj_t *create_settings_container(
    lv_obj_t *view,
    char label[64]
) {
    lv_obj_t *container = lv_obj_create(view);

    lv_obj_set_layout(container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);

    lv_obj_set_style_bg_color(container, lv_color_hex(0xe3e3e3), 0);
    lv_obj_set_style_radius(container, 16, 0);
    lv_obj_set_style_margin_all(container, 4, 0);
    lv_obj_set_width(container, 240-8);
    lv_obj_set_style_pad_all(container, 2, 0);

    lv_obj_t *title = create_label(label, container);
    lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);

    return container;
}


void update_height(lv_obj_t *view) {
    lv_obj_t *first = lv_obj_get_child(view, 0);
    lv_obj_t *last = lv_obj_get_child(view, -1);

    lv_obj_update_layout(view);
    lv_obj_update_layout(first);
    lv_obj_update_layout(last);

    int first_y = lv_obj_get_y(first);
    int last_bottom = lv_obj_get_y(last) + lv_obj_get_height(last);

    int height = last_bottom - first_y + 16;

    lv_obj_set_height(view, height);
}


lv_obj_t *create_settings_button(lv_obj_t *view, char label[64], char text[64], int x, int y, int action_id, void (*callback)(lv_event_t*)) {
    lv_button_t *button = lv_button_create(view);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, action_id);

    if (strcmp(label, "") != 0) {
        lv_label_t *desc_label = lv_label_create(view);
        lv_obj_set_style_text_font(desc_label, &lv_font_montserrat_24, 0);
        lv_label_set_text(desc_label, label);
    }


    lv_label_t *button_label = lv_label_create(button);
    lv_label_set_text(button_label, text);

    return button;
}

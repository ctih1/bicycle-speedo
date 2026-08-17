#ifndef UI_HELPERS_H
#define UI_HELPERS_H
#include <lvgl.h>

typedef enum {
    NUMBER,
    TEXT,
    TRUEFALSE,
    CLICK
} SettingsFormType;

lv_obj_t *create_label(const char *text, lv_obj_t *view);
lv_obj_t *create_icon(const void *src, lv_obj_t *view, int x, int y, int scaling);
lv_obj_t *create_settings_button(lv_obj_t *view, char label[64], char text[64], int x, int y, int action_id, void (*callback)(lv_event_t*));
lv_obj_t *create_settings_part(lv_obj_t *view, char description[128], char element_text[64], SettingsFormType settings_type, void (*callback)(lv_event_t*), int action_id);
lv_obj_t *create_settings_container(lv_obj_t *view, char label[64]);
void update_height(lv_obj_t *view);

#endif /* UI_HELPERS_H */
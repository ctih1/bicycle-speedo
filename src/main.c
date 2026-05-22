/**
 * @file main.c
 *
 */

/*********************
 *      INCLUDES
 *********************/

#ifndef _DEFAULT_SOURCE
  #define _DEFAULT_SOURCE /* needed for usleep() */
#endif

#include <stdlib.h>
#include <stdio.h>
#ifdef _MSC_VER
  #include <Windows.h>
#else
  #include <unistd.h>
  #include <pthread.h>
#endif
#include "lvgl/lvgl.h"
#include "lvgl/examples/lv_examples.h"
#include "lvgl/demos/lv_demos.h"
#include <SDL.h>
#include "hal/hal.h"

#define MARGIN 4
#define SIDE_MARGIN 8

#define BUTTON_GOTO_SETTINGS 1
#define BUTTON_GOFROM_SETTINGS 2
#define SETTINGS_BTN_RESET_TRIP 3

#ifdef LV_DEF_REFR_PERIOD
    #undef LV_DEF_REFR_PERIOD
#endif

#define REFRESH_SLEEP_MS 200

#if LV_USE_OS != LV_OS_FREERTOS

extern const lv_img_dsc_t down;
extern const lv_img_dsc_t up;
extern const lv_img_dsc_t bikeicon;

extern const lv_img_dsc_t battery;
extern const lv_img_dsc_t fwheelsensor;
extern const lv_img_dsc_t gearsensor;
extern const lv_img_dsc_t orientationsensor;
extern const lv_img_dsc_t pedalsensor;

static lv_obj_t *dashboard_view;
static lv_obj_t *settings_view;
static lv_obj_t *splash_screen_view;

static lv_obj_t *speed_label;
static lv_obj_t *gear_label;
static lv_obj_t *pedal_rpm_label;
static lv_obj_t *front_rpm_label;
static lv_obj_t *shift_image;
static lv_obj_t *bike_image;
static lv_obj_t *bike_rotation_label;
static lv_obj_t *battery_voltage_label;
static lv_obj_t *battery_voltage_rect;
static lv_obj_t *trip_distance_label;

static lv_obj_t *battery_icon;
static lv_obj_t *gear_icon;
static lv_obj_t *fwheel_icon;
static lv_obj_t *pedal_icon;
static lv_obj_t *orientation_icon;

static int speed = 4;
static int pedal_rpm = 60;
static int wheel_rpm = 168;
static float battery_voltage = 4.3f;
static int distance_traveled = 2419;

static int bike_rotation = 0;

static bool battery_warning = true;
static bool front_wheel_warning = false;
static bool pedal_warning = false;
static bool gear_warning = true;
static bool tilt_warning = false;

static bool splash_shown = false;

static void settings_callback(lv_event_t* event) {
    lv_event_code_t code = lv_event_get_code(event);
    int button_action = (int*)lv_event_get_user_data(event);

    if (button_action == BUTTON_GOTO_SETTINGS) {
        lv_scr_load(settings_view);
    } else if (button_action == BUTTON_GOFROM_SETTINGS) {
        lv_scr_load(dashboard_view);
    }
}

static void setting_change_callback(lv_event_t* event) {
    lv_event_code_t code = lv_event_get_code(event);
    int button_action = (int*)lv_event_get_user_data(event);

    switch(button_action) {
        case SETTINGS_BTN_RESET_TRIP:
            distance_traveled = 0;
            break;
    }
}


void create_button(char label[64], char text[64], int x, int y, int action_id) {
    lv_button_t *button = lv_button_create(settings_view);
    lv_obj_add_event_cb(button, setting_change_callback, LV_EVENT_CLICKED, action_id);

    lv_label_t *desc_label = lv_label_create(settings_view);
    lv_obj_set_style_text_font(desc_label, &lv_font_montserrat_24, 0);
    lv_label_set_text(desc_label, label);

    lv_label_t *button_label = lv_label_create(button);
    lv_label_set_text(button_label, text);

    lv_obj_set_pos(desc_label, x, y);
    lv_obj_set_pos(button, x, y + 26);
}

lv_obj_t *create_label(char text[64], lv_obj_t *view, int x, int y) {
    lv_obj_t *label = lv_label_create(view);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_28, 0);
    lv_obj_set_pos(label, x, y);

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

void create_dashboard() {
    speed_label = lv_label_create(dashboard_view);
    lv_obj_set_style_text_font(speed_label, &lv_font_montserrat_48, 0);
    lv_obj_set_pos(speed_label, SIDE_MARGIN, MARGIN);

    gear_label = lv_label_create(dashboard_view);
    lv_label_set_text(gear_label, "4TH");
    lv_obj_set_style_text_font(gear_label, &lv_font_montserrat_48, 0);
    lv_obj_set_pos(gear_label, SIDE_MARGIN, 48);

    lv_label_t *pedal_description = lv_label_create(dashboard_view);
    lv_label_set_text(pedal_description, "PEDALS");
    lv_obj_set_style_text_font(pedal_description, &lv_font_montserrat_28, 0);
    lv_obj_set_pos(pedal_description, SIDE_MARGIN, 140);

    pedal_rpm_label = lv_label_create(dashboard_view);
    lv_obj_set_style_text_font(pedal_rpm_label, &lv_font_montserrat_40, 0);
    lv_obj_set_pos(pedal_rpm_label, SIDE_MARGIN, 140+24);
    lv_obj_set_style_text_align(pedal_rpm_label, LV_TEXT_ALIGN_RIGHT, 0);

    shift_image = lv_image_create(dashboard_view);
    lv_obj_set_pos(shift_image, 200-SIDE_MARGIN, 140+28);

    lv_label_t *wheel_description = lv_label_create(dashboard_view);
    lv_label_set_text(wheel_description, "WHEEL");
    lv_obj_set_style_text_font(wheel_description, &lv_font_montserrat_28, 0);
    lv_obj_set_pos(wheel_description, SIDE_MARGIN, 220);

    front_rpm_label = lv_label_create(dashboard_view);
    lv_obj_set_style_text_font(front_rpm_label, &lv_font_montserrat_40, 0);
    lv_obj_set_pos(front_rpm_label, SIDE_MARGIN, 220+24);
    lv_obj_set_style_text_align(front_rpm_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(front_rpm_label, lv_color_hex(0x5142f5), LV_PART_MAIN);

    battery_voltage_label = lv_label_create(dashboard_view);
    lv_obj_set_style_text_font(battery_voltage_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(battery_voltage_label, 170, 52+MARGIN);

    battery_voltage_rect = lv_obj_create(dashboard_view);
    lv_obj_set_size(battery_voltage_rect, 28, 52);
    lv_obj_set_pos(battery_voltage_rect, 240-24-SIDE_MARGIN, MARGIN);
    lv_obj_set_style_bg_color(battery_voltage_rect, lv_color_hex(0XFF0000), 0);

    trip_distance_label = lv_label_create(dashboard_view);
    lv_obj_set_style_text_font(trip_distance_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(trip_distance_label, SIDE_MARGIN, 320-20-MARGIN);

    bike_image = create_icon(&bikeicon, dashboard_view, 155, 90, 256);
    bike_rotation_label = lv_label_create(dashboard_view);
    lv_obj_set_style_text_font(bike_rotation_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(bike_rotation_label, 155, 90);

    lv_button_t *settings_button = lv_button_create(dashboard_view);
    lv_label_t *settings_button_label = lv_label_create(settings_button);
    lv_label_set_text(settings_button_label, "Settings");
    lv_obj_add_event_cb(settings_button, settings_callback, LV_EVENT_CLICKED, BUTTON_GOTO_SETTINGS);
    lv_obj_set_align(settings_button, LV_ALIGN_BOTTOM_RIGHT);
}

void create_settings() {
    lv_label_t *settings_header = lv_label_create(settings_view);
    lv_label_set_text(settings_header, "Settings");
    lv_obj_set_style_text_font(settings_header, &lv_font_montserrat_28, 0);
    lv_obj_set_pos(settings_header, SIDE_MARGIN, MARGIN);

    lv_button_t *settings_button = lv_button_create(settings_view);
    lv_label_t *settings_button_label = lv_label_create(settings_button);
    lv_label_set_text(settings_button_label, "Back");
    lv_obj_add_event_cb(settings_button, settings_callback, LV_EVENT_CLICKED, BUTTON_GOFROM_SETTINGS);
    lv_obj_set_align(settings_button, LV_ALIGN_BOTTOM_RIGHT);

    create_button("Reset current trip", "Reset", SIDE_MARGIN, 60, SETTINGS_BTN_RESET_TRIP);
    create_button("Reset trip A", "Reset", SIDE_MARGIN, 120, 2);
    create_button("Reset trip B", "Reset", SIDE_MARGIN, 180, 3);
    create_button("Reset trip C", "Reset", SIDE_MARGIN, 240, 4);
    create_button("Metric/Imperial", "Toggle", SIDE_MARGIN, 300, 4);
    create_button("Wipe data part.", "Wipe", SIDE_MARGIN, 360, 4);
}


void create_open_animation() {

    lv_label_t *label = create_label("BATT 3.3 V", splash_screen_view, SIDE_MARGIN, 260);

    battery_icon = create_icon(&battery, splash_screen_view, 32, 50, 512);
    gear_icon = create_icon(&gearsensor, splash_screen_view, 32+64+MARGIN, 50, 512);
    fwheel_icon = create_icon(&fwheelsensor, splash_screen_view, 32+128+MARGIN, 50, 512);
    pedal_icon = create_icon(&pedalsensor, splash_screen_view, 32, 50+64, 512);
    orientation_icon = create_icon(&orientationsensor, splash_screen_view, 32+64+MARGIN, 50+64, 512);
}

void create_ui() {
    dashboard_view = lv_obj_create(NULL);
    settings_view = lv_obj_create(NULL);
    splash_screen_view = lv_obj_create(NULL);

    create_dashboard();
    create_settings();
    create_open_animation();
}

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  lv_init();

  sdl_hal_init(240, 320);

  create_ui();
  lv_scr_load(splash_screen_view);

  while(1) {
    uint32_t sleep_time_ms = lv_timer_handler();

    if (!splash_shown) {
        #ifdef _MSC_VER
            Sleep(1500);
        #else
            usleep(1500 * 1000);
        #endif

        if (!gear_warning) {
            lv_obj_add_flag(gear_icon, LV_OBJ_FLAG_HIDDEN);
        }
        if (!tilt_warning) {
            lv_obj_add_flag(orientation_icon, LV_OBJ_FLAG_HIDDEN);
        }
        if (!pedal_warning) {
            lv_obj_add_flag(pedal_icon, LV_OBJ_FLAG_HIDDEN);
        }
        if (!battery_warning) {
            lv_obj_add_flag(battery_icon, LV_OBJ_FLAG_HIDDEN);
        }

        lv_timer_handler();

        #ifdef _MSC_VER
            Sleep(2000);
        #else
            usleep(2000 * 1000);
        #endif

        splash_shown = true;
        lv_scr_load(dashboard_view);
    }

    char speed_text[32];
    sprintf(speed_text, "%d KMH", speed);
    lv_label_set_text(speed_label, speed_text);

    char pedal_text[32];
    sprintf(pedal_text, "%d RPM", pedal_rpm);
    lv_label_set_text(pedal_rpm_label, pedal_text);

    char wheel_text[32];
    sprintf(wheel_text, "%d RPM", wheel_rpm);
    lv_label_set_text(front_rpm_label, wheel_text);

    char battery_text[32];
    sprintf(battery_text, "%.2f V", battery_voltage);
    lv_label_set_text(battery_voltage_label, battery_text);

    char trip_text[32];
    sprintf(trip_text, "TRIP: %.2f KM", (double)distance_traveled/1000);
    lv_label_set_text(trip_distance_label, trip_text);

    char bike_rotation_text[32];
    sprintf(bike_rotation_text, "%d *", bike_rotation);
    lv_label_set_text(bike_rotation_label, bike_rotation_text);

    lv_color_t rpm_color = lv_color_hex(0xFF0000);
    lv_color_t battery_color = lv_color_hex(0XFF0000);

    lv_obj_add_flag(shift_image, LV_OBJ_FLAG_HIDDEN);

    if (pedal_rpm > 105) { // too fast
        lv_image_set_src(shift_image, &up);
        lv_obj_clear_flag(shift_image, LV_OBJ_FLAG_HIDDEN);
        rpm_color = lv_color_hex(0XF51414);
    } else if (pedal_rpm > 95) { // aerobic
        rpm_color = lv_color_hex(0XFAA40F);
    } else if (pedal_rpm > 80) { // optimal
        rpm_color = lv_color_hex(0X00BF2D);
    } else if (pedal_rpm > 70) { // relaxed
        rpm_color = lv_color_hex(0x008CBF);
    } else { // too slow
        rpm_color = lv_color_hex(0x0D00BF);
        lv_image_set_src(shift_image, &down);
        lv_obj_clear_flag(shift_image, LV_OBJ_FLAG_HIDDEN);
    }

    if (battery_voltage > 4.4) {
        battery_color = lv_color_hex(0x1ce830);
    } else if (battery_voltage > 4) {
        battery_color = lv_color_hex(0x96e81c);
    } else if (battery_voltage > 3.6) {
        battery_color = lv_color_hex(0xedd711);
    } else if(battery_voltage > 3.4) {
        battery_color = lv_color_hex(0xed5a11);
    } else {
        battery_color = lv_color_hex(0xed1111);
    }

    bike_rotation += 1;

    if (bike_rotation > 3600) {
        bike_rotation = 0;
    }
    lv_image_set_rotation(bike_image, 3600 - bike_rotation);

    wheel_rpm += 1;
    battery_voltage -= 0.01f;
    pedal_rpm += 1;

    if (pedal_rpm % 3 == 0) {
        speed += 1;
    }

    lv_obj_set_style_text_color(pedal_rpm_label, rpm_color, LV_PART_MAIN);
    lv_obj_set_style_bg_color(battery_voltage_rect, battery_color, 0);

    #ifdef _MSC_VER
        Sleep(REFRESH_SLEEP_MS);
    #else
        usleep(REFRESH_SLEEP_MS * 1000);
    #endif
  }

  return 0;
}

#endif

#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <Helpers.h>
#include <deque>
#include <XPT2046_Touchscreen.h>
#include <Wire.h>

#define SDA_PIN 18
#define SCL_PIN 5

#define MARGIN 4
#define SIDE_MARGIN 8
#define PEDAL_HALL_EFFECT 27
#define WHEEL_HALL_EFFECT 16

#define T_CLK  25
#define T_CS   33
#define T_DIN  32
#define T_OUT  39
#define T_IRQ  36

#define TOUCH_X_MIN  250
#define TOUCH_X_MAX  3750
#define TOUCH_Y_MIN  320
#define TOUCH_Y_MAX  3900


using std::deque;

const int BUTTON_GOTO_SETTINGS = 1;
const int BUTTON_GOFROM_SETTINGS = 2;
const int SETTINGS_BTN_RESET_TRIP = 3;
const int SETTINGS_BTN_VIEW_ERRORS = 4;
const int SETTINGS_BTN_VIEW_GEARS = 5;
const int SETTINGS_BTN_GOTO_DEBUG = 6;

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
static lv_obj_t *gear_screen_view;
static lv_obj_t *debug_view;

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

static lv_obj_t *cpu_temp_label;
static lv_obj_t *cpu_util_label;
static lv_obj_t *memory_util_label;
static lv_obj_t *touch_screen_dot;
static lv_obj_t *front_sensor_dot;
static lv_obj_t *pedal_sensor_dot;


static float speed = 0.0;
static int pedal_rpm = 0;
static int wheel_rpm = 0;
static float battery_voltage = 0.0f;
static int distance_traveled = 0;

static int bike_rotation = 0;

static bool battery_warning = true;
static bool front_wheel_warning = false;
static bool pedal_warning = false;
static bool gear_warning = true;
static bool tilt_warning = false;

static bool splash_shown = false;

static long total_wheel_rotations = 0;

static lv_indev_t *indev;

TFT_eSPI tft = TFT_eSPI();
TaskHandle_t sensor_task;
SPIClass touchscreenSPI(VSPI);
XPT2046_Touchscreen ts(T_CS, T_IRQ);

static const uint16_t screenWidth = 240;
static const uint16_t screenHeight = 320;

static lv_display_t *display;
static lv_color_t buf[screenWidth * 20];

void ili_flush_cb(lv_display_t *disp,
                  const lv_area_t *area,
                  uint8_t *px_map)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushPixels((uint16_t *)px_map, w * h);
    tft.endWrite();

    lv_display_flush_ready(disp);
}

void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    if (ts.tirqTouched() && ts.touched()) {
        TS_Point p = ts.getPoint();
        data->state = LV_INDEV_STATE_PRESSED;

        // very weird behaviour do not touch pretty please
        data->point.x = map((4095 - p.y), TOUCH_X_MIN, TOUCH_X_MAX, 0, 240 - 1);
        data->point.y = map(p.x, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, 320 - 1);

        lv_obj_set_pos(touch_screen_dot, data->point.x, data->point.y);

        Serial.printf("%d, %d\n", data->point.x, data->point.y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void settings_callback(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    int button_action = *((int *)lv_event_get_user_data(event));

    Serial.printf("Received settings cb %d\n", button_action);


    if (button_action == BUTTON_GOTO_SETTINGS)
    {
        lv_scr_load(settings_view);
    }
    else if (button_action == BUTTON_GOFROM_SETTINGS)
    {
        lv_scr_load(dashboard_view);
    }
}

static void setting_change_callback(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    int button_action = (int)(intptr_t)lv_event_get_user_data(event);

    Serial.printf("Settings callback to %d\n", button_action);

    switch (button_action)
    {
    case SETTINGS_BTN_RESET_TRIP:
        distance_traveled = 0;
        break;

    case SETTINGS_BTN_VIEW_ERRORS:
        splash_shown = false;

        lv_obj_remove_flag(gear_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(orientation_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(pedal_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(battery_icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(fwheel_icon, LV_OBJ_FLAG_HIDDEN);

        lv_scr_load(splash_screen_view);
        lv_timer_handler();

        break;

    case SETTINGS_BTN_VIEW_GEARS:
        lv_scr_load(gear_screen_view);
        break;

    case SETTINGS_BTN_GOTO_DEBUG:
        lv_scr_load(debug_view);
        break;
    }
}

void create_dashboard()
{
    speed_label = lv_label_create(dashboard_view);
    lv_obj_set_style_text_font(speed_label, &lv_font_montserrat_48, 0);
    lv_obj_set_pos(speed_label, SIDE_MARGIN, MARGIN);

    gear_label = lv_label_create(dashboard_view);
    lv_label_set_text(gear_label, "4TH");
    lv_obj_set_style_text_font(gear_label, &lv_font_montserrat_48, 0);
    lv_obj_set_pos(gear_label, SIDE_MARGIN, 48);

    lv_obj_t *pedal_description = lv_label_create(dashboard_view);
    lv_label_set_text(pedal_description, "PEDALS");
    lv_obj_set_style_text_font(pedal_description, &lv_font_montserrat_28, 0);
    lv_obj_set_pos(pedal_description, SIDE_MARGIN, 140);

    pedal_rpm_label = lv_label_create(dashboard_view);
    lv_obj_set_style_text_font(pedal_rpm_label, &lv_font_montserrat_40, 0);
    lv_obj_set_pos(pedal_rpm_label, SIDE_MARGIN, 140 + 24);
    lv_obj_set_style_text_align(pedal_rpm_label, LV_TEXT_ALIGN_RIGHT, 0);

    shift_image = lv_image_create(dashboard_view);
    lv_obj_set_pos(shift_image, 200 - SIDE_MARGIN, 140 + 28);

    lv_obj_t *wheel_description = lv_label_create(dashboard_view);
    lv_label_set_text(wheel_description, "WHEEL");
    lv_obj_set_style_text_font(wheel_description, &lv_font_montserrat_28, 0);
    lv_obj_set_pos(wheel_description, SIDE_MARGIN, 220);

    front_rpm_label = lv_label_create(dashboard_view);
    lv_obj_set_style_text_font(front_rpm_label, &lv_font_montserrat_40, 0);
    lv_obj_set_pos(front_rpm_label, SIDE_MARGIN, 220 + 24);
    lv_obj_set_style_text_align(front_rpm_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(front_rpm_label, lv_color_hex(0x5142f5), LV_PART_MAIN);

    battery_voltage_label = lv_label_create(dashboard_view);
    lv_obj_set_style_text_font(battery_voltage_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(battery_voltage_label, 170, 52 + MARGIN);

    battery_voltage_rect = lv_obj_create(dashboard_view);
    lv_obj_set_size(battery_voltage_rect, 28, 52);
    lv_obj_set_pos(battery_voltage_rect, 240 - 24 - SIDE_MARGIN, MARGIN);
    lv_obj_set_style_bg_color(battery_voltage_rect, lv_color_hex(0XFF0000), 0);

    trip_distance_label = lv_label_create(dashboard_view);
    lv_obj_set_style_text_font(trip_distance_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(trip_distance_label, SIDE_MARGIN, 320 - 20 - MARGIN);

    bike_image = create_icon(&bikeicon, dashboard_view, 155, 90, 256);
    bike_rotation_label = lv_label_create(dashboard_view);
    lv_obj_set_style_text_font(bike_rotation_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(bike_rotation_label, 155, 90);

    lv_obj_t *settings_button = lv_button_create(dashboard_view);
    lv_obj_t *settings_button_label = lv_label_create(settings_button);
    lv_label_set_text(settings_button_label, "Settings");
    lv_obj_add_event_cb(settings_button, settings_callback, LV_EVENT_CLICKED, (void *)&BUTTON_GOTO_SETTINGS);
    lv_obj_set_align(settings_button, LV_ALIGN_BOTTOM_RIGHT);
}

void create_settings()
{
    lv_obj_t *settings_header = lv_label_create(settings_view);
    lv_label_set_text(settings_header, "Settings");
    lv_obj_set_style_text_font(settings_header, &lv_font_montserrat_28, 0);
    lv_obj_set_pos(settings_header, SIDE_MARGIN, MARGIN);

    lv_obj_t *settings_button = lv_button_create(settings_view);
    lv_obj_t *settings_button_label = lv_label_create(settings_button);
    lv_label_set_text(settings_button_label, "Back");
    lv_obj_add_event_cb(settings_button, settings_callback, LV_EVENT_CLICKED, (void *)&BUTTON_GOFROM_SETTINGS);
    lv_obj_set_align(settings_button, LV_ALIGN_BOTTOM_RIGHT);

    lv_obj_t *debug_settings_view = create_settings_container(settings_view, "Debug settings");
    create_settings_part(debug_settings_view, "Go to debug view", "Go", CLICK, setting_change_callback, SETTINGS_BTN_GOTO_DEBUG);


    lv_obj_t *trip_settings_view = create_settings_container(settings_view, "Trip settings");
    create_settings_part(trip_settings_view, "Reset current trip", "Reset", CLICK, setting_change_callback, -1);
    create_settings_part(trip_settings_view, "Reset trip B", "Reset", CLICK, setting_change_callback, -1);
    create_settings_part(trip_settings_view, "Reset trip C", "Reset", CLICK, setting_change_callback, -1);
    update_height(trip_settings_view);

    lv_obj_t *measurements_settings_view = create_settings_container(settings_view, "Measurements");
    create_settings_part(measurements_settings_view, "Height (cm)", "e.g. 160", NUMBER, setting_change_callback, -1);
    create_settings_part(measurements_settings_view, "Weight (kg)", "e.g. 70", NUMBER, setting_change_callback, -1);
    update_height(measurements_settings_view);

    lv_obj_t *sensor_settings_view = create_settings_container(settings_view, "Sensors");
    create_settings_part(sensor_settings_view, "Front wheel magnet count", "default: 1", NUMBER, setting_change_callback, -1);
    create_settings_part(sensor_settings_view, "Pedal magnet count", "default: 2", NUMBER, setting_change_callback, -1);
    create_settings_part(sensor_settings_view, "Speed calc timeframe (s)", "default: 5", NUMBER, setting_change_callback, -1);
    create_settings_part(sensor_settings_view, "Wheel size (in.)", "default: 29", NUMBER, setting_change_callback, -1);
    create_settings_part(sensor_settings_view, "Gear configuration", "Open", CLICK, setting_change_callback, SETTINGS_BTN_VIEW_GEARS);
    update_height(sensor_settings_view);

    lv_obj_t *findmy_settings_view = create_settings_container(settings_view, "FindMy");
    create_settings_part(findmy_settings_view, "Enable support for Apple's Find My -network", "", TRUEFALSE, setting_change_callback, -1);
    create_settings_part(findmy_settings_view, "Ping interval (wait for n seconds)", "How many seconds to wait between pings", NUMBER, setting_change_callback, -1);
    create_settings_part(findmy_settings_view, "Ping on boot", "", TRUEFALSE, setting_change_callback, -1);
    update_height(findmy_settings_view);

    lv_obj_t *storage_settings_view = create_settings_container(settings_view, "Storage");
    create_settings_part(storage_settings_view, "Wipe partitions", "Wipe", CLICK, setting_change_callback, -1);
    update_height(storage_settings_view);

    // create_settings_button(settings_view, "Reset trip A", "Reset", SIDE_MARGIN, 120, 2, setting_change_callback);
    // create_settings_button(settings_view, "Reset trip B", "Reset", SIDE_MARGIN, 180, 3, setting_change_callback);
    // create_settings_button(settings_view, "Reset trip C", "Reset", SIDE_MARGIN, 240, 4, setting_change_callback);
    // create_settings_button(settings_view, "Metric/Imperial", "Toggle", SIDE_MARGIN, 300, 4, setting_change_callback);
    // create_settings_button(settings_view, "Wipe data part.", "Wipe", SIDE_MARGIN, 360, 4, setting_change_callback);
    // create_settings_button(settings_view, "View error dash", "View", SIDE_MARGIN, 420, SETTINGS_BTN_VIEW_ERRORS, setting_change_callback);
}

void create_open_animation()
{
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

    lv_obj_t *label = create_label("BATT 3.3 V", splash_screen_view);

    battery_icon = create_icon(&battery, splash_screen_view, 32, 50, 512);
    gear_icon = create_icon(&gearsensor, splash_screen_view, 32 + 64 + MARGIN, 50, 512);
    fwheel_icon = create_icon(&fwheelsensor, splash_screen_view, 32 + 128 + MARGIN, 50, 512);
    pedal_icon = create_icon(&pedalsensor, splash_screen_view, 32, 50 + 64, 512);
    orientation_icon = create_icon(&orientationsensor, splash_screen_view, 32 + 64 + MARGIN, 50 + 64, 512);
}

void create_gear_configurator()
{
    lv_obj_t *back_button = create_settings_button(gear_screen_view, "", "Back", 0, 0, BUTTON_GOFROM_SETTINGS, settings_callback);
    lv_obj_set_align(back_button, LV_ALIGN_BOTTOM_RIGHT);

    lv_obj_t *guide = create_label("Switch to gear #1", gear_screen_view);
}

void create_debug_view() {
    cpu_temp_label = create_label("CPU temperature", debug_view);
    cpu_util_label = create_label("CPU utilization", debug_view);
    memory_util_label = create_label("Memory util", debug_view);
    touch_screen_dot = lv_obj_create(debug_view);
    lv_obj_set_style_bg_color(touch_screen_dot, lv_color_hex(0x000000), 0);
    lv_obj_set_size(touch_screen_dot, 20, 20);

    lv_obj_t *back_button = create_settings_button(gear_screen_view, "", "Back", 0, 0, BUTTON_GOTO_SETTINGS, settings_callback);

}

void create_ui()
{
    dashboard_view = lv_obj_create(NULL);
    settings_view = lv_obj_create(NULL);
    splash_screen_view = lv_obj_create(NULL);
    gear_screen_view = lv_obj_create(NULL);
    debug_view = lv_obj_create(NULL);

    lv_obj_set_layout(settings_view, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(settings_view, LV_FLEX_FLOW_COLUMN);

    lv_obj_set_layout(debug_view, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(debug_view, LV_FLEX_FLOW_COLUMN);

    create_dashboard();
    create_settings();
    create_open_animation();
    create_gear_configurator();
    create_debug_view();
}

// NOTE: This entire function is currently vibe coded.
// I will replace it at some point, but my diagnosis for why my own code
// didn't work is that the hall effect sensors themselves don't work properly
// You can see my original attempt in rotation.cxx
void sensor_loop(void *params) {
    Serial.println("Sensor loop started");

    std::deque<uint32_t> pedal_rotation_times;
    std::deque<float> wheel_rpm_samples;

    bool last_pedal_state = false;
    bool last_wheel_state = false;

    uint32_t last_pedal_pulse = 0;
    uint32_t last_wheel_pulse = 0;

    while (true) {
        uint32_t now = millis();

        bool pedal_state = digitalRead(PEDAL_HALL_EFFECT) == LOW;

        if (!last_pedal_state && pedal_state) {
            pedal_rotation_times.push_front(now);
            last_pedal_pulse = now;
        }

        last_pedal_state = pedal_state;

        while (!pedal_rotation_times.empty() &&
               now - pedal_rotation_times.back() > 4000) {
            pedal_rotation_times.pop_back();
        }

        if (pedal_rotation_times.size() >= 2) {
            uint32_t newest = pedal_rotation_times.front();
            uint32_t oldest = pedal_rotation_times.back();

            float duration_s = (newest - oldest) / 1000.0f;
            float revs = pedal_rotation_times.size() - 1;

            pedal_rpm = (revs / duration_s) * 60.0f;
        } else {
            pedal_rpm *= 0.95f;
        }

        bool wheel_state = digitalRead(WHEEL_HALL_EFFECT) == LOW;

        if (!last_wheel_state && wheel_state) {
            total_wheel_rotations++;
            distance_traveled = (73*PI*total_wheel_rotations)/100;

            if (last_wheel_pulse != 0) {
                uint32_t period_ms = now - last_wheel_pulse;

                if (period_ms > 0) {
                    float instant_rpm = 60000.0f / period_ms;

                    wheel_rpm_samples.push_front(instant_rpm);

                    while (wheel_rpm_samples.size() > 5) {
                        wheel_rpm_samples.pop_back();
                    }
                }
            }

            last_wheel_pulse = now;
        }

        last_wheel_state = wheel_state;

        if (last_wheel_pulse != 0 && now - last_wheel_pulse > 3000) {
            wheel_rpm = 0.0f;
            wheel_rpm_samples.clear(); // clear stale samples so they don't
            last_wheel_pulse = 0;      // corrupt the average on next spin-up;
                                       // resetting the sentinel means the first
                                       // new pulse re-anchors the timestamp
                                       // cleanly rather than measuring a 3s+ gap
        } else if (!wheel_rpm_samples.empty()) {
            float sum = 0.0f;

            for (float r : wheel_rpm_samples) {
                sum += r;
            }

            wheel_rpm = sum / wheel_rpm_samples.size();
        }

        speed = wheel_rpm * 730 * PI * 60 / 1000000;

        delay(20);
    }
}

void setup()
{
    Serial.begin(115200);

    pinMode(PEDAL_HALL_EFFECT, INPUT_PULLUP);
    
    Serial.println("Setup started");

    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);

    Serial.println("Backlight on");

    tft.begin();

    // initiate sick as fuck rainbow flashss
    tft.fillScreen(TFT_RED);
    tft.fillScreen(TFT_ORANGE);
    tft.fillScreen(TFT_YELLOW);
    tft.fillScreen(TFT_GREEN);
    tft.fillScreen(TFT_CYAN);
    tft.fillScreen(TFT_BLUE);
    tft.fillScreen(TFT_PURPLE);
    tft.fillScreen(TFT_PINK);
    tft.fillScreen(TFT_BLACK);
    delay(200);

    uint16_t id = tft.readcommand8(0xD3, 2) << 8 | tft.readcommand8(0xD3, 3);
    Serial.printf("TFT started. Display: %u\n", (unsigned int)id);

    lv_init();
    Serial.println("LVGL Initialized");

    display = lv_display_create(screenWidth, screenHeight);
    Serial.println("LVGL Display created");

    lv_display_set_buffers(
        display,
        buf,
        nullptr,
        sizeof(buf),
        LV_DISPLAY_RENDER_MODE_PARTIAL);


    Serial.println("LVGL Buffers set");

    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchpad_read);
    Serial.println("LVGL Touch init");

    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_flush_cb(display, ili_flush_cb);
    Serial.println("LVGL Set flush callback and color format");

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    Serial.println("LVGL Input device created");

    create_ui();
    Serial.println("UI created");

    touchscreenSPI.begin(T_CLK, T_OUT, T_DIN, T_CS);
    ts.begin(touchscreenSPI);
    ts.setRotation(1);

    lv_scr_load(splash_screen_view);
    Serial.println("Screen loaded, showing signs...");

    delay(1500);

    Serial.println("Showing errors...");

    lv_timer_handler();
    delay(2000);

    Serial.println("Done! Showing dashboard");
    lv_scr_load(dashboard_view);

    Serial.println("Creating sensor task...");
    xTaskCreatePinnedToCore(sensor_loop, "SensorTask", 4096, NULL, 1, &sensor_task, 0);
}

void loop()
{
    lv_tick_inc(13);
    lv_timer_handler();

    char speed_text[32];
    sprintf(speed_text, "%d KMH", (int)floor(speed));
    lv_label_set_text(speed_label, speed_text);

    char pedal_text[32];
    sprintf(pedal_text, "%d RPM", pedal_rpm);
    lv_label_set_text(pedal_rpm_label, pedal_text);

    char wheel_text[32];
    sprintf(wheel_text, "%d RPM", wheel_rpm);
    lv_label_set_text(front_rpm_label, wheel_text);

    battery_voltage = (analogRead(34)*2.0)/1000.0;

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

    lv_obj_set_style_text_color(pedal_rpm_label, rpm_color, LV_PART_MAIN);
    lv_obj_set_style_bg_color(battery_voltage_rect, battery_color, 0);

    delay(13);
}
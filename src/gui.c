#include "gui.h"

#include <inttypes.h>
#include <stdio.h>

#include "config.h"
#include "ssd1306.h"
#include "lvgl.h"
#include "input.h"
#include "ui_menu.h"
#include "memory_card.h"
#include "cardman.h"

#include "ui_theme_mono.h"

static ssd1306_t oled_disp = { .external_vcc = 0 };
/* Displays the line at the bottom for long pressing buttons */
static lv_obj_t *g_navbar, *g_progress_bar, *g_progress_text;

static lv_obj_t *scr_card_switch, *scr_main, *scr_menu, *scr_freepsxboot, *menu, *main_page;
static lv_style_t style_inv;
static lv_obj_t *scr_main_idx_lbl, *scr_main_channel_lbl;

static int have_oled;
static int switching_card;
static uint64_t switching_card_timeout;

#define COLOR_FG      lv_color_white()
#define COLOR_BG      lv_color_black()

static void flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    if (have_oled) {
        ssd1306_clear(&oled_disp);

        for(int y = area->y1; y <= area->y2; y++) {
            for(int x = area->x1; x <= area->x2; x++) {
                if (color_p->full)
                    ssd1306_draw_pixel(&oled_disp, x, y);
                color_p++;
            }
        }

        ssd1306_show(&oled_disp);
    }
    lv_disp_flush_ready(disp_drv);
}

static void keypad_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    int pressed;

    data->state = LV_INDEV_STATE_RELEASED;

    pressed = input_get_pressed();
    if (pressed) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = pressed;
    }
}

static void create_nav(void) {
    static lv_style_t style_line;
    lv_style_init(&style_line);
    lv_style_set_line_width(&style_line, 1);
    lv_style_set_line_color(&style_line, lv_palette_main(LV_PALETTE_BLUE));

    g_navbar = lv_line_create(lv_layer_top());
    lv_obj_add_style(g_navbar, &style_line, 0);
}

static void gui_tick(void) {
    static uint64_t prev_time;
    if (!prev_time)
        prev_time = time_us_64();
    uint64_t now_time = time_us_64();
    uint64_t diff_ms = (now_time - prev_time) / 1000;

    if (diff_ms) {
        prev_time += diff_ms * 1000;
        lv_tick_inc(diff_ms);
        lv_timer_handler();
    }
}

static void reload_card_cb(int progress) {
    static lv_point_t line_points[2] = { {0, DISPLAY_HEIGHT/2}, {0, DISPLAY_HEIGHT/2} };
    static int prev_progress;
    progress += 5;
    if (progress/5 == prev_progress/5)
        return;
    prev_progress = progress;
    line_points[1].x = DISPLAY_WIDTH * progress / 100;
    lv_line_set_points(g_progress_bar, line_points, 2);

    lv_label_set_text(g_progress_text, cardman_get_progress_text());

    gui_tick();
}

static void evt_scr_main(lv_event_t *event) {
    if (event->code == LV_EVENT_KEY) {
        uint32_t key = lv_indev_get_key(lv_indev_get_act());
        printf("main screen got key %d\n", (int)key);
        if (key == INPUT_KEY_MENU) {
            printf("activate menu!\n");
            lv_scr_load(scr_menu);
            ui_menu_set_page(menu, NULL);
            ui_menu_set_page(menu, main_page);
            lv_obj_t *first = lv_obj_get_child(main_page, 0);
            lv_group_focus_obj(first);
            lv_event_stop_bubbling(event);
        }

        // TODO: if there was a card op recently (1s timeout?), should refuse to switch
        if (key == INPUT_KEY_PREV || key == INPUT_KEY_NEXT || key == INPUT_KEY_BACK || key == INPUT_KEY_ENTER) {
            memory_card_exit();
            cardman_close();

            switch (key) {
            case INPUT_KEY_PREV:
                cardman_prev_channel();
                break;
            case INPUT_KEY_NEXT:
                cardman_next_channel();
                break;
            case INPUT_KEY_BACK:
                cardman_prev_idx();
                break;
            case INPUT_KEY_ENTER:
                cardman_next_idx();
                break;
            }

            printf("new card=%d chan=%d\n", cardman_get_idx(), cardman_get_channel());
            switching_card = 1;
            switching_card_timeout = time_us_64() + 1500 * 1000;
        }

    }
}

static void evt_scr_freepsxboot(lv_event_t *event) {
    if (event->code == LV_EVENT_KEY) {
        // uint32_t key = lv_indev_get_key(lv_indev_get_act());
        UI_GOTO_SCREEN(scr_main);
        lv_event_stop_bubbling(event);
    }
}

static void evt_scr_menu(lv_event_t *event) {
    if (event->code == LV_EVENT_KEY) {
        uint32_t key = lv_indev_get_key(lv_indev_get_act());
        printf("menu screen got key %d\n", (int)key);
        if (key == INPUT_KEY_BACK || key == INPUT_KEY_MENU) {
            UI_GOTO_SCREEN(scr_main);
            lv_event_stop_bubbling(event);
        }
    }
}

static void evt_menu_page(lv_event_t *event) {
    if (event->code == LV_EVENT_KEY) {
        lv_obj_t *page = event->user_data;
        uint32_t key = lv_indev_get_key(lv_indev_get_act());
        lv_obj_t *cur = lv_group_get_focused(lv_group_get_default());
        if (lv_obj_get_parent(cur) != page)
            return;
        uint32_t idx = lv_obj_get_index(cur);
        uint32_t count = lv_obj_get_child_cnt(page);
        if (key == INPUT_KEY_NEXT) {
            lv_obj_t *next = lv_obj_get_child(page, (idx + 1) % count);
            lv_group_focus_obj(next);
            lv_event_stop_bubbling(event);
        } else if (key == INPUT_KEY_PREV) {
            lv_obj_t *prev = lv_obj_get_child(page, (idx + count - 1) % count);
            lv_group_focus_obj(prev);
            lv_event_stop_bubbling(event);
        } else if (key == INPUT_KEY_ENTER) {
            lv_event_send(cur, LV_EVENT_CLICKED, NULL);
            lv_event_stop_bubbling(event);
        } else if (key == INPUT_KEY_BACK) {
            /* going back from the root page - let it handle in evt_scr_menu */
            if (ui_menu_get_cur_main_page(menu) == main_page)
                return;
            lv_obj_t *back_btn = ui_menu_get_main_header_back_btn(menu);
            lv_event_send(back_btn, LV_EVENT_CLICKED, NULL);
            lv_event_stop_bubbling(event);
        }
    }
}

static void create_main_screen(void) {
    /* Main screen listing current memcard, status, etc */
    scr_main = ui_scr_create();
    lv_obj_add_event_cb(scr_main, evt_scr_main, LV_EVENT_ALL, NULL);

    lv_obj_t *lbl = lv_label_create(scr_main);
    lv_obj_set_align(lbl, LV_ALIGN_TOP_MID);
    lv_obj_add_style(lbl, &style_inv, 0);
    lv_obj_set_width(lbl, 128);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl, "PS2 Memory Card");

    lbl = lv_label_create(scr_main);
    lv_obj_set_align(lbl, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(lbl, 0, 24);
    lv_label_set_text(lbl, "Card");

    lbl = lv_label_create(scr_main);
    lv_obj_set_align(lbl, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_pos(lbl, 0, 24);
    lv_label_set_text(lbl, "");
    scr_main_idx_lbl = lbl;

    lbl = lv_label_create(scr_main);
    lv_obj_set_align(lbl, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(lbl, 0, 32);
    lv_label_set_text(lbl, "Channel");

    lbl = lv_label_create(scr_main);
    lv_obj_set_align(lbl, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_pos(lbl, 0, 32);
    lv_label_set_text(lbl, "");
    scr_main_channel_lbl = lbl;

    // lbl = lv_label_create(scr_main);
    // lv_obj_set_align(lbl, LV_ALIGN_TOP_LEFT);
    // lv_obj_set_pos(lbl, 0, 40);
    // lv_label_set_text(lbl, "Very long game title goes here");
    // lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    // lv_obj_set_width(lbl, 128);

    lbl = lv_label_create(scr_main);
    lv_obj_set_align(lbl, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_set_pos(lbl, 0, -2);
    lv_label_set_text(lbl, "<");
    lv_obj_add_style(lbl, &style_inv, 0);

    lbl = lv_label_create(scr_main);
    lv_obj_set_align(lbl, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_pos(lbl, 0, -2);
    lv_label_set_text(lbl, "Menu");
    lv_obj_set_width(lbl, 4 * 8 + 2);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(lbl, &style_inv, 0);

    lbl = lv_label_create(scr_main);
    lv_obj_set_align(lbl, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_set_pos(lbl, 0, -2);
    lv_label_set_text(lbl, ">");
    lv_obj_add_style(lbl, &style_inv, 0);
}

static void create_freepsxboot_screen(void) {
    scr_freepsxboot = ui_scr_create();
    lv_obj_add_event_cb(scr_freepsxboot, evt_scr_freepsxboot, LV_EVENT_ALL, NULL);

    lv_obj_t *lbl = lv_label_create(scr_freepsxboot);
    lv_obj_set_align(lbl, LV_ALIGN_TOP_MID);
    lv_obj_add_style(lbl, &style_inv, 0);
    lv_obj_set_width(lbl, 128);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl, "FreePSXBoot");

    lbl = lv_label_create(scr_freepsxboot);
    lv_obj_set_align(lbl, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(lbl, 0, 24);
    lv_label_set_text(lbl, "Model");

    lbl = lv_label_create(scr_freepsxboot);
    lv_obj_set_align(lbl, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_pos(lbl, 0, 24);
    lv_label_set_text(lbl, "1001v3");

    lbl = lv_label_create(scr_freepsxboot);
    lv_obj_set_align(lbl, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(lbl, 0, 32);
    lv_label_set_text(lbl, "Slot");

    lbl = lv_label_create(scr_freepsxboot);
    lv_obj_set_align(lbl, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_pos(lbl, 0, 32);
    lv_label_set_text(lbl, "Slot 2");

    lbl = lv_label_create(scr_freepsxboot);
    lv_obj_set_align(lbl, LV_ALIGN_BOTTOM_MID);
    lv_label_set_text(lbl, "Press any button to deactivate FreePSXBoot and return to the Memory Card mode");
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(lbl, 128);
}

static void create_cardswitch_screen(void) {
    scr_card_switch = ui_scr_create();

    lv_obj_t *lbl = lv_label_create(scr_card_switch);
    lv_obj_set_align(lbl, LV_ALIGN_TOP_MID);
    lv_obj_add_style(lbl, &style_inv, 0);
    lv_obj_set_width(lbl, 128);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl, "Loading card");

    static lv_style_t style_progress;
    lv_style_init(&style_progress);
    lv_style_set_line_width(&style_progress, 12);
    lv_style_set_line_color(&style_progress, lv_palette_main(LV_PALETTE_BLUE));

    g_progress_bar = lv_line_create(scr_card_switch);
    lv_obj_set_width(g_progress_bar, DISPLAY_WIDTH);
    lv_obj_add_style(g_progress_bar, &style_progress, 0);

    g_progress_text = lv_label_create(scr_card_switch);
    lv_obj_set_align(g_progress_text, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(g_progress_text, 0, DISPLAY_HEIGHT-9);
    lv_label_set_text(g_progress_text, "Read XXX kB/s");
}

static void create_menu_screen(void) {
    /* Menu screen accessible by pressing the menu button at main */
    scr_menu = ui_scr_create();
    lv_obj_add_event_cb(scr_menu, evt_scr_menu, LV_EVENT_ALL, NULL);

    menu = ui_menu_create(scr_menu);
    lv_obj_add_flag(menu, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_size(menu, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL) - 2);

    lv_obj_t *cont;
    lv_obj_t *label;

    lv_obj_t *freepsxboot_page = ui_menu_page_create(menu, "FreePSXBoot");
    lv_obj_add_flag(freepsxboot_page, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_group_add_obj(lv_group_get_default(), freepsxboot_page);
    lv_obj_add_event_cb(freepsxboot_page, evt_menu_page, LV_EVENT_ALL, freepsxboot_page);

    cont = ui_menu_cont_create(freepsxboot_page);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_group_add_obj(lv_group_get_default(), cont);
    label = lv_label_create(cont);
    lv_obj_set_flex_grow(label, 1);
    lv_label_set_text(label, "Enable");
    label = lv_label_create(cont);
    lv_label_set_text(label, "Yes");

    cont = ui_menu_cont_create(freepsxboot_page);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_group_add_obj(lv_group_get_default(), cont);
    label = lv_label_create(cont);
    lv_obj_set_flex_grow(label, 1);
    lv_label_set_text(label, "Model");
    label = lv_label_create(cont);
    lv_label_set_text(label, "1001v3");

    cont = ui_menu_cont_create(freepsxboot_page);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_group_add_obj(lv_group_get_default(), cont);
    label = lv_label_create(cont);
    lv_obj_set_flex_grow(label, 1);
    lv_label_set_text(label, "Slot");
    label = lv_label_create(cont);
    lv_label_set_text(label, "Slot 1");

    lv_obj_t *display_page = ui_menu_page_create(menu, "Display");
    lv_obj_add_flag(display_page, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_group_add_obj(lv_group_get_default(), display_page);
    lv_obj_add_event_cb(display_page, evt_menu_page, LV_EVENT_ALL, display_page);

    cont = ui_menu_cont_create(display_page);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_group_add_obj(lv_group_get_default(), cont);
    label = lv_label_create(cont);
    lv_obj_set_flex_grow(label, 1);
    lv_label_set_text(label, "Auto off");
    label = lv_label_create(cont);
    lv_label_set_text(label, "30s");

    /* Main menu page */
    main_page = ui_menu_page_create(menu, NULL);
    lv_obj_add_flag(main_page, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_group_add_obj(lv_group_get_default(), main_page);
    lv_obj_add_event_cb(main_page, evt_menu_page, LV_EVENT_ALL, main_page);

    cont = ui_menu_cont_create(main_page);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_group_add_obj(lv_group_get_default(), cont);
    label = lv_label_create(cont);
    lv_obj_set_flex_grow(label, 1);
    lv_label_set_text(label, "FreePSXBoot");
    label = lv_label_create(cont);
    lv_label_set_text(label, ">");
    ui_menu_set_load_page_event(menu, cont, freepsxboot_page);

    cont = ui_menu_cont_create(main_page);
    lv_obj_add_flag(cont, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_group_add_obj(lv_group_get_default(), cont);
    label = lv_label_create(cont);
    lv_obj_set_flex_grow(label, 1);
    lv_label_set_text(label, "Display");
    label = lv_label_create(cont);
    lv_label_set_text(label, ">");
    ui_menu_set_load_page_event(menu, cont, display_page);

    ui_menu_set_page(menu, main_page);
}

static void create_ui(void) {
    lv_style_init(&style_inv);
    lv_style_set_bg_opa(&style_inv, LV_OPA_COVER);
    lv_style_set_bg_color(&style_inv, COLOR_FG);
    lv_style_set_border_color(&style_inv, COLOR_BG);
    lv_style_set_line_color(&style_inv, COLOR_BG);
    lv_style_set_arc_color(&style_inv, COLOR_BG);
    lv_style_set_text_color(&style_inv, COLOR_BG);
    lv_style_set_outline_color(&style_inv, COLOR_BG);

    create_nav();
    create_main_screen();
    create_menu_screen();
    create_cardswitch_screen();
    create_freepsxboot_screen();

    /* start at the main screen - TODO - or freepsxboot */
    // UI_GOTO_SCREEN(scr_freepsxboot);
    UI_GOTO_SCREEN(scr_main);
}

void gui_init(void) {
    i2c_init(OLED_I2C_PERIPH, OLED_I2C_CLOCK);
    gpio_set_function(OLED_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(OLED_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_I2C_SDA);
    gpio_pull_up(OLED_I2C_SCL);

    have_oled = ssd1306_init(&oled_disp, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0x3C, OLED_I2C_PERIPH);

    lv_init();

    static lv_disp_draw_buf_t disp_buf;
    static lv_color_t buf_1[DISPLAY_WIDTH * DISPLAY_HEIGHT];
    lv_disp_draw_buf_init(&disp_buf, buf_1, NULL, DISPLAY_WIDTH * DISPLAY_HEIGHT);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = flush_cb;
    disp_drv.hor_res = DISPLAY_WIDTH;
    disp_drv.ver_res = DISPLAY_HEIGHT;
    disp_drv.direct_mode = disp_drv.full_refresh = 1;

    lv_disp_t *disp;
    disp = lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = keypad_read;

    lv_indev_t *indev = lv_indev_drv_register(&indev_drv);
    lv_group_t *g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(indev, g);

    lv_theme_t *th = ui_theme_mono_init(disp, 1, LV_FONT_DEFAULT);
    lv_disp_set_theme(disp, th);

    create_ui();
}

void gui_do_card_switch(void) {
    printf("switching the card now!\n");
    UI_GOTO_SCREEN(scr_card_switch);

    uint64_t start = time_us_64();
    cardman_set_progress_cb(reload_card_cb);
    cardman_open();
    cardman_set_progress_cb(NULL);
    memory_card_enter();
    uint64_t end = time_us_64();
    printf("full card switch took = %.2f s\n", (end - start) / 1e6);

    UI_GOTO_SCREEN(scr_main);

    input_flush();
}

void gui_task(void) {
    input_update_display(g_navbar);

    static int displayed_card_idx = -1;
    static int displayed_card_channel = -1;
    static char card_idx_s[8];
    static char card_channel_s[8];
    if (displayed_card_idx != cardman_get_idx() || displayed_card_channel != cardman_get_channel()) {
        displayed_card_idx = cardman_get_idx();
        displayed_card_channel = cardman_get_channel();
        snprintf(card_idx_s, sizeof(card_idx_s), "%d", displayed_card_idx);
        snprintf(card_channel_s, sizeof(card_channel_s), "%d", displayed_card_channel);
        lv_label_set_text(scr_main_idx_lbl, card_idx_s);
        lv_label_set_text(scr_main_channel_lbl, card_channel_s);
    }

    if (switching_card && switching_card_timeout < time_us_64()) {
        switching_card = 0;
        gui_do_card_switch();
    }

    gui_tick();
}

#include "zrh_layout.h"

ZRH_LayoutConfig g_layout;

/* Logging function declaration from zumareshandler.dll.c */
void LogF(const char *fmt, ...);

void ZRH_InitLayoutDefaults(void) {
    memset(&g_layout, 0, sizeof(g_layout));
    g_layout.valid = 1;

    /* Adventure Temples */
    g_layout.adv_back_x     = 0;   g_layout.adv_back_y     = 0;
    g_layout.adv_temple1_x  = 0;   g_layout.adv_temple1_y  = 188;
    g_layout.adv_temple2_x  = 0;   g_layout.adv_temple2_y  = 112;
    g_layout.adv_temple2v_x = 0;   g_layout.adv_temple2v_y = 112;
    g_layout.adv_temple3_x  = 395; g_layout.adv_temple3_y  = 125;
    g_layout.adv_temple3v_x = 395; g_layout.adv_temple3v_y = 125;
    g_layout.adv_temple4_x  = 174; g_layout.adv_temple4_y  = 1;
    g_layout.adv_temple5_x  = 126; g_layout.adv_temple5_y  = 148; /* Secret Temple 5: The Void */
    g_layout.adv_middle_x   = 0;   g_layout.adv_middle_y   = 72;

    /* Adventure Doors (0..12)
     * Doors 0..11: Temples 1..4
     * Door 12: Secret Temple 5 Door 5 (Stage 13)
     */
    g_layout.adv_doors[0].x1  = 226; g_layout.adv_doors[0].y1  = 300; g_layout.adv_doors[0].x2  = 256; g_layout.adv_doors[0].y2  = 361;
    g_layout.adv_doors[1].x1  = 296; g_layout.adv_doors[1].y1  = 270; g_layout.adv_doors[1].x2  = 327; g_layout.adv_doors[1].y2  = 364;
    g_layout.adv_doors[2].x1  = 366; g_layout.adv_doors[2].y1  = 300; g_layout.adv_doors[2].x2  = 398; g_layout.adv_doors[2].y2  = 360;
    g_layout.adv_doors[3].x1  = 0;   g_layout.adv_doors[3].y1  = 164; g_layout.adv_doors[3].x2  = 56;  g_layout.adv_doors[3].y2  = 218;
    g_layout.adv_doors[4].x1  = 69;  g_layout.adv_doors[4].y1  = 159; g_layout.adv_doors[4].x2  = 116; g_layout.adv_doors[4].y2  = 201;
    g_layout.adv_doors[5].x1  = 120; g_layout.adv_doors[5].y1  = 155; g_layout.adv_doors[5].x2  = 167; g_layout.adv_doors[5].y2  = 188;
    g_layout.adv_doors[6].x1  = 452; g_layout.adv_doors[6].y1  = 154; g_layout.adv_doors[6].x2  = 462; g_layout.adv_doors[6].y2  = 188;
    g_layout.adv_doors[7].x1  = 509; g_layout.adv_doors[7].y1  = 154; g_layout.adv_doors[7].x2  = 521; g_layout.adv_doors[7].y2  = 192;
    g_layout.adv_doors[8].x1  = 568; g_layout.adv_doors[8].y1  = 154; g_layout.adv_doors[8].x2  = 584; g_layout.adv_doors[8].y2  = 196;
    g_layout.adv_doors[9].x1  = 220; g_layout.adv_doors[9].y1  = 35;  g_layout.adv_doors[9].x2  = 248; g_layout.adv_doors[9].y2  = 86;
    g_layout.adv_doors[10].x1 = 291; g_layout.adv_doors[10].y1 = 35;  g_layout.adv_doors[10].x2 = 324; g_layout.adv_doors[10].y2 = 86;
    g_layout.adv_doors[11].x1 = 369; g_layout.adv_doors[11].y1 = 35;  g_layout.adv_doors[11].x2 = 408; g_layout.adv_doors[11].y2 = 85;
    g_layout.adv_doors[12].x1 = 291; g_layout.adv_doors[12].y1 = 176; g_layout.adv_doors[12].x2 = 323; g_layout.adv_doors[12].y2 = 201;

    /* Adventure Navigation Buttons */
    g_layout.adv_btn_mainmenu_x = 0;   g_layout.adv_btn_mainmenu_y = 438;
    g_layout.adv_btn_play_x     = 542; g_layout.adv_btn_play_y     = 441;

    /* Adventure Labels & Highscores */
    g_layout.adv_title_x           = 0;   g_layout.adv_title_y      = 0;
    g_layout.adv_hs_title_x        = 456; g_layout.adv_hs_title_y   = 0;
    g_layout.adv_bottom_box_x      = 289; g_layout.adv_bottom_box_y = 382;
    g_layout.adv_stage_num_x       = 344; g_layout.adv_stage_num_y  = 396;
    g_layout.adv_stage_text_y      = 422; g_layout.adv_stage_center_x = 326;
    g_layout.adv_subtitle_y        = 456; g_layout.adv_subtitle_center_x = 326;
    g_layout.adv_hs_scores_x       = 575;
    g_layout.adv_hs_date_x         = 567;
    g_layout.adv_hs_names_x        = 470;
    g_layout.adv_hs_base_y         = 70;
    g_layout.adv_hs_spacing        = 18;
    g_layout.adv_hs_clip_w         = 567;
    g_layout.adv_infobox_x         = 20;  g_layout.adv_infobox_y    = 60;
    g_layout.adv_infobox_w         = 150; g_layout.adv_infobox_h    = 100;
    g_layout.adv_infobox_line_spacing = 14;

    /* Secret Stage 13 Banner */
    g_layout.adv_s13_badge_x = 289; g_layout.adv_s13_badge_y = 382;
    g_layout.adv_s13_num_x   = 344; g_layout.adv_s13_num_y   = 396;
    g_layout.adv_s13_title_x = 326; g_layout.adv_s13_title_y = 422;
    g_layout.adv_s13_sub_x   = 326; g_layout.adv_s13_sub_y   = 456;

    /* Gauntlet Map Doors (0..3) */
    g_layout.gaunt_doors[0].x = 209; g_layout.gaunt_doors[0].y = 179;
    g_layout.gaunt_doors[1].x = 232; g_layout.gaunt_doors[1].y = 129;
    g_layout.gaunt_doors[2].x = 249; g_layout.gaunt_doors[2].y = 82;
    g_layout.gaunt_doors[3].x = 265; g_layout.gaunt_doors[3].y = 28;

    /* Gauntlet Navigation Buttons (640x480 canonical defaults) */
    g_layout.gaunt_btn_back_x     = 177; g_layout.gaunt_btn_back_y     = 444;
    g_layout.gaunt_btn_play_x     = 277; g_layout.gaunt_btn_play_y     = 444;
    g_layout.gaunt_btn_next_x     = 390; g_layout.gaunt_btn_next_y     = 444;
    g_layout.gaunt_btn_mainmenu_x = 10;  g_layout.gaunt_btn_mainmenu_y = 422;
    g_layout.gaunt_btn_survival_x = 506; g_layout.gaunt_btn_survival_y = 413;
    g_layout.gaunt_btn_practice_x = 505; g_layout.gaunt_btn_practice_y = 446;

    /* Gauntlet Labels & Highscores */
    g_layout.gaunt_title_x         = 0;   g_layout.gaunt_title_y        = 0;
    g_layout.gaunt_door_frame_x    = 170; g_layout.gaunt_door_frame_y   = 220;
    g_layout.gaunt_hs_title_x      = 434; g_layout.gaunt_hs_title_y     = 0;
    g_layout.gaunt_door_name_x     = 170; g_layout.gaunt_door_name_y    = 427;
    g_layout.gaunt_practice_text_x = 150; g_layout.gaunt_practice_text_y = 260;
    g_layout.gaunt_practice_text_w = 320;
    g_layout.gaunt_custom_map_x    = 190; g_layout.gaunt_custom_map_y   = 260; /* 190 inside doorway */
    g_layout.gaunt_gem_x           = 560; g_layout.gaunt_gem_y          = 220;
    g_layout.gaunt_hs_scores_x     = 575;
    g_layout.gaunt_hs_names_x      = 445;
    g_layout.gaunt_hs_base_y       = 70;
    g_layout.gaunt_hs_spacing      = 18;
    g_layout.gaunt_hs_clip_w       = 567;
    g_layout.gaunt_infobox_x       = 20;  g_layout.gaunt_infobox_y      = 60;
    g_layout.gaunt_infobox_w       = 150; g_layout.gaunt_infobox_h      = 100;
    g_layout.gaunt_infobox_line_spacing = 14;

    /* Main Menu Buttons & Frog Elements (640x480 canonical coordinates) */
    g_layout.mm_btn1_x     = 452; g_layout.mm_btn1_y     = 64;
    g_layout.mm_btn2_x     = 436; g_layout.mm_btn2_y     = 153;
    g_layout.mm_btn3_x     = 418; g_layout.mm_btn3_y     = 236;
    g_layout.mm_btn4_x     = 394; g_layout.mm_btn4_y     = 305;
    g_layout.mm_btn5_x     = 496; g_layout.mm_btn5_y     = 315;
    g_layout.mm_eyes_x     = 191; g_layout.mm_eyes_y     = 331;
    g_layout.mm_hat_x      = 188; g_layout.mm_hat_y      = 247;
    g_layout.mm_sun_x      = -135; g_layout.mm_sun_y     = -140;
    g_layout.mm_sun_face_x = 0;   g_layout.mm_sun_face_y = 0;
    g_layout.mm_eye_ray1_x = 29;  g_layout.mm_eye_ray1_y = 25;
    g_layout.mm_eye_ray2_x = 87;  g_layout.mm_eye_ray2_y = 16;
    g_layout.mm_eye_idle1_x= 219; g_layout.mm_eye_idle1_y= 368;
    g_layout.mm_eye_idle2_x= 276; g_layout.mm_eye_idle2_y= 355;
    g_layout.mm_welcome_y     = 2;
    g_layout.mm_user_switch_y = 43;
    g_layout.mm_space_w       = 0;
    g_layout.mm_space_h       = 0;

    /* More Games Quadrants */
    g_layout.mg_t0_x = 58;  g_layout.mg_t0_y = 107; g_layout.mg_t0_w = 250; g_layout.mg_t0_h = 150;
    g_layout.mg_t1_x = 330; g_layout.mg_t1_y = 107; g_layout.mg_t1_w = 250; g_layout.mg_t1_h = 150;
    g_layout.mg_t2_x = 58;  g_layout.mg_t2_y = 277; g_layout.mg_t2_w = 250; g_layout.mg_t2_h = 150;
    g_layout.mg_t3_x = 330; g_layout.mg_t3_y = 277; g_layout.mg_t3_w = 250; g_layout.mg_t3_h = 150;
    g_layout.mg_btn_back_x = 164; g_layout.mg_btn_back_y = 445;
    g_layout.mg_space_w = 0;
    g_layout.mg_space_h = 0;

    /* Loading Screen (640x480 canonical coordinates) */
    g_layout.load_bar_x        = 128;
    g_layout.load_bar_y        = 349;
    g_layout.load_click_play_y = 430;
    g_layout.load_space_w      = 0;
    g_layout.load_space_h      = 0;

    /* Adventure Coordinate Spaces (default 0 for legacy canonical 640x480) */
    g_layout.adv_temples_space_w = 0;
    g_layout.adv_temples_space_h = 0;
    g_layout.adv_doors_space_w   = 0;
    g_layout.adv_doors_space_h   = 0;
    g_layout.adv_buttons_space_w = 0;
    g_layout.adv_buttons_space_h = 0;
    g_layout.adv_labels_space_w  = 0;
    g_layout.adv_labels_space_h  = 0;
    g_layout.gaunt_doors_space_w   = 0;
    g_layout.gaunt_doors_space_h   = 0;
    g_layout.gaunt_buttons_space_w = 0;
    g_layout.gaunt_buttons_space_h = 0;
    g_layout.gaunt_labels_space_w  = 0;
    g_layout.gaunt_labels_space_h  = 0;

    /* In-Game HUD & Banners */
    g_layout.hud_level_banner_offset_x = 40;
    g_layout.hud_level_banner_offset_y = 40;
    g_layout.hud_lives_banner_center_x = 320;
    g_layout.hud_lives_banner_center_y = 240;

    /* In-Game Stone Menubar Elements */
    g_layout.hud_menubar_lives_x       = 28;
    g_layout.hud_menubar_stage_cx      = 200;
    g_layout.hud_menubar_score_rx      = 360;
    g_layout.hud_menubar_zuma_x        = 409;
    g_layout.hud_menubar_menu_x        = 540;
    g_layout.hud_menubar_margin_bottom = 100;
    g_layout.hud_menubar_margin_top    = 50;
    g_layout.hud_menubar_margin_right  = 150;
    g_layout.hud_menubar_margin_left   = 200;
    g_layout.hud_lives_y       = 6;
    g_layout.hud_lives_spacing = 26;
    g_layout.hud_stage_y       = 8;
    g_layout.hud_score_y       = 8;
    g_layout.hud_zuma_y        = 2;
    g_layout.hud_zuma_w        = 178;
    g_layout.hud_zuma_h        = 24;
    g_layout.hud_menu_y        = 4;
    g_layout.hud_menu_w        = 67;
    g_layout.hud_menu_h        = 27;

    /* Stage Complete Cutscene */
    g_layout.stage_complete_text_x              = 320;
    g_layout.stage_complete_text_y              = 150;
    g_layout.stage_complete_text_align          = 2;
    g_layout.stage_complete_banner_x            = 320;
    g_layout.stage_complete_banner_y            = 250;
    g_layout.stage_complete_banner_align        = 2;
    g_layout.stage_complete_frog_writing_y      = 250;
    g_layout.stage_complete_frog_writing_align  = 2;
    g_layout.stage_complete_frog_target_x       = 320;
    g_layout.stage_complete_frog_target_y       = 200;
    g_layout.stage_complete_frog_target_align   = 5;
    g_layout.stage_complete_frog_bx             = 150;
    g_layout.stage_complete_frog_cx             = 500;
    g_layout.stage_complete_frog_dx             = 320;
    g_layout.stage_complete_frog_waypoint_y     = 250;
    g_layout.stage_complete_frog_waypoints_align= 5;

    /* Temple Cleared (Congratulations Dialog) */
    g_layout.congrats_btn_continue_x   = 410;
    g_layout.congrats_btn_continue_y   = 450;
    g_layout.congrats_title_x          = 320;
    g_layout.congrats_title_y          = 120;
    g_layout.congrats_tiki_x           = 50;
    g_layout.congrats_tiki_y           = 160;
    g_layout.congrats_speech_x         = 220;
    g_layout.congrats_speech_y         = 180;
    g_layout.congrats_speech_w         = 380;
    g_layout.congrats_speech_h         = 150;
    g_layout.congrats_frog_medallion_x = 120;
    g_layout.congrats_frog_medallion_y = 380;
    g_layout.congrats_frog_reply_x     = 200;
    g_layout.congrats_frog_reply_y     = 280;

    /* Credits Screen */
    g_layout.credits_btn_whatever_offset_x = 130;
    g_layout.credits_text_center_x         = 320;
    g_layout.credits_scroll_start_y        = 480;

    /* Help / Instructions Screen */
    g_layout.help_back_x        = 11;
    g_layout.help_back_y        = 51;
    g_layout.help_skull_x       = 65;
    g_layout.help_skull_y       = 20;
    g_layout.help_coin_x        = 282;
    g_layout.help_coin_y        = 45;
    g_layout.help_frog_x        = 500;
    g_layout.help_frog_y        = 65;
    g_layout.help_w1_top_text_x = 30;
    g_layout.help_w1_top_text_y = 40;
    g_layout.help_w2_top_text_x = 8;
    g_layout.help_w2_top_text_y = 40;
    g_layout.help_swap_click_x  = 560;
    g_layout.help_swap_click_y  = 55;
    g_layout.help_swap_mouse_x  = 580;
    g_layout.help_swap_mouse_y  = 60;
    g_layout.help_ball_target_x = 240;
    g_layout.help_ball_target_y = 160;
    g_layout.help_dev_temple5_x = 225;
    g_layout.help_dev_temple5_y = 114;
    g_layout.help_dev_mouse_m_x = 465;
    g_layout.help_dev_mouse_m_y = 145;
    g_layout.help_checkbox_x    = 14;
    g_layout.help_checkbox_y    = 430;
    g_layout.help_done_btn_x    = 450;
    g_layout.help_done_btn_y    = 430;
    g_layout.help_done_btn_w    = 180;
    g_layout.help_done_btn_h    = 44;

    /* Universal Dialog Centering */
    g_layout.dialog_min_top = 10;
    g_layout.dialog_align = 5;

    /* Alignments & Anchor Adjustments */
    g_layout.adv_btn_mainmenu_align = 7;
    g_layout.adv_btn_play_align     = 9;
    g_layout.gaunt_btn_back_align     = 8;
    g_layout.gaunt_btn_play_align     = 8;
    g_layout.gaunt_btn_next_align     = 8;
    g_layout.gaunt_btn_mainmenu_align = 7;
    g_layout.gaunt_btn_survival_align = 9;
    g_layout.gaunt_btn_practice_align = 9;
    g_layout.hud_level_banner_align   = 9;
    g_layout.hud_lives_banner_align   = 5;
    g_layout.hud_menubar_align        = 2;
    g_layout.credits_whatever_align   = 9;
    g_layout.credits_text_align       = 2;
    g_layout.congrats_btn_align            = 5;
    g_layout.congrats_title_align          = 5;
    g_layout.congrats_tiki_align          = 5;
    g_layout.congrats_speech_align        = 5;
    g_layout.congrats_frog_medallion_align = 5;
    g_layout.congrats_frog_reply_align    = 5;

    /* DevTester HelpScreen (Tab 3) Full Visual & Layout Specifications */
    g_layout.dev_help_title_x        = 320;
    g_layout.dev_help_title_y        = 28;
    g_layout.dev_help_title_align    = 2;
    g_layout.dev_help_subtitle_x     = 320;
    g_layout.dev_help_subtitle_y     = 78;
    g_layout.dev_help_subtitle_align = 2;
    g_layout.dev_help_col1_x         = 162;
    g_layout.dev_help_col2_x         = 478;
    g_layout.dev_help_adv_header_y   = 108;
    g_layout.dev_help_adv_header_align = 2;
    g_layout.dev_help_temple5_align  = 2;
    g_layout.dev_help_adv_text_x     = 30;
    g_layout.dev_help_adv_text_y     = 206;
    g_layout.dev_help_adv_text_spacing = 18;
    g_layout.dev_help_adv_text_align = 1;
    g_layout.dev_help_gaunt_header_y = 108;
    g_layout.dev_help_gaunt_header_align = 2;
    g_layout.dev_help_gaunt_text_x   = 343;
    g_layout.dev_help_gaunt_text_y   = 132;
    g_layout.dev_help_gaunt_text_spacing = 21;
    g_layout.dev_help_gaunt_text_align = 1;
    g_layout.dev_help_cheat_header_y = 282;
    g_layout.dev_help_cheat_header_align = 2;
    g_layout.dev_help_cheat_text_x   = 30;
    g_layout.dev_help_cheat_text_y   = 304;
    g_layout.dev_help_cheat_text_spacing = 18;
    g_layout.dev_help_cheat_text_align = 1;
    g_layout.dev_help_demo_balls_x   = 478;
    g_layout.dev_help_demo_balls_y   = 295;
    g_layout.dev_help_demo_balls_spacing = 32;
    g_layout.dev_help_demo_balls_align = 5;
    g_layout.dev_help_demo_mouse_x   = 508;
    g_layout.dev_help_demo_mouse_y   = 350;
    g_layout.dev_help_demo_mouse_align = 5;
    g_layout.dev_help_demo_click_x   = 458;
    g_layout.dev_help_demo_click_y   = 362;
    g_layout.dev_help_demo_click_align = 5;
    g_layout.dev_help_demo_clear_x   = 478;
    g_layout.dev_help_demo_clear_y   = 303;
    g_layout.dev_help_demo_clear_align = 5;
    g_layout.dev_dialog_width        = 380;
    g_layout.dev_dialog_min_top      = 10;
    g_layout.dev_dialog_align        = 5;
}

/* Fast XML attribute parser */
static int parse_xml_attr_str(const char *tag_start, const char *tag_end, const char *attr_name, char *out_str, size_t max_len) {
    char p1[64], p2[64];
    snprintf(p1, sizeof(p1), "%s=\"", attr_name);
    snprintf(p2, sizeof(p2), "%s='", attr_name);

    const char *p = strstr(tag_start, p1);
    char quote = '"';
    size_t plen = strlen(p1);
    if (!p || p >= tag_end) {
        p = strstr(tag_start, p2);
        quote = '\'';
        plen = strlen(p2);
    }
    if (p && p < tag_end) {
        p += plen;
        const char *end_val = strchr(p, quote);
        if (end_val && end_val <= tag_end) {
            size_t len = (size_t)(end_val - p);
            if (len >= max_len) len = max_len - 1;
            memcpy(out_str, p, len);
            out_str[len] = '\0';
            return 1;
        }
    }
    return 0;
}

static int parse_xml_attr_align(const char *tag_start, const char *tag_end, const char *attr_name, int *out_align) {
    char str[32];
    if (parse_xml_attr_str(tag_start, tag_end, attr_name, str, sizeof(str))) {
        *out_align = ZRH_ParseAlignAttr(str);
        return 1;
    }
    return 0;
}

static int parse_xml_attr(const char *tag_start, const char *tag_end, const char *attr_name, int *out_val) {
    char p1[64], p2[64];
    snprintf(p1, sizeof(p1), "%s=\"", attr_name);
    snprintf(p2, sizeof(p2), "%s='", attr_name);

    const char *p = strstr(tag_start, p1);
    char quote = '"';
    size_t plen = strlen(p1);
    if (!p || p >= tag_end) {
        p = strstr(tag_start, p2);
        quote = '\'';
        plen = strlen(p2);
    }
    if (p && p < tag_end) {
        p += plen;
        char *end_val = strchr(p, quote);
        if (end_val && end_val <= tag_end) {
            *out_val = atoi(p);
            return 1;
        }
    }
    return 0;
}

/* Finds tag by name inside section or buffer */
static const char *find_tag_start(const char *buf, const char *end, const char *tag_name, const char **out_tag_end) {
    char open_tag[64];
    snprintf(open_tag, sizeof(open_tag), "<%s", tag_name);
    size_t tag_len = strlen(open_tag);

    const char *p = buf;
    while (p < end) {
        const char *found = strstr(p, open_tag);
        if (!found || found >= end) return NULL;

        /* Verify boundary after tag name (space, >, tab, newline) */
        char next = found[tag_len];
        if (next == ' ' || next == '\t' || next == '>' || next == '/' || next == '\r' || next == '\n') {
            const char *close_bracket = strchr(found, '>');
            if (close_bracket && close_bracket < end) {
                if (out_tag_end) *out_tag_end = close_bracket;
                return found;
            }
        }
        p = found + 1;
    }
    return NULL;
}

void ZRH_LoadLayoutXml(void) {
    char chosen_path[MAX_PATH] = {0};
    char test_path[MAX_PATH];
    FILE *f = NULL;

    /* Candidate 1: exe_dir\properties\layout.xml */
    if (GetModuleFileNameA(NULL, test_path, MAX_PATH)) {
        char *slash = strrchr(test_path, '\\');
        if (slash) *(slash + 1) = '\0';
        strcat(test_path, "properties\\layout.xml");
        f = fopen(test_path, "rb");
        if (f) strcpy(chosen_path, test_path);
    }

    /* Candidate 2: dll_dir\..\properties\layout.xml */
    if (!f) {
        HMODULE hDll = GetModuleHandleA("zumareshandler.dll");
        if (hDll && GetModuleFileNameA(hDll, test_path, MAX_PATH)) {
            char *slash = strrchr(test_path, '\\');
            if (slash) *(slash + 1) = '\0';
            strcat(test_path, "..\\properties\\layout.xml");
            f = fopen(test_path, "rb");
            if (f) strcpy(chosen_path, test_path);
        }
    }

    /* Candidate 3: dll_dir\properties\layout.xml */
    if (!f) {
        HMODULE hDll = GetModuleHandleA("zumareshandler.dll");
        if (hDll && GetModuleFileNameA(hDll, test_path, MAX_PATH)) {
            char *slash = strrchr(test_path, '\\');
            if (slash) *(slash + 1) = '\0';
            strcat(test_path, "properties\\layout.xml");
            f = fopen(test_path, "rb");
            if (f) strcpy(chosen_path, test_path);
        }
    }

    if (f && chosen_path[0]) {
        LogF("[ZRES] layout.xml = %s", chosen_path);
    } else {
        LogF("[ZRES] [ERROR] properties/layout.xml not found across all search candidates (exe_dir, dll_dir\\.., dll_dir). Layout patching aborted!");
        return;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > 2 * 1024 * 1024) {
        fclose(f);
        return;
    }

    char *buf = (char *)malloc(sz + 1);
    if (!buf) {
        fclose(f);
        return;
    }

    fread(buf, 1, sz, f);
    buf[sz] = 0;
    fclose(f);

    const char *buf_end = buf + sz;

    /* Parse Adventure Temples */
    const char *tend = NULL;
    const char *t = NULL;

    const char *adv_temples_sec = strstr(buf, "<AdventureTemples");
    const char *adv_temples_sec_end = adv_temples_sec ? strstr(adv_temples_sec, "</AdventureTemples>") : NULL;
    if (adv_temples_sec && adv_temples_sec_end) {
        const char *open_end = strchr(adv_temples_sec, '>');
        if (open_end && open_end < adv_temples_sec_end) {
            const char *sp = strstr(adv_temples_sec, "space=\"");
            if (!sp) sp = strstr(adv_temples_sec, "space='");
            if (sp && sp < open_end) {
                sscanf(sp + 7, "%dx%d", &g_layout.adv_temples_space_w, &g_layout.adv_temples_space_h);
            }
        }
    }

    if ((t = find_tag_start(buf, buf_end, "AdvBack", &tend)) != NULL) {
        parse_xml_attr(t, tend, "x", &g_layout.adv_back_x);
        parse_xml_attr(t, tend, "y", &g_layout.adv_back_y);
    }
    if ((t = find_tag_start(buf, buf_end, "Temple1", &tend)) != NULL) {
        parse_xml_attr(t, tend, "x", &g_layout.adv_temple1_x);
        parse_xml_attr(t, tend, "y", &g_layout.adv_temple1_y);
    }
    if ((t = find_tag_start(buf, buf_end, "Temple2", &tend)) != NULL) {
        parse_xml_attr(t, tend, "x", &g_layout.adv_temple2_x);
        parse_xml_attr(t, tend, "y", &g_layout.adv_temple2_y);
    }
    if ((t = find_tag_start(buf, buf_end, "Temple2V", &tend)) != NULL) {
        parse_xml_attr(t, tend, "x", &g_layout.adv_temple2v_x);
        parse_xml_attr(t, tend, "y", &g_layout.adv_temple2v_y);
    } else {
        g_layout.adv_temple2v_x = g_layout.adv_temple2_x;
        g_layout.adv_temple2v_y = g_layout.adv_temple2_y;
    }
    if ((t = find_tag_start(buf, buf_end, "Temple3", &tend)) != NULL) {
        parse_xml_attr(t, tend, "x", &g_layout.adv_temple3_x);
        parse_xml_attr(t, tend, "y", &g_layout.adv_temple3_y);
    }
    if ((t = find_tag_start(buf, buf_end, "Temple3V", &tend)) != NULL) {
        parse_xml_attr(t, tend, "x", &g_layout.adv_temple3v_x);
        parse_xml_attr(t, tend, "y", &g_layout.adv_temple3v_y);
    } else {
        g_layout.adv_temple3v_x = g_layout.adv_temple3_x;
        g_layout.adv_temple3v_y = g_layout.adv_temple3_y;
    }
    if ((t = find_tag_start(buf, buf_end, "Temple4", &tend)) != NULL) {
        parse_xml_attr(t, tend, "x", &g_layout.adv_temple4_x);
        parse_xml_attr(t, tend, "y", &g_layout.adv_temple4_y);
    }
    if ((t = find_tag_start(buf, buf_end, "Temple5", &tend)) != NULL) {
        parse_xml_attr(t, tend, "x", &g_layout.adv_temple5_x);
        parse_xml_attr(t, tend, "y", &g_layout.adv_temple5_y);
    }
    if ((t = find_tag_start(buf, buf_end, "Middle", &tend)) != NULL) {
        parse_xml_attr(t, tend, "x", &g_layout.adv_middle_x);
        parse_xml_attr(t, tend, "y", &g_layout.adv_middle_y);
    }

    /* Parse Adventure Doors (supports both explicit doorX/frogX and legacy x1/x2) */
    const char *adv_doors_sec = strstr(buf, "<AdventureDoors");
    const char *adv_doors_sec_end = adv_doors_sec ? strstr(adv_doors_sec, "</AdventureDoors>") : NULL;
    if (adv_doors_sec && adv_doors_sec_end) {
        const char *open_end = strchr(adv_doors_sec, '>');
        if (open_end && open_end < adv_doors_sec_end) {
            const char *sp = strstr(adv_doors_sec, "space=\"");
            if (!sp) sp = strstr(adv_doors_sec, "space='");
            if (sp && sp < open_end) {
                sscanf(sp + 7, "%dx%d", &g_layout.adv_doors_space_w, &g_layout.adv_doors_space_h);
            }
        }
        const char *scan = adv_doors_sec;
        while ((scan = find_tag_start(scan, adv_doors_sec_end, "Door", &tend)) != NULL) {
            int id = -1;
            if (parse_xml_attr(scan, tend, "id", &id) && id >= 0 && id < 13) {
                int val;
                if (parse_xml_attr(scan, tend, "doorX", &val) || parse_xml_attr(scan, tend, "x1", &val))
                    g_layout.adv_doors[id].x1 = val;
                if (parse_xml_attr(scan, tend, "doorY", &val) || parse_xml_attr(scan, tend, "y1", &val))
                    g_layout.adv_doors[id].y1 = val;
                if (parse_xml_attr(scan, tend, "frogX", &val) || parse_xml_attr(scan, tend, "x2", &val))
                    g_layout.adv_doors[id].x2 = val;
                if (parse_xml_attr(scan, tend, "frogY", &val) || parse_xml_attr(scan, tend, "y2", &val))
                    g_layout.adv_doors[id].y2 = val;
            }
            scan = tend + 1;
        }
    }

    /* Parse Adventure Navigation Buttons */
    const char *adv_btn_sec = strstr(buf, "<AdventureButtons");
    const char *adv_btn_sec_end = adv_btn_sec ? strstr(adv_btn_sec, "</AdventureButtons>") : NULL;
    if (adv_btn_sec && adv_btn_sec_end) {
        const char *open_end = strchr(adv_btn_sec, '>');
        if (open_end && open_end < adv_btn_sec_end) {
            const char *sp = strstr(adv_btn_sec, "space=\"");
            if (!sp) sp = strstr(adv_btn_sec, "space='");
            if (sp && sp < open_end) {
                sscanf(sp + 7, "%dx%d", &g_layout.adv_buttons_space_w, &g_layout.adv_buttons_space_h);
            }
        }
        if ((t = find_tag_start(adv_btn_sec, adv_btn_sec_end, "MainMenu", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.adv_btn_mainmenu_x);
            parse_xml_attr(t, tend, "y", &g_layout.adv_btn_mainmenu_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.adv_btn_mainmenu_align);
        }
        if ((t = find_tag_start(adv_btn_sec, adv_btn_sec_end, "Play", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.adv_btn_play_x);
            parse_xml_attr(t, tend, "y", &g_layout.adv_btn_play_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.adv_btn_play_align);
        }
    }

    /* Parse Adventure Labels */
    const char *adv_lbl_sec = strstr(buf, "<AdventureLabels");
    const char *adv_lbl_sec_end = adv_lbl_sec ? strstr(adv_lbl_sec, "</AdventureLabels>") : NULL;
    if (adv_lbl_sec && adv_lbl_sec_end) {
        const char *open_end = strchr(adv_lbl_sec, '>');
        if (open_end && open_end < adv_lbl_sec_end) {
            const char *sp = strstr(adv_lbl_sec, "space=\"");
            if (!sp) sp = strstr(adv_lbl_sec, "space='");
            if (sp && sp < open_end) {
                sscanf(sp + 7, "%dx%d", &g_layout.adv_labels_space_w, &g_layout.adv_labels_space_h);
            }
        }
        if ((t = find_tag_start(adv_lbl_sec, adv_lbl_sec_end, "AdventureTitle", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.adv_title_x);
            parse_xml_attr(t, tend, "y", &g_layout.adv_title_y);
        }
        if ((t = find_tag_start(adv_lbl_sec, adv_lbl_sec_end, "HighscoreTitle", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.adv_hs_title_x);
            parse_xml_attr(t, tend, "y", &g_layout.adv_hs_title_y);
        }
        if ((t = find_tag_start(adv_lbl_sec, adv_lbl_sec_end, "BottomBox", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.adv_bottom_box_x);
            parse_xml_attr(t, tend, "y", &g_layout.adv_bottom_box_y);
        }
        if ((t = find_tag_start(adv_lbl_sec, adv_lbl_sec_end, "StageNum", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.adv_stage_num_x);
            parse_xml_attr(t, tend, "y", &g_layout.adv_stage_num_y);
        }
        if ((t = find_tag_start(adv_lbl_sec, adv_lbl_sec_end, "StageText", &tend)) != NULL) {
            parse_xml_attr(t, tend, "y", &g_layout.adv_stage_text_y);
            parse_xml_attr(t, tend, "centerX", &g_layout.adv_stage_center_x);
        }
        if ((t = find_tag_start(adv_lbl_sec, adv_lbl_sec_end, "Subtitle", &tend)) != NULL) {
            parse_xml_attr(t, tend, "y", &g_layout.adv_subtitle_y);
            parse_xml_attr(t, tend, "centerX", &g_layout.adv_subtitle_center_x);
        }
        if ((t = find_tag_start(adv_lbl_sec, adv_lbl_sec_end, "HighscoreScores", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.adv_hs_scores_x);
            parse_xml_attr(t, tend, "y", &g_layout.adv_hs_base_y);
            parse_xml_attr(t, tend, "baseY", &g_layout.adv_hs_base_y);
            parse_xml_attr(t, tend, "spacing", &g_layout.adv_hs_spacing);
        }
        if ((t = find_tag_start(adv_lbl_sec, adv_lbl_sec_end, "HighscoreDate", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.adv_hs_date_x);
        }
        if ((t = find_tag_start(adv_lbl_sec, adv_lbl_sec_end, "HighscoreNames", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.adv_hs_names_x);
            parse_xml_attr(t, tend, "y", &g_layout.adv_hs_base_y);
            parse_xml_attr(t, tend, "baseY", &g_layout.adv_hs_base_y);
        }
        if ((t = find_tag_start(adv_lbl_sec, adv_lbl_sec_end, "HighscoreRow", &tend)) != NULL) {
            parse_xml_attr(t, tend, "spacing", &g_layout.adv_hs_spacing);
        }
        if ((t = find_tag_start(adv_lbl_sec, adv_lbl_sec_end, "InfoBox", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.adv_infobox_x);
            parse_xml_attr(t, tend, "y", &g_layout.adv_infobox_y);
            parse_xml_attr(t, tend, "w", &g_layout.adv_infobox_w);
            parse_xml_attr(t, tend, "h", &g_layout.adv_infobox_h);
            parse_xml_attr(t, tend, "lineSpacing", &g_layout.adv_infobox_line_spacing);
        }

        /* Stage 13 Banner elements (Unified with BottomBox, StageNum, StageText, Subtitle) */
        if ((t = find_tag_start(adv_lbl_sec, adv_lbl_sec_end, "Stage13Badge", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.adv_s13_badge_x);
            parse_xml_attr(t, tend, "y", &g_layout.adv_s13_badge_y);
        } else {
            g_layout.adv_s13_badge_x = g_layout.adv_bottom_box_x;
            g_layout.adv_s13_badge_y = g_layout.adv_bottom_box_y;
        }
        if ((t = find_tag_start(adv_lbl_sec, adv_lbl_sec_end, "Stage13Num", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.adv_s13_num_x);
            parse_xml_attr(t, tend, "y", &g_layout.adv_s13_num_y);
        } else {
            g_layout.adv_s13_num_x = g_layout.adv_stage_num_x;
            g_layout.adv_s13_num_y = g_layout.adv_stage_num_y;
        }
        if ((t = find_tag_start(adv_lbl_sec, adv_lbl_sec_end, "Stage13Title", &tend)) != NULL) {
            if (!parse_xml_attr(t, tend, "centerX", &g_layout.adv_s13_title_x)) {
                parse_xml_attr(t, tend, "x", &g_layout.adv_s13_title_x);
            }
            parse_xml_attr(t, tend, "y", &g_layout.adv_s13_title_y);
        } else {
            g_layout.adv_s13_title_x = g_layout.adv_stage_center_x;
            g_layout.adv_s13_title_y = g_layout.adv_stage_text_y;
        }
        if ((t = find_tag_start(adv_lbl_sec, adv_lbl_sec_end, "Stage13Subtitle", &tend)) != NULL) {
            if (!parse_xml_attr(t, tend, "centerX", &g_layout.adv_s13_sub_x)) {
                parse_xml_attr(t, tend, "x", &g_layout.adv_s13_sub_x);
            }
            parse_xml_attr(t, tend, "y", &g_layout.adv_s13_sub_y);
        } else {
            g_layout.adv_s13_sub_x = g_layout.adv_subtitle_center_x;
            g_layout.adv_s13_sub_y = g_layout.adv_subtitle_y;
        }
    }

    /* Parse Gauntlet Doors */
    const char *gaunt_doors_sec = strstr(buf, "<GauntletDoors");
    const char *gaunt_doors_sec_end = gaunt_doors_sec ? strstr(gaunt_doors_sec, "</GauntletDoors>") : NULL;
    if (gaunt_doors_sec && gaunt_doors_sec_end) {
        const char *open_end = strchr(gaunt_doors_sec, '>');
        if (open_end && open_end < gaunt_doors_sec_end) {
            const char *sp = strstr(gaunt_doors_sec, "space=\"");
            if (!sp) sp = strstr(gaunt_doors_sec, "space='");
            if (sp && sp < open_end) {
                sscanf(sp + 7, "%dx%d", &g_layout.gaunt_doors_space_w, &g_layout.gaunt_doors_space_h);
            }
        }
        const char *scan = gaunt_doors_sec;
        while ((scan = find_tag_start(scan, gaunt_doors_sec_end, "Door", &tend)) != NULL) {
            int id = -1;
            if (parse_xml_attr(scan, tend, "id", &id) && id >= 0 && id < 4) {
                parse_xml_attr(scan, tend, "x", &g_layout.gaunt_doors[id].x);
                parse_xml_attr(scan, tend, "y", &g_layout.gaunt_doors[id].y);
            }
            scan = tend + 1;
        }
    }

    /* Parse Gauntlet Navigation Buttons */
    const char *gaunt_btn_sec = strstr(buf, "<GauntletButtons");
    const char *gaunt_btn_sec_end = gaunt_btn_sec ? strstr(gaunt_btn_sec, "</GauntletButtons>") : NULL;
    if (gaunt_btn_sec && gaunt_btn_sec_end) {
        const char *open_end = strchr(gaunt_btn_sec, '>');
        if (open_end && open_end < gaunt_btn_sec_end) {
            const char *sp = strstr(gaunt_btn_sec, "space=\"");
            if (!sp) sp = strstr(gaunt_btn_sec, "space='");
            if (sp && sp < open_end) {
                sscanf(sp + 7, "%dx%d", &g_layout.gaunt_buttons_space_w, &g_layout.gaunt_buttons_space_h);
            }
        }
        if ((t = find_tag_start(gaunt_btn_sec, gaunt_btn_sec_end, "Practice", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.gaunt_btn_practice_x);
            parse_xml_attr(t, tend, "y", &g_layout.gaunt_btn_practice_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.gaunt_btn_practice_align);
        }
        if ((t = find_tag_start(gaunt_btn_sec, gaunt_btn_sec_end, "Survival", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.gaunt_btn_survival_x);
            parse_xml_attr(t, tend, "y", &g_layout.gaunt_btn_survival_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.gaunt_btn_survival_align);
        }
        if ((t = find_tag_start(gaunt_btn_sec, gaunt_btn_sec_end, "Back", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.gaunt_btn_back_x);
            parse_xml_attr(t, tend, "y", &g_layout.gaunt_btn_back_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.gaunt_btn_back_align);
        }
        if ((t = find_tag_start(gaunt_btn_sec, gaunt_btn_sec_end, "MainMenu", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.gaunt_btn_mainmenu_x);
            parse_xml_attr(t, tend, "y", &g_layout.gaunt_btn_mainmenu_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.gaunt_btn_mainmenu_align);
        }
        if ((t = find_tag_start(gaunt_btn_sec, gaunt_btn_sec_end, "Next", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.gaunt_btn_next_x);
            parse_xml_attr(t, tend, "y", &g_layout.gaunt_btn_next_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.gaunt_btn_next_align);
        }
        if ((t = find_tag_start(gaunt_btn_sec, gaunt_btn_sec_end, "Play", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.gaunt_btn_play_x);
            parse_xml_attr(t, tend, "y", &g_layout.gaunt_btn_play_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.gaunt_btn_play_align);
        }
    }

    /* Parse Gauntlet Labels */
    const char *gaunt_lbl_sec = strstr(buf, "<GauntletLabels");
    const char *gaunt_lbl_sec_end = gaunt_lbl_sec ? strstr(gaunt_lbl_sec, "</GauntletLabels>") : NULL;
    if (gaunt_lbl_sec && gaunt_lbl_sec_end) {
        const char *open_end = strchr(gaunt_lbl_sec, '>');
        if (open_end && open_end < gaunt_lbl_sec_end) {
            const char *sp = strstr(gaunt_lbl_sec, "space=\"");
            if (!sp) sp = strstr(gaunt_lbl_sec, "space='");
            if (sp && sp < open_end) {
                sscanf(sp + 7, "%dx%d", &g_layout.gaunt_labels_space_w, &g_layout.gaunt_labels_space_h);
            }
        }
        if ((t = find_tag_start(gaunt_lbl_sec, gaunt_lbl_sec_end, "DoorFrame", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.gaunt_door_frame_x);
            parse_xml_attr(t, tend, "y", &g_layout.gaunt_door_frame_y);
            parse_xml_attr(t, tend, "w", &g_layout.gaunt_door_frame_w);
            parse_xml_attr(t, tend, "h", &g_layout.gaunt_door_frame_h);
        }
        if ((t = find_tag_start(gaunt_lbl_sec, gaunt_lbl_sec_end, "GauntletTitle", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.gaunt_title_x);
            parse_xml_attr(t, tend, "y", &g_layout.gaunt_title_y);
        }
        if ((t = find_tag_start(gaunt_lbl_sec, gaunt_lbl_sec_end, "HighscoreTitle", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.gaunt_hs_title_x);
            parse_xml_attr(t, tend, "y", &g_layout.gaunt_hs_title_y);
        }
        if ((t = find_tag_start(gaunt_lbl_sec, gaunt_lbl_sec_end, "DoorName", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.gaunt_door_name_x);
            parse_xml_attr(t, tend, "y", &g_layout.gaunt_door_name_y);
        }
        if ((t = find_tag_start(gaunt_lbl_sec, gaunt_lbl_sec_end, "PracticeText", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.gaunt_practice_text_x);
            parse_xml_attr(t, tend, "y", &g_layout.gaunt_practice_text_y);
            parse_xml_attr(t, tend, "w", &g_layout.gaunt_practice_text_w);
        }
        if ((t = find_tag_start(gaunt_lbl_sec, gaunt_lbl_sec_end, "CustomMapTag", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.gaunt_custom_map_x);
            parse_xml_attr(t, tend, "y", &g_layout.gaunt_custom_map_y);
        }
        if ((t = find_tag_start(gaunt_lbl_sec, gaunt_lbl_sec_end, "Gem", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.gaunt_gem_x);
            parse_xml_attr(t, tend, "y", &g_layout.gaunt_gem_y);
        }
        if ((t = find_tag_start(gaunt_lbl_sec, gaunt_lbl_sec_end, "HighscoreScores", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.gaunt_hs_scores_x);
            parse_xml_attr(t, tend, "y", &g_layout.gaunt_hs_base_y);
            parse_xml_attr(t, tend, "baseY", &g_layout.gaunt_hs_base_y);
            parse_xml_attr(t, tend, "spacing", &g_layout.gaunt_hs_spacing);
        }
        if ((t = find_tag_start(gaunt_lbl_sec, gaunt_lbl_sec_end, "HighscoreNames", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.gaunt_hs_names_x);
            parse_xml_attr(t, tend, "y", &g_layout.gaunt_hs_base_y);
            parse_xml_attr(t, tend, "baseY", &g_layout.gaunt_hs_base_y);
        }
        if ((t = find_tag_start(gaunt_lbl_sec, gaunt_lbl_sec_end, "HighscoreRow", &tend)) != NULL) {
            parse_xml_attr(t, tend, "spacing", &g_layout.gaunt_hs_spacing);
        }
        if ((t = find_tag_start(gaunt_lbl_sec, gaunt_lbl_sec_end, "HighscoreClip", &tend)) != NULL) {
            parse_xml_attr(t, tend, "w", &g_layout.gaunt_hs_clip_w);
        }
        if ((t = find_tag_start(gaunt_lbl_sec, gaunt_lbl_sec_end, "InfoBox", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.gaunt_infobox_x);
            parse_xml_attr(t, tend, "y", &g_layout.gaunt_infobox_y);
            parse_xml_attr(t, tend, "w", &g_layout.gaunt_infobox_w);
            parse_xml_attr(t, tend, "h", &g_layout.gaunt_infobox_h);
            parse_xml_attr(t, tend, "lineSpacing", &g_layout.gaunt_infobox_line_spacing);
        }
    }

    char sp_doors[32] = "canon";
    char sp_btn[32]   = "canon";
    char sp_lbl[32]   = "canon";
    if (g_layout.gaunt_doors_space_w > 0)
        snprintf(sp_doors, sizeof(sp_doors), "%dx%d", g_layout.gaunt_doors_space_w, g_layout.gaunt_doors_space_h);
    if (g_layout.gaunt_buttons_space_w > 0)
        snprintf(sp_btn, sizeof(sp_btn), "%dx%d", g_layout.gaunt_buttons_space_w, g_layout.gaunt_buttons_space_h);
    if (g_layout.gaunt_labels_space_w > 0)
        snprintf(sp_lbl, sizeof(sp_lbl), "%dx%d", g_layout.gaunt_labels_space_w, g_layout.gaunt_labels_space_h);
    LogF("[GAUNT_LAYOUT] space doors=%s buttons=%s labels=%s", sp_doors, sp_btn, sp_lbl);

    /* Sanity check: log ERROR if space was not parsed but coordinates exceed canonical 640x480 */
    if (g_layout.gaunt_doors_space_w == 0) {
        for (int i = 0; i < 4; i++) {
            if (g_layout.gaunt_doors[i].x > 640 || g_layout.gaunt_doors[i].y > 480) {
                LogF("[GAUNT_LAYOUT][ERROR] GauntletDoors space not set, but Door %d exceeds canonical bounds: x=%d y=%d!",
                     i, g_layout.gaunt_doors[i].x, g_layout.gaunt_doors[i].y);
            }
        }
    }
    if (g_layout.gaunt_buttons_space_w == 0) {
        if (g_layout.gaunt_btn_play_x > 640 || g_layout.gaunt_btn_play_y > 480 ||
            g_layout.gaunt_btn_practice_x > 640 || g_layout.gaunt_btn_practice_y > 480) {
            LogF("[GAUNT_LAYOUT][ERROR] GauntletButtons space not set, but button coordinates exceed canonical bounds!");
        }
    }
    if (g_layout.gaunt_labels_space_w == 0) {
        if (g_layout.gaunt_hs_title_x > 640 || g_layout.gaunt_hs_title_y > 480 ||
            g_layout.gaunt_door_frame_x > 640 || g_layout.gaunt_door_frame_y > 480) {
            LogF("[GAUNT_LAYOUT][ERROR] GauntletLabels space not set, but label coordinates exceed canonical bounds!");
        }
    }

    /* Parse Main Menu Buttons & Frog Elements */
    const char *mm_sec = strstr(buf, "<MainMenu");
    const char *mm_sec_end = mm_sec ? strstr(mm_sec, "</MainMenu>") : NULL;
    if (mm_sec && mm_sec_end) {
        /* Parse coordinate space attribute, e.g. space="1200x675" */
        const char *open_end = strchr(mm_sec, '>');
        const char *sp = strstr(mm_sec, "space=\"");
        if (!sp) sp = strstr(mm_sec, "space='");
        if (open_end && sp && sp < open_end) {
            sscanf(sp + 7, "%dx%d", &g_layout.mm_space_w, &g_layout.mm_space_h);
        }

        if ((t = find_tag_start(mm_sec, mm_sec_end, "AdventureBtn", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mm_btn1_x);
            parse_xml_attr(t, tend, "y", &g_layout.mm_btn1_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.mm_btn1_align);
        }
        if ((t = find_tag_start(mm_sec, mm_sec_end, "GauntletBtn", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mm_btn2_x);
            parse_xml_attr(t, tend, "y", &g_layout.mm_btn2_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.mm_btn2_align);
        }
        if ((t = find_tag_start(mm_sec, mm_sec_end, "OptionsBtn", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mm_btn3_x);
            parse_xml_attr(t, tend, "y", &g_layout.mm_btn3_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.mm_btn3_align);
        }
        if ((t = find_tag_start(mm_sec, mm_sec_end, "MoreGamesBtn", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mm_btn4_x);
            parse_xml_attr(t, tend, "y", &g_layout.mm_btn4_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.mm_btn4_align);
        }
        if ((t = find_tag_start(mm_sec, mm_sec_end, "QuitBtn", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mm_btn5_x);
            parse_xml_attr(t, tend, "y", &g_layout.mm_btn5_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.mm_btn5_align);
        }
        if ((t = find_tag_start(mm_sec, mm_sec_end, "FrogEyes", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mm_eyes_x);
            parse_xml_attr(t, tend, "y", &g_layout.mm_eyes_y);
        }
        if ((t = find_tag_start(mm_sec, mm_sec_end, "Hat", &tend)) != NULL ||
            (t = find_tag_start(mm_sec, mm_sec_end, "EyeBlink", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mm_hat_x);
            parse_xml_attr(t, tend, "y", &g_layout.mm_hat_y);
        }
        if ((t = find_tag_start(mm_sec, mm_sec_end, "Sun", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mm_sun_x);
            parse_xml_attr(t, tend, "y", &g_layout.mm_sun_y);
        } else if ((t = find_tag_start(mm_sec, mm_sec_end, "Sun1", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mm_sun_x);
            parse_xml_attr(t, tend, "y", &g_layout.mm_sun_y);
        }
        if ((t = find_tag_start(mm_sec, mm_sec_end, "MMSun", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mm_sun_face_x);
            parse_xml_attr(t, tend, "y", &g_layout.mm_sun_face_y);
        }
        if ((t = find_tag_start(mm_sec, mm_sec_end, "EyeRay1", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mm_eye_ray1_x);
            parse_xml_attr(t, tend, "y", &g_layout.mm_eye_ray1_y);
        }
        if ((t = find_tag_start(mm_sec, mm_sec_end, "EyeRay2", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mm_eye_ray2_x);
            parse_xml_attr(t, tend, "y", &g_layout.mm_eye_ray2_y);
        }
        if ((t = find_tag_start(mm_sec, mm_sec_end, "EyeIdle1", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mm_eye_idle1_x);
            parse_xml_attr(t, tend, "y", &g_layout.mm_eye_idle1_y);
        }
        if ((t = find_tag_start(mm_sec, mm_sec_end, "EyeIdle2", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mm_eye_idle2_x);
            parse_xml_attr(t, tend, "y", &g_layout.mm_eye_idle2_y);
        }
        if ((t = find_tag_start(mm_sec, mm_sec_end, "WelcomeText", &tend)) != NULL) {
            parse_xml_attr(t, tend, "y", &g_layout.mm_welcome_y);
        }
        if ((t = find_tag_start(mm_sec, mm_sec_end, "UserSwitchText", &tend)) != NULL) {
            parse_xml_attr(t, tend, "y", &g_layout.mm_user_switch_y);
        }
    }

    /* Parse More Games Screen */
    const char *mg_sec = strstr(buf, "<MoreGames ");
    if (!mg_sec) mg_sec = strstr(buf, "<MoreGames>");
    const char *mg_sec_end = mg_sec ? strstr(mg_sec, "</MoreGames>") : NULL;
    if (mg_sec && mg_sec_end) {
        /* Parse coordinate space attribute, e.g. space="1200x675" */
        const char *open_end = strchr(mg_sec, '>');
        const char *sp = strstr(mg_sec, "space=\"");
        if (!sp) sp = strstr(mg_sec, "space='");
        if (open_end && sp && sp < open_end) {
            sscanf(sp + 7, "%dx%d", &g_layout.mg_space_w, &g_layout.mg_space_h);
        }
        if ((t = find_tag_start(mg_sec, mg_sec_end, "Thumb0", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mg_t0_x);
            parse_xml_attr(t, tend, "y", &g_layout.mg_t0_y);
            parse_xml_attr(t, tend, "w", &g_layout.mg_t0_w);
            parse_xml_attr(t, tend, "h", &g_layout.mg_t0_h);
        }
        if ((t = find_tag_start(mg_sec, mg_sec_end, "Thumb1", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mg_t1_x);
            parse_xml_attr(t, tend, "y", &g_layout.mg_t1_y);
            parse_xml_attr(t, tend, "w", &g_layout.mg_t1_w);
            parse_xml_attr(t, tend, "h", &g_layout.mg_t1_h);
        }
        if ((t = find_tag_start(mg_sec, mg_sec_end, "Thumb2", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mg_t2_x);
            parse_xml_attr(t, tend, "y", &g_layout.mg_t2_y);
            parse_xml_attr(t, tend, "w", &g_layout.mg_t2_w);
            parse_xml_attr(t, tend, "h", &g_layout.mg_t2_h);
        }
        if ((t = find_tag_start(mg_sec, mg_sec_end, "Thumb3", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mg_t3_x);
            parse_xml_attr(t, tend, "y", &g_layout.mg_t3_y);
            parse_xml_attr(t, tend, "w", &g_layout.mg_t3_w);
            parse_xml_attr(t, tend, "h", &g_layout.mg_t3_h);
        }
        if ((t = find_tag_start(mg_sec, mg_sec_end, "BackBtn", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.mg_btn_back_x);
            parse_xml_attr(t, tend, "y", &g_layout.mg_btn_back_y);
        }
    }

    char sp_mg[32] = "canon";
    if (g_layout.mg_space_w > 0)
        snprintf(sp_mg, sizeof(sp_mg), "%dx%d", g_layout.mg_space_w, g_layout.mg_space_h);
    LogF("[MG_LAYOUT] space moregames=%s", sp_mg);

    /* Sanity check: log ERROR if space was not parsed but coordinates exceed canonical 640x480 */
    if (g_layout.mg_space_w == 0) {
        if (g_layout.mg_t0_x > 640 || g_layout.mg_t0_y > 480 ||
            g_layout.mg_t1_x > 640 || g_layout.mg_t1_y > 480 ||
            g_layout.mg_t2_x > 640 || g_layout.mg_t2_y > 480 ||
            g_layout.mg_t3_x > 640 || g_layout.mg_t3_y > 480 ||
            g_layout.mg_btn_back_x > 640 || g_layout.mg_btn_back_y > 480) {
            LogF("[MG_LAYOUT][ERROR] MoreGames space not set, but coordinates exceed canonical bounds!");
        }
    }

    /* Parse Loading Screen */
    const char *load_sec = strstr(buf, "<LoadingScreen");
    const char *load_sec_end = load_sec ? strstr(load_sec, "</LoadingScreen>") : NULL;
    if (load_sec && load_sec_end) {
        /* Parse coordinate space attribute, e.g. space="1200x675" */
        const char *open_end = strchr(load_sec, '>');
        const char *sp = strstr(load_sec, "space=\"");
        if (!sp) sp = strstr(load_sec, "space='");
        if (open_end && sp && sp < open_end) {
            sscanf(sp + 7, "%dx%d", &g_layout.load_space_w, &g_layout.load_space_h);
        }

        if ((t = find_tag_start(load_sec, load_sec_end, "LoadingBar", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.load_bar_x);
            parse_xml_attr(t, tend, "y", &g_layout.load_bar_y);
        }
        if ((t = find_tag_start(load_sec, load_sec_end, "ClickToPlay", &tend)) != NULL) {
            parse_xml_attr(t, tend, "y", &g_layout.load_click_play_y);
        }
    }

    /* Parse In-Game HUD & Banners */
    const char *hud_sec = strstr(buf, "<InGameHUD");
    const char *hud_sec_end = hud_sec ? strstr(hud_sec, "</InGameHUD>") : NULL;
    if (hud_sec && hud_sec_end) {
        /* Parse coordinate space attribute, e.g. space="1200x675" */
        const char *open_end = strchr(hud_sec, '>');
        const char *sp = strstr(hud_sec, "space=\"");
        if (!sp) sp = strstr(hud_sec, "space='");
        if (open_end && sp && sp < open_end) {
            sscanf(sp + 7, "%dx%d", &g_layout.hud_space_w, &g_layout.hud_space_h);
        }

        if ((t = find_tag_start(hud_sec, hud_sec_end, "LevelBanner", &tend)) != NULL) {
            parse_xml_attr(t, tend, "offsetX", &g_layout.hud_level_banner_offset_x);
            parse_xml_attr(t, tend, "offsetY", &g_layout.hud_level_banner_offset_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.hud_level_banner_align);
        }
        if ((t = find_tag_start(hud_sec, hud_sec_end, "LivesBanner", &tend)) != NULL) {
            if (parse_xml_attr(t, tend, "centerX", &g_layout.hud_lives_banner_center_x)) g_layout.has_lives_banner_center_x = true;
            if (parse_xml_attr(t, tend, "centerY", &g_layout.hud_lives_banner_center_y)) g_layout.has_lives_banner_center_y = true;
            parse_xml_attr_align(t, tend, "align", &g_layout.hud_lives_banner_align);
        }
        if ((t = find_tag_start(hud_sec, hud_sec_end, "Menubar", &tend)) != NULL) {
            if (parse_xml_attr(t, tend, "livesX",  &g_layout.hud_menubar_lives_x))  g_layout.has_menubar_lives_x = true;
            if (parse_xml_attr(t, tend, "stageCX", &g_layout.hud_menubar_stage_cx)) g_layout.has_menubar_stage_cx = true;
            if (parse_xml_attr(t, tend, "scoreRX", &g_layout.hud_menubar_score_rx)) g_layout.has_menubar_score_rx = true;
            if (parse_xml_attr(t, tend, "zumaX",   &g_layout.hud_menubar_zuma_x))   g_layout.has_menubar_zuma_x = true;
            if (parse_xml_attr(t, tend, "menuX",   &g_layout.hud_menubar_menu_x))   g_layout.has_menubar_menu_x = true;
            parse_xml_attr_align(t, tend, "align", &g_layout.hud_menubar_align);
        }
        if ((t = find_tag_start(hud_sec, hud_sec_end, "MenubarMargins", &tend)) != NULL) {
            parse_xml_attr(t, tend, "top",    &g_layout.hud_menubar_margin_top);
            parse_xml_attr(t, tend, "bottom", &g_layout.hud_menubar_margin_bottom);
            parse_xml_attr(t, tend, "left",   &g_layout.hud_menubar_margin_left);
            parse_xml_attr(t, tend, "right",  &g_layout.hud_menubar_margin_right);
        }
        if ((t = find_tag_start(hud_sec, hud_sec_end, "LivesPos", &tend)) != NULL) {
            if (parse_xml_attr(t, tend, "x", &g_layout.hud_menubar_lives_x)) g_layout.has_menubar_lives_x = true;
            parse_xml_attr(t, tend, "y", &g_layout.hud_lives_y);
            parse_xml_attr(t, tend, "spacing", &g_layout.hud_lives_spacing);
        }
        if ((t = find_tag_start(hud_sec, hud_sec_end, "StageName", &tend)) != NULL) {
            if (parse_xml_attr(t, tend, "centerX", &g_layout.hud_menubar_stage_cx) ||
                parse_xml_attr(t, tend, "x", &g_layout.hud_menubar_stage_cx)) g_layout.has_menubar_stage_cx = true;
            parse_xml_attr(t, tend, "y", &g_layout.hud_stage_y);
        }
        if ((t = find_tag_start(hud_sec, hud_sec_end, "ScoreText", &tend)) != NULL) {
            if (parse_xml_attr(t, tend, "rightX", &g_layout.hud_menubar_score_rx) ||
                parse_xml_attr(t, tend, "x", &g_layout.hud_menubar_score_rx)) g_layout.has_menubar_score_rx = true;
            parse_xml_attr(t, tend, "y", &g_layout.hud_score_y);
        }
        if ((t = find_tag_start(hud_sec, hud_sec_end, "ProgressBar", &tend)) != NULL) {
            if (parse_xml_attr(t, tend, "x", &g_layout.hud_menubar_zuma_x)) g_layout.has_menubar_zuma_x = true;
            parse_xml_attr(t, tend, "y", &g_layout.hud_zuma_y);
            parse_xml_attr(t, tend, "w", &g_layout.hud_zuma_w);
            parse_xml_attr(t, tend, "h", &g_layout.hud_zuma_h);
        }
        if ((t = find_tag_start(hud_sec, hud_sec_end, "MenuBtn", &tend)) != NULL) {
            if (parse_xml_attr(t, tend, "x", &g_layout.hud_menubar_menu_x)) g_layout.has_menubar_menu_x = true;
            parse_xml_attr(t, tend, "y", &g_layout.hud_menu_y);
            parse_xml_attr(t, tend, "w", &g_layout.hud_menu_w);
            parse_xml_attr(t, tend, "h", &g_layout.hud_menu_h);
        }
    }

    /* Parse Stage Complete Cutscene */
    const char *sc_sec = strstr(buf, "<StageComplete");
    const char *sc_sec_end = sc_sec ? strstr(sc_sec, "</StageComplete>") : NULL;
    if (sc_sec && sc_sec_end) {
        if ((t = find_tag_start(sc_sec, sc_sec_end, "StageText", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.stage_complete_text_x);
            parse_xml_attr(t, tend, "y", &g_layout.stage_complete_text_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.stage_complete_text_align);
        }
        if ((t = find_tag_start(sc_sec, sc_sec_end, "CompleteText", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.stage_complete_banner_x);
            parse_xml_attr(t, tend, "y", &g_layout.stage_complete_banner_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.stage_complete_banner_align);
        }
        if ((t = find_tag_start(sc_sec, sc_sec_end, "FrogWriting", &tend)) != NULL) {
            parse_xml_attr(t, tend, "y", &g_layout.stage_complete_frog_writing_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.stage_complete_frog_writing_align);
        }
        if ((t = find_tag_start(sc_sec, sc_sec_end, "FrogTarget", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.stage_complete_frog_target_x);
            parse_xml_attr(t, tend, "y", &g_layout.stage_complete_frog_target_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.stage_complete_frog_target_align);
        }
        if ((t = find_tag_start(sc_sec, sc_sec_end, "FrogWaypoints", &tend)) != NULL) {
            parse_xml_attr(t, tend, "bx", &g_layout.stage_complete_frog_bx);
            parse_xml_attr(t, tend, "cx", &g_layout.stage_complete_frog_cx);
            parse_xml_attr(t, tend, "dx", &g_layout.stage_complete_frog_dx);
            parse_xml_attr(t, tend, "y", &g_layout.stage_complete_frog_waypoint_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.stage_complete_frog_waypoints_align);
        }
    }

    /* Parse Temple Cleared / Congratulations */
    const char *tc_sec = strstr(buf, "<TempleCleared");
    const char *tc_sec_end = tc_sec ? strstr(tc_sec, "</TempleCleared>") : NULL;
    if (tc_sec && tc_sec_end) {
        if ((t = find_tag_start(tc_sec, tc_sec_end, "ContinueBtn", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.congrats_btn_continue_x);
            parse_xml_attr(t, tend, "y", &g_layout.congrats_btn_continue_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.congrats_btn_align);
        }
        if ((t = find_tag_start(tc_sec, tc_sec_end, "Title", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.congrats_title_x);
            parse_xml_attr(t, tend, "y", &g_layout.congrats_title_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.congrats_title_align);
        }
        if ((t = find_tag_start(tc_sec, tc_sec_end, "TikiHead", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.congrats_tiki_x);
            parse_xml_attr(t, tend, "y", &g_layout.congrats_tiki_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.congrats_tiki_align);
        }
        if ((t = find_tag_start(tc_sec, tc_sec_end, "DialogText", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.congrats_speech_x);
            parse_xml_attr(t, tend, "y", &g_layout.congrats_speech_y);
            parse_xml_attr(t, tend, "width", &g_layout.congrats_speech_w);
            parse_xml_attr(t, tend, "height", &g_layout.congrats_speech_h);
            parse_xml_attr_align(t, tend, "align", &g_layout.congrats_speech_align);
        }
        if ((t = find_tag_start(tc_sec, tc_sec_end, "FrogMedallion", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.congrats_frog_medallion_x);
            parse_xml_attr(t, tend, "y", &g_layout.congrats_frog_medallion_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.congrats_frog_medallion_align);
        }
        if ((t = find_tag_start(tc_sec, tc_sec_end, "FrogReply", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.congrats_frog_reply_x);
            parse_xml_attr(t, tend, "y", &g_layout.congrats_frog_reply_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.congrats_frog_reply_align);
        }
    }

    /* Parse Credits Screen */
    const char *cred_sec = strstr(buf, "<CreditsScreen");
    const char *cred_sec_end = cred_sec ? strstr(cred_sec, "</CreditsScreen>") : NULL;
    if (cred_sec && cred_sec_end) {
        if ((t = find_tag_start(cred_sec, cred_sec_end, "WhateverBtn", &tend)) != NULL) {
            parse_xml_attr(t, tend, "offsetX", &g_layout.credits_btn_whatever_offset_x);
        }
        if ((t = find_tag_start(cred_sec, cred_sec_end, "CreditsText", &tend)) != NULL) {
            parse_xml_attr(t, tend, "centerX", &g_layout.credits_text_center_x);
        }
        if ((t = find_tag_start(cred_sec, cred_sec_end, "ScrollStart", &tend)) != NULL) {
            parse_xml_attr(t, tend, "y", &g_layout.credits_scroll_start_y);
        }
    }

    /* Parse Help Screen */
    const char *help_sec = strstr(buf, "<HelpScreen");
    const char *help_sec_end = help_sec ? strstr(help_sec, "</HelpScreen>") : NULL;
    if (help_sec && help_sec_end) {
        if ((t = find_tag_start(help_sec, help_sec_end, "HelpBack", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.help_back_x);
            parse_xml_attr(t, tend, "y", &g_layout.help_back_y);
        }
        if ((t = find_tag_start(help_sec, help_sec_end, "Skull", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.help_skull_x);
            parse_xml_attr(t, tend, "y", &g_layout.help_skull_y);
        }
        if ((t = find_tag_start(help_sec, help_sec_end, "Coin", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.help_coin_x);
            parse_xml_attr(t, tend, "y", &g_layout.help_coin_y);
        }
        if ((t = find_tag_start(help_sec, help_sec_end, "Frog", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.help_frog_x);
            parse_xml_attr(t, tend, "y", &g_layout.help_frog_y);
        }
        if ((t = find_tag_start(help_sec, help_sec_end, "TopText1", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.help_w1_top_text_x);
            parse_xml_attr(t, tend, "y", &g_layout.help_w1_top_text_y);
        }
        if ((t = find_tag_start(help_sec, help_sec_end, "TopText2", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.help_w2_top_text_x);
            parse_xml_attr(t, tend, "y", &g_layout.help_w2_top_text_y);
        }
        if ((t = find_tag_start(help_sec, help_sec_end, "SwapClick", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.help_swap_click_x);
            parse_xml_attr(t, tend, "y", &g_layout.help_swap_click_y);
        }
        if ((t = find_tag_start(help_sec, help_sec_end, "SwapMouse", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.help_swap_mouse_x);
            parse_xml_attr(t, tend, "y", &g_layout.help_swap_mouse_y);
        }
        if ((t = find_tag_start(help_sec, help_sec_end, "BallTarget", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.help_ball_target_x);
            parse_xml_attr(t, tend, "y", &g_layout.help_ball_target_y);
        }
        if ((t = find_tag_start(help_sec, help_sec_end, "DevTemple5", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.help_dev_temple5_x);
            parse_xml_attr(t, tend, "y", &g_layout.help_dev_temple5_y);
        }
        if ((t = find_tag_start(help_sec, help_sec_end, "DevMouseM", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.help_dev_mouse_m_x);
            parse_xml_attr(t, tend, "y", &g_layout.help_dev_mouse_m_y);
        }
        if ((t = find_tag_start(help_sec, help_sec_end, "DontShowCheckbox", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.help_checkbox_x);
            parse_xml_attr(t, tend, "y", &g_layout.help_checkbox_y);
        }
        if ((t = find_tag_start(help_sec, help_sec_end, "DoneBtn", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.help_done_btn_x);
            parse_xml_attr(t, tend, "y", &g_layout.help_done_btn_y);
            parse_xml_attr(t, tend, "w", &g_layout.help_done_btn_w);
            parse_xml_attr(t, tend, "h", &g_layout.help_done_btn_h);
        }
    }

    /* Parse Universal Dialogs */
    const char *dlg_sec = strstr(buf, "<Dialogs");
    const char *dlg_sec_end = dlg_sec ? strstr(dlg_sec, "</Dialogs>") : NULL;
    if (dlg_sec && dlg_sec_end) {
        if ((t = find_tag_start(dlg_sec, dlg_sec_end, "Dialog", &tend)) != NULL) {
            parse_xml_attr(t, tend, "minTop", &g_layout.dialog_min_top);
            parse_xml_attr_align(t, tend, "align", &g_layout.dialog_align);
        }
    }

    /* Parse DevTester HelpScreen & Cheats */
    const char *dev_help_sec = strstr(buf, "<DevHelpScreen");
    const char *dev_help_sec_end = dev_help_sec ? strstr(dev_help_sec, "</DevHelpScreen>") : NULL;
    if (dev_help_sec && dev_help_sec_end) {
        if ((t = find_tag_start(dev_help_sec, dev_help_sec_end, "HeaderTitle", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.dev_help_title_x);
            parse_xml_attr(t, tend, "y", &g_layout.dev_help_title_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.dev_help_title_align);
        }
        if ((t = find_tag_start(dev_help_sec, dev_help_sec_end, "HeaderSubtitle", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.dev_help_subtitle_x);
            parse_xml_attr(t, tend, "y", &g_layout.dev_help_subtitle_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.dev_help_subtitle_align);
        }
        if ((t = find_tag_start(dev_help_sec, dev_help_sec_end, "Columns", &tend)) != NULL) {
            parse_xml_attr(t, tend, "col1X", &g_layout.dev_help_col1_x);
            parse_xml_attr(t, tend, "col2X", &g_layout.dev_help_col2_x);
        }
        if ((t = find_tag_start(dev_help_sec, dev_help_sec_end, "AdvHeader", &tend)) != NULL) {
            parse_xml_attr(t, tend, "y", &g_layout.dev_help_adv_header_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.dev_help_adv_header_align);
        }
        if ((t = find_tag_start(dev_help_sec, dev_help_sec_end, "Temple5Preview", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.help_dev_temple5_x);
            parse_xml_attr(t, tend, "y", &g_layout.help_dev_temple5_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.dev_help_temple5_align);
        }
        if ((t = find_tag_start(dev_help_sec, dev_help_sec_end, "AdvBullets", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.dev_help_adv_text_x);
            parse_xml_attr(t, tend, "y", &g_layout.dev_help_adv_text_y);
            parse_xml_attr(t, tend, "spacing", &g_layout.dev_help_adv_text_spacing);
            parse_xml_attr_align(t, tend, "align", &g_layout.dev_help_adv_text_align);
        }
        if ((t = find_tag_start(dev_help_sec, dev_help_sec_end, "GauntHeader", &tend)) != NULL) {
            parse_xml_attr(t, tend, "y", &g_layout.dev_help_gaunt_header_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.dev_help_gaunt_header_align);
        }
        if ((t = find_tag_start(dev_help_sec, dev_help_sec_end, "GauntBullets", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.dev_help_gaunt_text_x);
            parse_xml_attr(t, tend, "y", &g_layout.dev_help_gaunt_text_y);
            parse_xml_attr(t, tend, "spacing", &g_layout.dev_help_gaunt_text_spacing);
            parse_xml_attr_align(t, tend, "align", &g_layout.dev_help_gaunt_text_align);
        }
        if ((t = find_tag_start(dev_help_sec, dev_help_sec_end, "CheatHeader", &tend)) != NULL) {
            parse_xml_attr(t, tend, "y", &g_layout.dev_help_cheat_header_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.dev_help_cheat_header_align);
        }
        if ((t = find_tag_start(dev_help_sec, dev_help_sec_end, "CheatBullets", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.dev_help_cheat_text_x);
            parse_xml_attr(t, tend, "y", &g_layout.dev_help_cheat_text_y);
            parse_xml_attr(t, tend, "spacing", &g_layout.dev_help_cheat_text_spacing);
            parse_xml_attr_align(t, tend, "align", &g_layout.dev_help_cheat_text_align);
        }
        if ((t = find_tag_start(dev_help_sec, dev_help_sec_end, "DemoBalls", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.dev_help_demo_balls_x);
            parse_xml_attr(t, tend, "y", &g_layout.dev_help_demo_balls_y);
            parse_xml_attr(t, tend, "spacing", &g_layout.dev_help_demo_balls_spacing);
            parse_xml_attr_align(t, tend, "align", &g_layout.dev_help_demo_balls_align);
        }
        if ((t = find_tag_start(dev_help_sec, dev_help_sec_end, "DemoMouse", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.dev_help_demo_mouse_x);
            parse_xml_attr(t, tend, "y", &g_layout.dev_help_demo_mouse_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.dev_help_demo_mouse_align);
        }
        if ((t = find_tag_start(dev_help_sec, dev_help_sec_end, "DemoClick", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.dev_help_demo_click_x);
            parse_xml_attr(t, tend, "y", &g_layout.dev_help_demo_click_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.dev_help_demo_click_align);
        }
        if ((t = find_tag_start(dev_help_sec, dev_help_sec_end, "DemoClear", &tend)) != NULL) {
            parse_xml_attr(t, tend, "x", &g_layout.dev_help_demo_clear_x);
            parse_xml_attr(t, tend, "y", &g_layout.dev_help_demo_clear_y);
            parse_xml_attr_align(t, tend, "align", &g_layout.dev_help_demo_clear_align);
        }
        if ((t = find_tag_start(dev_help_sec, dev_help_sec_end, "DevDialog", &tend)) != NULL) {
            parse_xml_attr(t, tend, "width", &g_layout.dev_dialog_width);
            parse_xml_attr(t, tend, "minTop", &g_layout.dev_dialog_min_top);
            parse_xml_attr_align(t, tend, "align", &g_layout.dev_dialog_align);
        }
    }

    free(buf);
    LogF("[ZRES] Successfully loaded and applied custom layout from properties/layout.xml.");
}

const ZRH_LayoutConfig* __cdecl ZRH_GetLayout(void) {
    return &g_layout;
}

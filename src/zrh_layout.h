#ifndef ZRH_LAYOUT_H
#define ZRH_LAYOUT_H

#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int valid;

    /* Adventure Map Temples */
    int adv_temple1_x,  adv_temple1_y;
    int adv_temple2_x,  adv_temple2_y;
    int adv_temple2v_x, adv_temple2v_y;
    int adv_temple3_x,  adv_temple3_y;
    int adv_temple3v_x, adv_temple3v_y;
    int adv_temple4_x,  adv_temple4_y;
    int adv_temple5_x,  adv_temple5_y; /* Secret Temple 5: The Void (Stage 13) */
    int adv_middle_x,   adv_middle_y;

    /* Adventure Map Doors (0..12)
     * Doors 0..11: Temples 1..4 (Stages 1..12)
     * Door 12: Secret Temple 5 Door 5 (Stage 13: The Void)
     * x1, y1 = Door Graphic coordinates (doorX, doorY)
     * x2, y2 = Golden Frog Waypoint coordinates (frogX, frogY)
     */
    struct { int x1, y1, x2, y2; } adv_doors[13];

    /* Adventure Navigation Buttons */
    int adv_btn_mainmenu_x, adv_btn_mainmenu_y;
    int adv_btn_play_x,     adv_btn_play_y;

    /* Adventure Labels & Highscores */
    int adv_title_x,        adv_title_y;
    int adv_hs_title_x,     adv_hs_title_y;
    int adv_bottom_box_x,   adv_bottom_box_y;
    int adv_stage_num_x,    adv_stage_num_y;
    int adv_stage_text_y,   adv_stage_center_x;
    int adv_subtitle_y,     adv_subtitle_center_x;
    int adv_hs_scores_x,    adv_hs_date_x, adv_hs_names_x;
    int adv_hs_base_y,      adv_hs_spacing, adv_hs_clip_w;
    int adv_infobox_x,      adv_infobox_y;
    int adv_infobox_w,      adv_infobox_h;
    int adv_infobox_line_spacing;

    /* Secret Stage 13 Banner */
    int adv_s13_badge_x,    adv_s13_badge_y;
    int adv_s13_num_x,      adv_s13_num_y;
    int adv_s13_title_x,    adv_s13_title_y;
    int adv_s13_sub_x,      adv_s13_sub_y;

    /* Adventure Coordinate Spaces (e.g. space="1200x675") */
    int adv_temples_space_w, adv_temples_space_h;
    int adv_doors_space_w,   adv_doors_space_h;
    int adv_buttons_space_w, adv_buttons_space_h;
    int adv_labels_space_w,  adv_labels_space_h;

    /* Gauntlet Coordinate Spaces (e.g. space="1200x675") */
    int gaunt_doors_space_w,   gaunt_doors_space_h;
    int gaunt_buttons_space_w, gaunt_buttons_space_h;
    int gaunt_labels_space_w,  gaunt_labels_space_h;

    /* Gauntlet Map Doors (0..3) */
    struct { int x, y; } gaunt_doors[4];

    /* Gauntlet Navigation Buttons */
    int gaunt_btn_practice_x, gaunt_btn_practice_y;
    int gaunt_btn_survival_x, gaunt_btn_survival_y;
    int gaunt_btn_back_x,     gaunt_btn_back_y;
    int gaunt_btn_mainmenu_x, gaunt_btn_mainmenu_y;
    int gaunt_btn_next_x,     gaunt_btn_next_y;
    int gaunt_btn_play_x,     gaunt_btn_play_y;

    /* Gauntlet Labels & Highscores */
    int gaunt_title_x,         gaunt_title_y;
    int gaunt_door_frame_x,    gaunt_door_frame_y;
    int gaunt_door_frame_w,    gaunt_door_frame_h;
    int gaunt_hs_title_x,      gaunt_hs_title_y;
    int gaunt_door_name_x,     gaunt_door_name_y;
    int gaunt_practice_text_x, gaunt_practice_text_y;
    int gaunt_practice_text_w;
    int gaunt_custom_map_x,    gaunt_custom_map_y;
    int gaunt_gem_x,           gaunt_gem_y;
    int gaunt_hs_scores_x,     gaunt_hs_names_x, gaunt_hs_clip_w;
    int gaunt_hs_base_y,       gaunt_hs_spacing;
    int gaunt_infobox_x,       gaunt_infobox_y;
    int gaunt_infobox_w,       gaunt_infobox_h;
    int gaunt_infobox_line_spacing;

    /* Main Menu Buttons & Frog Elements */
    int mm_btn1_x, mm_btn1_y;
    int mm_btn2_x, mm_btn2_y;
    int mm_btn3_x, mm_btn3_y;
    int mm_btn4_x, mm_btn4_y;
    int mm_btn5_x, mm_btn5_y;
    int mm_eyes_x, mm_eyes_y;
    int mm_hat_x, mm_hat_y;
    int mm_sun_x, mm_sun_y;
    int mm_eye_ray1_x, mm_eye_ray1_y;
    int mm_eye_ray2_x, mm_eye_ray2_y;
    int mm_eye_idle1_x, mm_eye_idle1_y;
    int mm_eye_idle2_x, mm_eye_idle2_y;
    int mm_welcome_y;
    int mm_user_switch_y;
    int mm_space_w, mm_space_h;

    /* More Games */
    int mg_t0_x, mg_t0_y, mg_t0_w, mg_t0_h;
    int mg_t1_x, mg_t1_y, mg_t1_w, mg_t1_h;
    int mg_t2_x, mg_t2_y, mg_t2_w, mg_t2_h;
    int mg_t3_x, mg_t3_y, mg_t3_w, mg_t3_h;
    int mg_btn_back_x, mg_btn_back_y;

    /* Loading Screen */
    int load_bar_x, load_bar_y;
    int load_click_play_y;
    int load_space_w, load_space_h;

    /* In-Game HUD & Banners */
    int hud_level_banner_offset_x, hud_level_banner_offset_y;
    int hud_lives_banner_center_x, hud_lives_banner_center_y;

    /* In-Game Stone Menubar Elements */
    int hud_menubar_lives_x;
    int hud_menubar_stage_cx;
    int hud_menubar_score_rx;
    int hud_menubar_zuma_x;
    int hud_menubar_menu_x;
    int hud_menubar_margin_bottom;
    int hud_menubar_margin_top;
    int hud_menubar_margin_right;
    int hud_menubar_margin_left;
    int hud_lives_y, hud_lives_spacing;
    int hud_stage_y;
    int hud_score_y;
    int hud_zuma_y, hud_zuma_w, hud_zuma_h;
    int hud_menu_y, hud_menu_w, hud_menu_h;
    /* Ruang koordinat InGameHUD & flag kehadiran atribut */
    int  hud_space_w, hud_space_h;
    bool has_menubar_lives_x, has_menubar_stage_cx, has_menubar_score_rx;
    bool has_menubar_zuma_x, has_menubar_menu_x;
    bool has_lives_banner_center_x, has_lives_banner_center_y;

    /* Stage Complete Cutscene */
    int stage_complete_text_x, stage_complete_text_y, stage_complete_text_align;
    int stage_complete_banner_x, stage_complete_banner_y, stage_complete_banner_align;
    int stage_complete_frog_writing_y, stage_complete_frog_writing_align;
    int stage_complete_frog_target_x, stage_complete_frog_target_y, stage_complete_frog_target_align;
    int stage_complete_frog_bx, stage_complete_frog_cx, stage_complete_frog_dx, stage_complete_frog_waypoint_y, stage_complete_frog_waypoints_align;

    /* Temple Cleared (Congratulations Dialog) */
    int congrats_btn_continue_x, congrats_btn_continue_y;
    int congrats_title_x, congrats_title_y;
    int congrats_tiki_x, congrats_tiki_y;
    int congrats_speech_x, congrats_speech_y, congrats_speech_w, congrats_speech_h;
    int congrats_frog_medallion_x, congrats_frog_medallion_y;
    int congrats_frog_reply_x, congrats_frog_reply_y;

    /* Credits Screen */
    int credits_btn_whatever_offset_x;
    int credits_text_center_x;
    int credits_scroll_start_y;

    /* Help / Instructions Screen */
    int help_back_x, help_back_y;
    int help_skull_x, help_skull_y;
    int help_coin_x, help_coin_y;
    int help_frog_x, help_frog_y;
    int help_w1_top_text_x, help_w1_top_text_y;
    int help_w2_top_text_x, help_w2_top_text_y;
    int help_swap_click_x, help_swap_click_y;
    int help_swap_mouse_x, help_swap_mouse_y;
    int help_ball_target_x, help_ball_target_y;
    int help_dev_temple5_x, help_dev_temple5_y;
    int help_dev_mouse_m_x, help_dev_mouse_m_y;
    int help_checkbox_x, help_checkbox_y;
    int help_done_btn_x, help_done_btn_y, help_done_btn_w, help_done_btn_h;

    /* Universal Dialog Centering */
    int dialog_min_top;
    int dialog_align;

    /* Alignments & Anchor Adjustments (0=default, 1=top-left, 2=top-middle, 3=top-right, 4=middle-left, 5=center, 6=middle-right, 7=bottom-left, 8=bottom-middle, 9=bottom-right) */
    int mm_btn1_align, mm_btn2_align, mm_btn3_align, mm_btn4_align, mm_btn5_align;
    int mm_eyes_align;
    int adv_btn_mainmenu_align, adv_btn_play_align;
    int adv_bottom_box_align, adv_s13_align;
    int gaunt_btn_practice_align, gaunt_btn_survival_align, gaunt_btn_back_align;
    int gaunt_btn_mainmenu_align, gaunt_btn_next_align, gaunt_btn_play_align;
    int hud_level_banner_align, hud_lives_banner_align, hud_menubar_align;
    int credits_whatever_align, credits_text_align;
    int congrats_btn_align, congrats_title_align;
    int congrats_tiki_align, congrats_speech_align, congrats_frog_medallion_align, congrats_frog_reply_align;

    /* DevTester HelpScreen (Tab 3) Full Visual & Layout Specifications */
    int dev_help_title_x, dev_help_title_y, dev_help_title_align;
    int dev_help_subtitle_x, dev_help_subtitle_y, dev_help_subtitle_align;
    int dev_help_col1_x, dev_help_col2_x;
    int dev_help_adv_header_y, dev_help_adv_header_align;
    int dev_help_temple5_align;
    int dev_help_adv_text_x, dev_help_adv_text_y, dev_help_adv_text_spacing, dev_help_adv_text_align;
    int dev_help_gaunt_header_y, dev_help_gaunt_header_align;
    int dev_help_gaunt_text_x, dev_help_gaunt_text_y, dev_help_gaunt_text_spacing, dev_help_gaunt_text_align;
    int dev_help_cheat_header_y, dev_help_cheat_header_align;
    int dev_help_cheat_text_x, dev_help_cheat_text_y, dev_help_cheat_text_spacing, dev_help_cheat_text_align;
    int dev_help_demo_balls_x, dev_help_demo_balls_y, dev_help_demo_balls_spacing, dev_help_demo_balls_align;
    int dev_help_demo_mouse_x, dev_help_demo_mouse_y, dev_help_demo_mouse_align;
    int dev_help_demo_click_x, dev_help_demo_click_y, dev_help_demo_click_align;
    int dev_help_demo_clear_x, dev_help_demo_clear_y, dev_help_demo_clear_align;
    int dev_dialog_width, dev_dialog_min_top, dev_dialog_align;
} ZRH_LayoutConfig;

static inline int ZRH_ParseAlignAttr(const char *val) {
    if (!val) return 0;
    while (*val == ' ' || *val == '\t' || *val == '"' || *val == '\'') val++;
    if (_strnicmp(val, "top-left", 8) == 0 || _strnicmp(val, "tl", 2) == 0) return 1;
    if (_strnicmp(val, "top-middle", 10) == 0 || _strnicmp(val, "top-center", 10) == 0 || _strnicmp(val, "tc", 2) == 0 || _strnicmp(val, "tm", 2) == 0) return 2;
    if (_strnicmp(val, "top-right", 9) == 0 || _strnicmp(val, "tr", 2) == 0) return 3;
    if (_strnicmp(val, "middle-left", 11) == 0 || _strnicmp(val, "center-left", 11) == 0 || _strnicmp(val, "ml", 2) == 0 || _strnicmp(val, "cl", 2) == 0) return 4;
    if (_strnicmp(val, "middle-center", 13) == 0 || _strnicmp(val, "center", 6) == 0 || _strnicmp(val, "middle", 6) == 0 || _strnicmp(val, "c", 1) == 0) return 5;
    if (_strnicmp(val, "middle-right", 12) == 0 || _strnicmp(val, "center-right", 12) == 0 || _strnicmp(val, "mr", 2) == 0 || _strnicmp(val, "cr", 2) == 0) return 6;
    if (_strnicmp(val, "bottom-left", 11) == 0 || _strnicmp(val, "bl", 2) == 0) return 7;
    if (_strnicmp(val, "bottom-middle", 13) == 0 || _strnicmp(val, "bottom-center", 13) == 0 || _strnicmp(val, "bc", 2) == 0 || _strnicmp(val, "bm", 2) == 0) return 8;
    if (_strnicmp(val, "bottom-right", 12) == 0 || _strnicmp(val, "br", 2) == 0) return 9;
    return 0;
}

static inline void ZRH_ResolveAnchorPos(int in_x, int in_y, int align, int W, int H, float scale, int *out_x, int *out_y) {
    switch (align) {
        case 1: /* top-left */
            if (out_x) *out_x = (int)((float)in_x * scale + 0.5f);
            if (out_y) *out_y = (int)((float)in_y * scale + 0.5f);
            break;
        case 2: /* top-middle / top-center */
            if (out_x) *out_x = W / 2 + (int)((float)(in_x - 320) * scale + 0.5f);
            if (out_y) *out_y = (int)((float)in_y * scale + 0.5f);
            break;
        case 3: /* top-right */
            if (out_x) *out_x = W - (int)((float)(640 - in_x) * scale + 0.5f);
            if (out_y) *out_y = (int)((float)in_y * scale + 0.5f);
            break;
        case 4: /* middle-left / center-left */
            if (out_x) *out_x = (int)((float)in_x * scale + 0.5f);
            if (out_y) *out_y = H / 2 + (int)((float)(in_y - 240) * scale + 0.5f);
            break;
        case 5: /* center / middle */
            if (out_x) *out_x = W / 2 + (int)((float)(in_x - 320) * scale + 0.5f);
            if (out_y) *out_y = H / 2 + (int)((float)(in_y - 240) * scale + 0.5f);
            break;
        case 6: /* middle-right / center-right */
            if (out_x) *out_x = W - (int)((float)(640 - in_x) * scale + 0.5f);
            if (out_y) *out_y = H / 2 + (int)((float)(in_y - 240) * scale + 0.5f);
            break;
        case 7: /* bottom-left */
            if (out_x) *out_x = (int)((float)in_x * scale + 0.5f);
            if (out_y) *out_y = H - (int)((float)(480 - in_y) * scale + 0.5f);
            break;
        case 8: /* bottom-middle / bottom-center */
            if (out_x) *out_x = W / 2 + (int)((float)(in_x - 320) * scale + 0.5f);
            if (out_y) *out_y = H - (int)((float)(480 - in_y) * scale + 0.5f);
            break;
        case 9: /* bottom-right */
            if (out_x) *out_x = W - (int)((float)(640 - in_x) * scale + 0.5f);
            if (out_y) *out_y = H - (int)((float)(480 - in_y) * scale + 0.5f);
            break;
        default: /* 0: standard proportional with letterbox/pillarbox offset */
            if (out_x) {
                int cw = (int)(640.0f * scale + 0.5f);
                int xo = (W - cw) / 2;
                *out_x = (int)((float)in_x * scale + 0.5f) + xo;
            }
            if (out_y) {
                int ch = (int)(480.0f * scale + 0.5f);
                int yo = (H - ch) / 2;
                *out_y = (int)((float)in_y * scale + 0.5f) + yo;
            }
            break;
    }
}

extern ZRH_LayoutConfig g_layout;

void ZRH_InitLayoutDefaults(void);
void ZRH_LoadLayoutXml(void);
__declspec(dllexport) const ZRH_LayoutConfig* __cdecl ZRH_GetLayout(void);

#ifdef __cplusplus
}
#endif

#endif /* ZRH_LAYOUT_H */

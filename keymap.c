/* Copyright 2020 Naoki Katahira
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H
#include <action.h>
#include <math.h>
#include <stdio.h>

// Required for RPC communication functions
#include <split_util.h>
#include <transactions.h>

// Required Debugging & Printing
// #include <print.h>
// bool debug_ms_reports = false;       // Debug mouse reports

// Example build commands:
// make lily58/rev1:via:flash -e POINTING_DEVICE=trackball_trackball -e POINTING_DEVICE_POSITION=left -j8
// make lily58/rev1:via:flash -e POINTING_DEVICE=trackball_trackball -e POINTING_DEVICE_POSITION=right -j8

uint8_t     TLAY_LAY;
uint16_t    TLAY_REL;
bool        TLAY_PEN = false;

bool        BTN_SWAP = false;       // If true, swap the behavior of O_ & I_ keycodes
float       GROWTH_FACTOR = 8;      // Moved here to retain value across key presses
uint8_t     RGB_CURRENT;

bool        RGB_MS_MODE = false;    // RGB Emulation Mode Arrow/Scroll
uint16_t    RGB_MS_MOVE;            // Holds Last Move Time
// Auto Mouse Layer Variables
bool        ATML = false;           // Off by Default
bool        ATML_Active = false;
uint16_t    ATML_Timer;
// Backspace repeating hold
bool        BS_HOL;
bool        BS_REL;
uint16_t    BS_TIM;
uint8_t     KEY_MATRIX;
// Global limiter to prevent excessive timer_read()'s
#define     TIMER_LIMITER 501
#define     ATML_TIMEOUT 2000       // Auto Mouse Layer Timeout
#define     RGB_MS_TIMEOUT 2000     // Mouse Mode Timeout

// Structs for handle_mouse_buttons()
typedef enum {
    MODE_OFF,
    MODE_PENDING, // waiting to see if next tap is coming
    MODE_ARROW,
    MODE_SCROLL
} emu_mode_t;

typedef struct {
    uint16_t        last_press_time;
    bool            button_was_pressed;
    emu_mode_t      mode;
} btn_state_t;

static btn_state_t  left_button  = {0, false, MODE_OFF};
static btn_state_t  right_button = {0, false, MODE_OFF};

// Maybe bad idea, but lets try it
// Turn off tap layer after timeout in matrix_scan_user()
static void layer_jump_timeout(void) {
    layer_off(1);
    layer_off(2);
    layer_off(3);
    layer_off(4);
    TLAY_LAY = 0;
    TLAY_PEN = false;
}

static bool layer_jump_handler(        // Tap into another layer using the same key
    uint16_t        tap_key,    // which keycode to tap if quick press
    uint8_t         layer,      // which layer to activate
    uint16_t*       timer,      // pointer to a timer variable (e.g., &bspc_l1_timer)
    bool            condition,  // true = enter layer/tap branch, false = send KC_SPC
    keyrecord_t*    record) {

    if (record->event.pressed) {
        if (condition) {
            *timer = timer_read();
            layer_off(layer == 1 ? 2 : 1); // turn off the "other" layer
            layer_on(layer);
            TLAY_LAY = layer;
            TLAY_PEN = false;
        } else {
            register_code(KC_SPC);
        }
    } else {
        if (condition) {
            TLAY_REL = timer_read();
            TLAY_PEN = true;
            if (timer_elapsed(*timer) < BW_TAP_TIME) {
                tap_code(tap_key);
            }
        } else {
            unregister_code(KC_SPC);
        }
    }
    return false;
}

// Helper for tap-hold layer changing
static bool layer_tap_handler(
    uint16_t        tap_key,    // Tap Key
    uint8_t         layer,      // Hold Layer
    uint16_t*       timer,      // Timer
    uint16_t        tap_time,   // Tap Time
    keyrecord_t*    record) {

    if (record->event.pressed) {
        *timer = timer_read();
        layer_on(layer);
    } else {
        layer_off(layer);
        if (timer_elapsed(*timer) < tap_time) {
            tap_code(tap_key);
        }
    }
    return false;
}

// Helper for tap-hold keys that send tap_key if tapped, else hold_key if held
static bool tap_hold_handler(
    uint16_t        tap_key,    // Tap Key
    uint16_t        alt_key,    // Hold Key
    uint16_t*       timer,      // Timer
    bool            condition,  // Bool Variable
    bool            mb,         // flag to remove MouseButton timer
    keyrecord_t*    record) {

    if (timer == NULL) {
        // Multi purpose
        // If no timer provided, just do simple tap/hold without timing
        // Resets Mouse Mode timer in handle_mouse_mode_rgb()
        if (condition) {
            record->event.pressed ? register_code(tap_key) : unregister_code(tap_key);
        } else {
            record->event.pressed ? register_code(alt_key) : unregister_code(alt_key);
            if (mb && timer_elapsed(RGB_MS_MOVE) > TIMER_LIMITER) {
            RGB_MS_MOVE = timer_read();
        }
        }
        return false;
    } else {
        // If timer provided, do timed tap/hold behavior
        if (record->event.pressed) {
            *timer = timer_read();
        } else {
            if (timer_elapsed(*timer) < BW_TAP_TIME) {
                tap_code(tap_key);
            } else {
                tap_code(alt_key);
            }
        }
        return false;
    }
}

enum custom_keycodes {
    O_CAP_L1 = SAFE_RANGE,  // 64
    O_SPC_L2,               // 65
    I_CAP_L1,               // 66
    I_SPC_L2,               // 67
    BW_CAP_L3,              // 68
    BW_TAB_L3,              // 69
    FX_SLV_M,               // 70
    FX_SLV_P,               // 71
    R_MB1,                  // 72
    R_MB2,                  // 73
    L_MB1,                  // 74
    L_MB2,                  // 75
    BW_ESC_GRV,             // 76
    PLS_BSPC,               // 77
    B_SWAP,                 // 78
    BLANK_SPACE_HOLDER,     // 79
    R_RBRC,                 // 80
    L_LBRC,                 // 81
    ML_AUTO,                // 82
    LC_LS,                  // 83
    LS_LC,                  // 84
    S1_ESC,                 // 85
    S2_1,                   // 86
    S3_2,                   // 87
    S4_3,                   // 88
    S5_4,                   // 89
    S6_5,                   // 90
    BSPC_H,                 // 91
    SEL_H                  // 92
//    MS_DEBUG,               // 79
};

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    static uint16_t le1_timer;
    static uint16_t le2_timer;
    static uint16_t ri1_timer;
    static uint16_t ri2_timer;

    switch (keycode) {

        case O_CAP_L1:
            return layer_jump_handler(KC_CAPS, 1, &le1_timer, BTN_SWAP, record);

        case O_SPC_L2:
            return layer_jump_handler(KC_SPC, 2, &ri1_timer, BTN_SWAP, record);

        case I_CAP_L1:
            return layer_jump_handler(KC_CAPS, 1, &le1_timer, !BTN_SWAP, record);

        case I_SPC_L2:
            return layer_jump_handler(KC_SPC, 2, &ri1_timer, !BTN_SWAP, record);

        case BW_CAP_L3:
            return layer_tap_handler(KC_CAPS, 4, &le2_timer, BW_TAP_TIME, record);

        case BW_TAB_L3:
            return layer_tap_handler(KC_TAB, 4, &ri2_timer, BW_TAP_TIME, record);

        case FX_SLV_M: // Reduce growth factor
            if (record->event.pressed) {
                GROWTH_FACTOR /= 2;
            }
            return false;

        case FX_SLV_P: // Increase growth factor
            if (record->event.pressed) {
                GROWTH_FACTOR *= 2;
            }
            return false;
        // Group for mouse buttons with tap-hold behavior
        // _MB* are toggled by RGB_MS_MODE to change to mouse buttons on mouse move
        case R_MB1:
            return tap_hold_handler(KC_Y, KC_MS_BTN1, NULL, !RGB_MS_MODE, true, record);

        case R_MB2:
            return tap_hold_handler(KC_U, KC_MS_BTN2, NULL, !RGB_MS_MODE, true, record);

        case L_MB1:
            return tap_hold_handler(KC_T, KC_MS_BTN1, NULL, !RGB_MS_MODE, true, record);

        case L_MB2:
            return tap_hold_handler(KC_R, KC_MS_BTN2, NULL, !RGB_MS_MODE, true, record);

        case BW_ESC_GRV:
            return tap_hold_handler(KC_ESC, KC_GRV, &ri1_timer, NULL, NULL, record);

        case PLS_BSPC:
            return tap_hold_handler(KC_BSPC, KC_EQL, &ri1_timer, NULL, NULL, record);
        // Toggle Button Swap for MB* keys
        case B_SWAP:
            if (record->event.pressed) {
                BTN_SWAP = !BTN_SWAP;
                layer_jump_timeout();
                if (is_keyboard_master()) {
                    uint8_t msg[2] = {2, BTN_SWAP ? 1 : 0};
                    transaction_rpc_send(USER_SYNC, sizeof(msg), msg);
                }
            }
            return false;

        case BLANK_SPACE_HOLDER:
            return false;
        // Bracket keys with tap-hold layer switching
        case R_RBRC:
            return layer_tap_handler(KC_RBRC, 2, &ri1_timer, 250, record);

        case L_LBRC:
            return layer_tap_handler(KC_LBRC, 1, &le1_timer, 250, record);
        // Auto Mouse Layer Toggle
        case ML_AUTO:
            if (record->event.pressed) {
                ATML = !ATML;
            }
            layer_jump_timeout();
            return false;
        // Swap Keys
        case LC_LS:
            return tap_hold_handler(KC_LCTL, KC_LSFT, NULL, !BTN_SWAP, false, record);

        case LS_LC:
            return tap_hold_handler(KC_LSFT, KC_LCTL, NULL, !BTN_SWAP, false, record);

        case S1_ESC:
            return tap_hold_handler(KC_1, KC_ESC, NULL, !BTN_SWAP, false, record);

        case S2_1:
            return tap_hold_handler(KC_2, KC_1, NULL, !BTN_SWAP, false, record);

        case S3_2:
            return tap_hold_handler(KC_3, KC_2, NULL, !BTN_SWAP, false, record);

        case S4_3:
            return tap_hold_handler(KC_4, KC_3, NULL, !BTN_SWAP, false, record);

        case S5_4:
            return tap_hold_handler(KC_5, KC_4, NULL, !BTN_SWAP, false, record);

        case S6_5:
            return tap_hold_handler(KC_6, KC_5, NULL, !BTN_SWAP, false, record);

        case BSPC_H:
            if (record->event.pressed) {
                BS_TIM = timer_read();
                BS_HOL = true;
                KEY_MATRIX = 0;
            } else {
                if (timer_elapsed(BS_TIM) < TAPPING_TERM) {
                    tap_code(KC_EQL); // Tapped and released quickly: send '9'
                    BS_HOL = false;
                }
                BS_REL = true;
            }
            return false; // Skip default handling

        case SEL_H:
            if (record->event.pressed) {
                right_button.mode = MODE_ARROW;
                BS_TIM = timer_read();
                BS_HOL = true;
                KEY_MATRIX = 1;
            } else {
                if (timer_elapsed(BS_TIM) < TAPPING_TERM) {
                    tap_code(KC_H); // Tapped and released quickly: send '9'
                    BS_HOL = false;
                }
                BS_REL = true;
                right_button.mode = MODE_OFF;
            }
            return false; // Skip default handling
/*
        case MS_DEBUG:
            if (record->event.pressed) {
                debug_enable = !debug_enable;  // Toggle debug output
                if (debug_enable) {
                    uprintf("Debug Enabled\n");
                } else {
                    uprintf("Debug Disabled\n");
                }
            }
            return false;
*/
    }
    return true;
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
[0] = LAYOUT(
/* QWERTY
   ,------------+------------+------------+------------+------------+------------.                                      ,------------+------------+------------+------------+------------+------------.
   |     1      |     2      |     3      |     4      |     5      |     6      |                                      |      7     |     8      |     9      |     10     |     11     |     12     |

*/        S1_ESC,        S2_1,        S3_2,        S4_3,        S5_4,        S6_5,                                               KC_7,        KC_8,        KC_9,        KC_0,     KC_MINS,    PLS_BSPC,
/* |    Esc     |     1      |     2      |     3      |     4      |     5      |                                      |            |            |            |            |            |     +      |
   |------------+------------+------------+------------+------------+------------|                                      |------------+------------+------------+------------+------------+------------|
         Tab          Q            W            E            R            T                                                    Y           U            I            O            P            -

*/        KC_TAB,        KC_Q,        KC_W,        KC_E,       L_MB2,       L_MB1,                                              R_MB1,       R_MB2,        KC_I,        KC_O,        KC_P,     KC_BSLS,
/* |            |            |            |            |     MB2    |     MB1    |                                      |     MB1    |     MB2    |            |            |            |            |
   |------------+------------+------------+------------+------------+------------|                                      |------------+------------+------------+------------+------------+------------|
        lCTRL         A            S            D            F            G                                                    H           J            K             L            ;           '

*/         LC_LS,        KC_A,        KC_S,        KC_D,        KC_F,        KC_G,                                              KC_H ,        KC_J,        KC_K,        KC_L,     KC_SCLN,     KC_QUOT,
/* |   lShift   |            |            |            |            |            |-------------.          ,-------------|            |            |            |            |            |            |
   |------------+------------+------------+------------+------------+------------|     [       |          |      ]      |------------+------------+------------+------------+------------+------------|
       LShift         Z            X            C            V            B                                                    N           M            ,             .            /         RShift
                                                                                       L1                       l2
*/         LS_LC,        KC_Z,        KC_X,        KC_C,        KC_V,        KC_B,        L_LBRC,                 R_RBRC,        KC_N,        KC_M,     KC_COMM,      KC_DOT,     KC_SLSH,MT(MOD_RSFT,KC_ENT),
/* |    LCtrl   |            |            |            |            |            |-------------|          |-------------|            |            |            |            |            |    Enter   |
   `------------+------------+---------+--+---------+--+---------+--+------------|             /           \            \------------+---+--------+---+--------+---+--------+------------+------------'
                                            LAlt         LGUI        Space            Caps                       Space           Space         ESC       Backspace
                                       |            |            |            | /   L1 / L2   /             \   L2 / L1  \  |            |            |            |
                                       |            |            |            |/             /               \            \ |            |            |            |
*/                               MT(MOD_LALT,KC_DEL),    KC_LGUI,    I_CAP_L1,      O_CAP_L1,                     O_SPC_L2,     I_SPC_L2,  BW_ESC_GRV,      KC_BSPC
/*                                     `------------+------------+------------+-------------'                 '------------''------------+------------+------------'
*/),
[1] = LAYOUT(
/* LOWER
   ,------------+------------+------------+------------+------------+------------.                                      ,------------+------------+------------+------------+------------+------------.
   |     F1     |     F2     |     F3     |     F4     |     F5     |     F6     |                                      |     F7     |     F8     |     F9     |     F10    |    F11     |    F12     |

*/         KC_F1,       KC_F2,       KC_F3,       KC_F4,       KC_F5,       KC_F6,                                              KC_F7,       KC_F8,       KC_F9,      KC_F10,      KC_F11,      KC_F12,
/* |            |            |            |            |            |            |                                      |            |            |            |            |            |            |
   |------------+------------+------------+------------+------------+------------|                                      |------------+------------+------------+------------+------------+------------|
       Button       Desktop      Desktop        Up         Volume       Volume                                                 /           7            8            9             -           +
        Swap         Left         Right                     Down          Up                                                                                                       _           =
*/       B_SWAP,C(G(KC_LEFT)),C(G(KC_RGHT)),      KC_UP,     KC_VOLD,     KC_VOLU,                                            KC_PSLS,       KC_P7,       KC_P8,       KC_P9,     KC_MINS,      KC_EQL,
/* |            |            |            |            |            |            |                                      |            |            |            |            |            |            |
   |------------+------------+------------+------------+------------+------------|                                      |------------+------------+------------+------------+------------+------------|
        Task         Redo         Left         Down         Right       Prev                                                   *           4            5            6             +
       Manager
*/  C(S(KC_ESC)),     C(KC_Y),     KC_LEFT,     KC_DOWN,     KC_RGHT,     KC_MPRV,                                            KC_ASTR,       KC_P4,       KC_P5,       KC_P6,     KC_PPLS,       KC_NO,
/* |            |            |            |            |            |            |-------------.          ,-------------|            |            |            |            |            |            |
   |------------+------------+------------+------------+------------+------------|     Play    |          |     Mute    |------------+------------+------------+------------+------------+------------|
       LShift        Undo         Cut          Copy         Paste       Next                                                               1            2            3             .         RShift

*/       KC_LSFT,     C(KC_Z),     C(KC_X),     C(KC_C),     C(KC_V),     KC_MNXT,      KC_MPLY,                 KC_MUTE,       KC_NO,       KC_P1,       KC_P2,       KC_P3,     KC_PDOT,     KC_RSFT,
/* |            |            |            |            |            |            |-------------|          |-------------|            |            |            |            |            |            |
   `------------+------------+---------+--+---------+--+---------+--+------------|             /           \            \------------+---+--------+---+--------+---+--------+------------+------------'
                                                         LGUI         Space          Space                       Tab             Tab            0
                                       |            |            |            | /     L2      /             \     L3     \  |   Space    |            |            |
                                       |            |            |            |/             /               \            \ |            |            |            |
*/ 	                                         KC_TRNS,     KC_TRNS,    I_SPC_L2,     O_SPC_L2,                    BW_TAB_L3,     BW_TAB_L3,       KC_P0,     KC_TRNS
/*                                     `------------+------------+------------+-------------'                 '------------''------------+------------+------------'
*/),
[2] = LAYOUT(
/* UPPER
   ,------------+------------+------------+------------+------------+------------.                                      ,------------+------------+------------+------------+------------+------------.
   |   Reset    |    Auto    |            |            |            |            |                                      |    Back    |   Forth    |    Play    |     Min    |    Max     |   Close    |
      EEPROM      Mouse Layer                                                                                                                         Pause        Window       Window      Window
*/        EE_CLR,     ML_AUTO,       KC_NO,       KC_NO,       KC_NO,       KC_NO,                                        KC_WWW_BACK,KC_WWW_FORWARD,   KC_MPLY,  G(KC_DOWN),    G(KC_UP),    A(KC_F4),
/* |            |            |            |            |            |            |                                      |            |            |            |            |            |            |
   |------------+------------+------------+------------+------------+------------|                                      |------------+------------+------------+------------+------------+------------|
       Button                                   Up                                                                          Last          F12           Up          Left         Right        Close
        Swap                                                                                                                Window                                  Tab           Tab          Tab
*/        B_SWAP,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,                                          A(KC_TAB),      KC_F11,       KC_UP, RCS(KC_TAB),RCTL(KC_TAB),  RCTL(KC_W),
/* |            |            |            |            |            |            |                                      |            |            |            |            |            |            |
   |------------+------------+------------+------------+------------+------------|                                      |------------+------------+------------+------------+------------+------------|
                      Redo        Left        Down          Right                                                           Cycle         Left         Down         Right         Tab         Undo
                                                                                                                            Window                                              Window        Tab
*/         KC_NO,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,                                          A(KC_ESC),     KC_LEFT,     KC_DOWN,     KC_RGHT, LCA(KC_TAB),   RCS(KC_T),
/* |            |            |            |            |            |            |-------------.          ,-------------|            |            |            |            |            |            |
   |------------+------------+------------+------------+------------+------------|             |          |             |------------+------------+------------+------------+------------+------------|
       LShift        Undo         Cut          Copy         Paste                                                            Undo         Redo         Paste       Volume       Volume       Taskbar
                                                                                                                                                                    Down          Up         Buttons
*/       KC_LSFT,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     C(KC_Y),        KC_NO,                   KC_NO,     C(KC_Z),     C(KC_Y),     C(KC_V),     KC_VOLU,     KC_VOLD,     G(KC_T),
/* |            |            |            |            |            |            |-------------|          |-------------|            |            |            |            |            |            |
   `------------+------------+---------+--+---------+--+---------+--+------------|             /           \            \------------+---+--------+---+--------+---+--------+------------+------------'
                                                                      Caps            Caps                      Caps             Caps
                                       |            |            |    Space   | /     L3      /             \     L1     \  |    Space   |            |            |
                                       |            |            |            |/             /               \            \ |            |            |            |
*/ 	                                         KC_TRNS,     KC_TRNS,   BW_CAP_L3,    BW_CAP_L3,                     O_CAP_L1,      I_CAP_L1,     KC_TRNS,     KC_TRNS
/*                                     `------------+------------+------------+-------------'                 '------------''------------+------------+------------'
*/),
[3] = LAYOUT(
/* AUTO MOUSE MOVE LAYER                                           * Need to work on dual function keys for this layer * RCS(KC_TAB)   RCTL(KC_TAB)
   ,------------+------------+------------+------------+------------+------------.                                      ,------------+------------+------------+------------+------------+------------.
   |   Reset    |    Auto    |            |            |            |            |                                      |    Back    |   Forth    |    Close   |     Min    |    Max     |   Close    |
      EEPROM      Mouse Layer                                                                                                                           Tab        Window       Window      Window
*/QK_CLEAR_EEPROM,    ML_AUTO,       KC_NO,       KC_NO,       KC_NO,       KC_NO,                                        KC_WWW_BACK,KC_WWW_FORWARD,RCTL(KC_W),  G(KC_DOWN),    G(KC_UP),    A(KC_F4),
/* |            |            |            |            |            |            |                                      |Left Tab    |Right Tab   | Play Pause |            |            |            |
   |------------+------------+------------+------------+------------+------------|                                      |------------+------------+------------+------------+------------+------------|
       Button                                   Up                                                                          MB1          MB2           MB3
        Swap
*/        B_SWAP,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,                                         KC_MS_BTN1,  KC_MS_BTN2,  KC_MS_BTN2,       KC_NO,       KC_NO,       KC_NO,
/* |            |            |            |            |            |            |                                      |            |            |            |            |            |            |
   |------------+------------+------------+------------+------------+------------|                                      |------------+------------+------------+------------+------------+------------|
                      Redo        Left        Down          Right                                                            Last         Cycle         Down         Right         Tab         Undo
                                                                                                                            Window       Window                                   Window        Tab
*/         KC_NO,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,                                          A(KC_TAB),    A(KC_ESC),     KC_DOWN,     KC_RGHT, LCA(KC_TAB),   RCS(KC_T),
/* |            |            |            |            |            |            |-------------.          ,-------------|            |            |            |            |            |            |
   |------------+------------+------------+------------+------------+------------|GROWTH_FACTOR|          |GROWTH_FACTOR|------------+------------+------------+------------+------------+------------|
       LShift        Undo         Cut          Copy         Paste                                                            Undo         Redo         Paste       Volume       Volume       Taskbar
                                                                                                                                                                    Down          Up         Buttons
*/       KC_LSFT,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     C(KC_Y),     FX_SLV_M,                FX_SLV_P,     C(KC_Z),     C(KC_Y),     C(KC_V),     KC_VOLU,     KC_VOLD,     G(KC_T),
/* |            |            |            |            |            |            |-------------|          |-------------|            |            |            |            |            |            |
   `------------+------------+---------+--+---------+--+---------+--+------------|             /           \            \------------+---+--------+---+--------+---+--------+------------+------------'

                                       |            |            |            | /             /             \            \  |            |            |            |
                                       |            |            |            |/             /               \            \ |            |            |            |
*/ 	                                         KC_TRNS,     KC_TRNS,     KC_TRNS,      KC_TRNS,                      KC_TRNS,       KC_TRNS,     KC_TRNS,     KC_TRNS
/*                                     `------------+------------+------------+-------------'                 '------------''------------+------------+------------'
*/),
[4] = LAYOUT(
/* SETTINGS LAYER                                               * Need to add variable modifiers to change what to increment on hold *
   ,------------+------------+------------+------------+------------+------------.                                      ,------------+------------+------------+------------+------------+------------.
   |   Reset    |    Auto    |            |            |            |ATML_TIMEOUT|    ARROW_MOMENTUM                    |            |            |            |            |            |            |
      EEPROM      Mouse Layer                                                         ARROW_STEP
*/QK_CLEAR_EEPROM,    ML_AUTO,       KC_NO,       KC_NO,       KC_NO,       KC_NO,                                            KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,
/* |            |            |            |            |            |            |    SCROLL_DIVISOR_H                  |            |            |            |            |            |            |
   |------------+------------+------------+------------+------------+------------|    SCROLL_DIVISOR_H                  |------------+------------+------------+------------+------------+------------|
       Button                                                       RGB_MS_TIMEOUT
        Swap
*/        B_SWAP,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,                                   KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,
/* |            |            |            |            |            |            |       MOMENTUM                       |            |            |            |            |            |            |
   |------------+------------+------------+------------+------------+------------|       MIN_SCALE                      |------------+------------+------------+------------+------------+------------|
   |            |            |            |            |            |            |       MAX_SCALE                      |            |            |            |            |            |            |

*/       KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,                                            KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,
/* |            |            |            |            |            |            |-------------.          ,-------------|            |            |            |            |            |            |
   |------------+------------+------------+------------+------------+------------|GROWTH_FACTOR|          |GROWTH_FACTOR|------------+------------+------------+------------+------------+------------|
       LShift        Undo         Cut          Copy         Paste                                                        Incrementer  Incremeneter
                                                                                        -                        +             -            +
*/       KC_LSFT,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     FX_SLV_M,                FX_SLV_P,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,     KC_TRNS,
/* |            |            |            |            |            |            |-------------|          |-------------|            |            |            |            |            |            |
   `------------+------------+---------+--+---------+--+---------+--+------------|             /           \            \------------+---+--------+---+--------+---+--------+------------+------------'

                                       |            |            |            | /             /             \            \  |            |            |            |
                                       |            |            |            |/             /               \            \ |            |            |            |
*/ 	                                         KC_TRNS,     KC_TRNS,     KC_TRNS,      KC_TRNS,                      KC_TRNS,       KC_TRNS,     KC_TRNS,     KC_TRNS
/*                                     `------------+------------+------------+-------------'                 '------------''------------+------------+------------'
*/)
};

/* Debugging Mouse Reports
// Pass the mouse reports to this function to print debug info if debug is enabled
void debug_mouse_reports(report_mouse_t left_report, report_mouse_t right_report) {
        uint16_t t = timer_read();
        (void)t;  // Tell compiler t is intentionally unused (suppresses warning)
        if (left_report.x || left_report.y || left_report.h || left_report.v || left_report.buttons) {
            uprintf("[%lu] LEFT: x=%d y=%d h=%d v=%d buttons=0x%02X\n",
                    (unsigned long)t, left_report.x, left_report.y,
                    left_report.h, left_report.v, left_report.buttons);
        }
        if (right_report.x || right_report.y || right_report.h || right_report.v || right_report.buttons) {
            uprintf("[%lu] RIGHT: x=%d y=%d h=%d v=%d buttons=0x%02X\n",
                    (unsigned long)t, right_report.x, right_report.y,
                    right_report.h, right_report.v, right_report.buttons);
        }
}
*/

// ------------------------------- //
//   RGB Layer Synchronization RPC //
// ------------------------------- //

// Set RGBW color of the Pimoroni Trackball based on the active layer.
// Called by the master device when the layer changes to update itself and the slave devices.
void set_trackball_rgb_for_layer(uint8_t layer) {
    switch (layer) {
        case 0:
            if (BTN_SWAP) {
                pimoroni_trackball_set_rgbw(255, 255, 255, 0);  // White (base layer, swapped)
            } else {
                pimoroni_trackball_set_rgbw(0, 0, 255, 0);      // Blue (base layer, normal)
            }
            RGB_CURRENT = 0;
            break;
        case 1:
            pimoroni_trackball_set_rgbw(192, 0, 64, 0);         // Red
            RGB_CURRENT = 1;
            break;
        case 2:
            pimoroni_trackball_set_rgbw(0, 192, 128, 0);        // Green
            RGB_CURRENT = 2;
            break;
        case 3:
            pimoroni_trackball_set_rgbw(153, 113, 0, 0);        // Yellow
            RGB_CURRENT = 3;
            break;
        case 4:
            if (BTN_SWAP) {
                pimoroni_trackball_set_rgbw(0, 0, 255, 0);      // Blue (mouse layer, swapped)
            } else {
                pimoroni_trackball_set_rgbw(255, 255, 255, 0);  // White (mouse layer, normal)
            }
            RGB_CURRENT = 4;
            break;
        case 5:
            pimoroni_trackball_set_rgbw(0, 0, 0, 0);            // Off
            RGB_CURRENT = 5;
            break;
        case 6:
            pimoroni_trackball_set_rgbw(138, 43, 226, 0);       // Pink (Hot Pink)
            RGB_CURRENT = 6;
            break;
        default:
            // Optionally handle out-of-range layers
            RGB_CURRENT = 255; // Invalid index or none
            break;

            // Hot Pink (255, 105, 180)
            // Orange	(255, 165, 0)
            // Indigo	(75, 0, 130)
            // Violet	(138, 43, 226)
            // Purple	(128, 0, 128)
    }
}

// Set RGBW for slave
static void set_trackball_rgb_for_slave(uint8_t layer, uint8_t both) {
    // Set number to choose which to update
    // 0 = slave, 1 = master, 2 = both
    if (is_keyboard_master() && (both !=1) ) {
        uint8_t msg[2] = {1, layer};
        transaction_rpc_send(USER_SYNC, sizeof(msg), msg);
    }
    if (both == 2) {
         set_trackball_rgb_for_layer(layer);
    }
}

// RPC handler for slave devices to sync RGB based on the active layer.
// Called when the master sends a layer change RPC event.
static void user_sync_slave_handler(
    uint8_t         in_buflen,
    const void*     in_data,
    uint8_t         out_buflen,
    void*           out_data) {

    if (in_buflen < 2) return;
    const uint8_t *bytes = (const uint8_t *)in_data;
    uint8_t type = bytes[0];
    switch(type) {
        case 1: // Layer sync
            set_trackball_rgb_for_layer(bytes[1]);
            break;
        case 2: // BTN_SWAP sync
            BTN_SWAP = (bool)bytes[1];
            set_trackball_rgb_for_layer(get_highest_layer(layer_state));
            break;

    // Requires #define SPLIT_TRANSACTION_IDS_USER USER_SYNC in config.h
    // Also #include <split_util.h>, #include <transactions.h> in keymap.c
    }
}

// Register the RPC handler for USER_RGB_LAYER_SYNC events.
// This function runs once after keyboard initialization.
void keyboard_post_init_user(void) {
//    debug_enable = false;  // Disable debug console on startup
//    debug_matrix = false;  // Optionally disable matrix debug on startup
    // Optional: adjust CPI per side if needed
    // pointing_device_set_cpi_on_side(true, 8000);   // Left side: low CPI for scrolling
    // pointing_device_set_cpi_on_side(false, 16000); // Right side: high CPI for standard usage

    // Register the RPC handler
    transaction_register_rpc(USER_SYNC, user_sync_slave_handler);

    // Set initial RGB color for base layer
    set_trackball_rgb_for_layer(0);
    // Resets BTN_SWAP for SLAVE on reset, throws off RGB syncing.
    if (is_keyboard_master()) {
        uint8_t msg[2] = {2, BTN_SWAP ? 1 : 0};
        transaction_rpc_send(USER_SYNC, sizeof(msg), msg);
    }
}

// Handle layer state changes.
// Updates the trackball RGB color and sends updated layer info to slave devices.
layer_state_t layer_state_set_user(layer_state_t state) {
    led_t caps = host_keyboard_led_state();
    uint8_t layer = get_highest_layer(state);
    uint8_t sync_layer = layer;
    // When Caps Lock is active on base layer, use layer 5 (Clear) to indicate Caps Lock RGB
    // But on other layers, Caps Lock does not change the color
    if (layer == 0 && caps.caps_lock) {
        sync_layer = 6;
    }

    set_trackball_rgb_for_slave(sync_layer,2);
    return state;
}

// Use matrix_scan_user with caution & sparingly: runs frequently
void matrix_scan_user(void) {
    // Scan for dual-role key pressed/held
    if (is_keyboard_master()) {
        if ((BS_HOL) && (timer_elapsed(BS_TIM) > TAPPING_TERM)) { // Not ideal, so use custom global state instead
            BS_HOL = false;
            switch (KEY_MATRIX) {
                case 0:
                    register_code(KC_BSPC); break;
                case 1: // Mouse Selection & Copy on release
                    register_code(KC_MS_BTN1);
                    unregister_code(KC_MS_BTN1);
                    register_code(KC_LSFT); break;
            }
        }
        if (BS_REL) {
            switch (KEY_MATRIX) {
                case 0:
                    unregister_code(KC_BSPC); break;
                case 1:
                    unregister_code(KC_LSFT);
                    register_code16(C(KC_C));
                    unregister_code16(C(KC_C)); break;
            }
            BS_REL = false;
        }

        if (TLAY_PEN && timer_elapsed(TLAY_REL) > 200) {
            layer_jump_timeout();
        }
    }
}

// LED Indicator for Caps Lock
bool led_update_user(led_t led_state) {
    uint8_t layer;
    if (led_state.caps_lock) {
        layer = 6;
    } else {
        layer = get_highest_layer(layer_state);
    }

    set_trackball_rgb_for_slave(layer,2);
    return false; // Prevent default handler if applicable

    // Requires #define SPLIT_LED_STATE_ENABLE in config.h, OR maybe not, still works without it.
}

// LED Indicator for Caps Word
void caps_word_set_user(bool active) {
    uint8_t layer;
    if (active) {
        layer = 6;
    } else {
        layer = get_highest_layer(layer_state);
    }
    set_trackball_rgb_for_slave(layer,2);
}

// Arrow key emulation
// Arrow key simulation constants
#define     ARROW_MOMENTUM 0.99   // Smoothing factor
#define     ARROW_STEP 6          // Pixel threshold before triggering arrow tap
// Arrow key accumulators
int         accumulated_arrow_x = 0;
int         accumulated_arrow_y = 0;
float       average_arrow_x = 0;
float       average_arrow_y = 0;

static void handle_arrow_emulation(report_mouse_t* mouse_report) {

    average_arrow_x = average_arrow_x * ARROW_MOMENTUM + mouse_report->x;
    average_arrow_y = average_arrow_y * ARROW_MOMENTUM + mouse_report->y;

    // Trigger arrow taps repeatedly until below threshold
    while (fabsf(average_arrow_x) >= ARROW_STEP) {
        tap_code((average_arrow_x > 0) ? KC_RIGHT : KC_LEFT);
        average_arrow_x += (average_arrow_x > 0) ? -ARROW_STEP : ARROW_STEP;
    }
    while (fabsf(average_arrow_y) >= ARROW_STEP) {
        tap_code((average_arrow_y > 0) ? KC_DOWN : KC_UP);
        average_arrow_y += (average_arrow_y > 0) ? -ARROW_STEP : ARROW_STEP;
    }

    // Suppress raw motion (replace with taps only)
    mouse_report->x = 0;
    mouse_report->y = 0;
}

// Scrolling emulation
// Scroll speed divisors (higher = slower scrolling)
#define     SCROLL_DIVISOR_H 8.0
#define     SCROLL_DIVISOR_V 8.0
// Accumulated scroll values (for smooth scroll)
float       scroll_accumulated_h = 0;
float       scroll_accumulated_v = 0;
// Drag Scroll emulation
void handle_scroll_emulation(report_mouse_t* mouse_report) {

    scroll_accumulated_h += (float)mouse_report->x / SCROLL_DIVISOR_H;
    scroll_accumulated_v += -(float)mouse_report->y / SCROLL_DIVISOR_V;

    // To apply natural scroll subtract raw motion before assigning to report [-]
    mouse_report->h = (int16_t)scroll_accumulated_h;
    mouse_report->v = (int16_t)scroll_accumulated_v;

    // Retain fractional scroll remainders
    scroll_accumulated_h -= (int16_t)scroll_accumulated_h;
    scroll_accumulated_v -= (int16_t)scroll_accumulated_v;

    // Suppress raw motion (replace with scrolling only)
    mouse_report->x = 0;
    mouse_report->y = 0;
}

// Adaptive Scaling (Trackballs)
// Non-linear scaling constants
#define     MOMENTUM 0.075f //Smooths out movement, lower = precision
#define     MIN_SCALE 0.0001f
#define     MAX_SCALE 64.0f
// #define GROWTH_FACTOR 8.0f - moved to global variable for runtime adjustment

static void pimoroni_adaptive_scaling(report_mouse_t* mouse_report) {

    static float accumulated_factor = MIN_SCALE;

    // Vector length of raw motion (x and y)
    float mouse_length = sqrtf((float)(mouse_report->x * mouse_report->x) + (float)(mouse_report->y * mouse_report->y));

    // Compute instantaneous scaling factor and update exponential moving average
    float factor = GROWTH_FACTOR * mouse_length + MIN_SCALE;

    // Update exponential moving average of scaling factor
    accumulated_factor = accumulated_factor * (1.0f - MOMENTUM) + factor * MOMENTUM;

    if (accumulated_factor > MAX_SCALE) {
        // Clamp scaling factor to avoid overflow
        mouse_report->x = (int16_t)(mouse_report->x * MAX_SCALE);
        mouse_report->y = (int16_t)(mouse_report->y * MAX_SCALE);
    } else {
        // Scale mouse movement by average factor
        mouse_report->x = (int16_t)(mouse_report->x * accumulated_factor);
        mouse_report->y = (int16_t)(mouse_report->y * accumulated_factor);
    }
}

static btn_state_t handle_mouse_buttons(report_mouse_t report, btn_state_t state) {
    bool pressed = (report.buttons & (1 << 0)) != 0;

    if (pressed && !state.button_was_pressed) {
        uint16_t now = timer_read();

        if (state.mode == MODE_OFF) {
            // First tap from OFF → enter pending state
            state.mode = MODE_PENDING;
            state.last_press_time = now;
        }
        else if (state.mode == MODE_PENDING) {
            if (timer_elapsed(state.last_press_time) < 400) {
                state.mode = MODE_ARROW;
                //handle_arrow_emulation(&report);
                set_trackball_rgb_for_slave(1,2);
            } else {
                // Too slow, treat as new first press
                state.last_press_time = now;
            }
        }
        else {
            // Arrow or Scroll is already active → turn off
            state.mode = MODE_OFF;
            set_trackball_rgb_for_slave(0,2);
        }
    }

    // If we're pending and time ran out, finalize as Scroll
    if (state.mode == MODE_PENDING && timer_elapsed(state.last_press_time) >= 400) {
        // Single tap quick → Scroll
        state.mode = MODE_SCROLL;
        //handle_scroll_emulation(&report);
        set_trackball_rgb_for_slave(2,2);
    }

    state.button_was_pressed = pressed;
    return state;

    // Usage: Passed by value, state is returned to caller.
    // left_button  = handle_mouse_buttons(left_report,  left_button);
    // right_button = handle_mouse_buttons(right_report, right_button);
}

// Mouse Mode Handling Syncing RGB
// Activates mouse mode (white RGB) on movement, reverts to layer color after timeout
static report_mouse_t handle_mouse_mode_rgb(report_mouse_t left_report, report_mouse_t right_report) {
    // Changed from void to report_mouse_t to see if it's more performant

    int16_t combined_x = left_report.x + right_report.x;
    int16_t combined_y = left_report.y + right_report.y;
    uint8_t m_m_layer, m_s_layer, l_layer, r_layer;
    // LEFT BUTTON
    switch (left_button.mode) {
        case MODE_OFF:      l_layer = 4; break;
        case MODE_ARROW:    l_layer = 1; break;
        case MODE_SCROLL:   l_layer = 2; break;
        default: break;
    }
    // RIGHT BUTTON
    switch (right_button.mode) {
        case MODE_OFF:      r_layer = 4; break;
        case MODE_ARROW:    r_layer = 1; break;
        case MODE_SCROLL:   r_layer = 2; break;
        default: break;
    }
    #ifdef MASTER_LEFT
        m_m_layer = r_layer;
        m_s_layer = l_layer;
    #elif defined(MASTER_RIGHT)
        m_m_layer = l_layer;
        m_s_layer = r_layer;
    #endif
    // Move Layer
    if ((combined_x != 0 || combined_y != 0)) {
        // Activate mouse mode (white RGB)
        if (!RGB_MS_MODE || RGB_CURRENT == 0) {
            // This causes some short lived RGB syncing issues but worth it to not spam RPC coms
            // May have fixed short lived syncing issue by adding RGB_CURRENT == 0
            set_trackball_rgb_for_layer(m_m_layer);
            set_trackball_rgb_for_slave(m_s_layer, 0);
            RGB_MS_MODE = true;
        }
        // Reduce unecessary timer reads on every mouse move
        if (timer_elapsed(RGB_MS_MOVE) > TIMER_LIMITER) {
            RGB_MS_MOVE = timer_read();
        }
    // Current Layer
    } else if (RGB_MS_MODE && timer_elapsed(RGB_MS_MOVE) > RGB_MS_TIMEOUT) {
        RGB_MS_MODE = false;
        // Timeout → revert to current active layer
        led_t caps = host_keyboard_led_state();
        uint8_t current_layer = caps.caps_lock ? 6 : get_highest_layer(layer_state);
        set_trackball_rgb_for_slave(current_layer,2);
    }
    return pointing_device_combine_reports(left_report, right_report);
}

static report_mouse_t auto_mouse_layer_handler(report_mouse_t mouse_report) {
    if (mouse_report.x || mouse_report.y) {
        // Mouse moved or button pressed, turn on mouse layer
        if (!ATML_Active) {
            layer_on(3);
            ATML_Active = true;
        }
        // Only reset timer once on first movement
        if (timer_elapsed(ATML_Timer) > TIMER_LIMITER) {
            ATML_Timer = timer_read();
        }
    } else if (timer_elapsed(ATML_Timer) > ATML_TIMEOUT) {
        if (ATML_Active) {
            layer_off(3);
            ATML_Active = false;
        }
    }
    return mouse_report;
}

report_mouse_t pointing_device_task_combined_user(report_mouse_t left_report, report_mouse_t right_report) {

    if (is_keyboard_master()) {
        // Handle button logic
        left_button  = handle_mouse_buttons(left_report,  left_button);
        right_button = handle_mouse_buttons(right_report, right_button);
        // Handle Mousing Mode or Auto Mouse Layer
        if (ATML) {
            auto_mouse_layer_handler(left_report);
            auto_mouse_layer_handler(right_report);
        } else {
            handle_mouse_mode_rgb(left_report, right_report);
        }
        // Apply continuous emulation depending on active mode
        switch (left_button.mode) {
            case MODE_ARROW:  handle_arrow_emulation(&left_report);  break;
            case MODE_SCROLL: handle_scroll_emulation(&left_report); break;
            default: break;
        }
        switch (right_button.mode) {
            case MODE_ARROW:  handle_arrow_emulation(&right_report);  break;
            case MODE_SCROLL: handle_scroll_emulation(&right_report); break;
            default: break;
        }
        // --- Layer overrides ---
        switch (get_highest_layer(layer_state)) {
            case 1:
                handle_arrow_emulation(&left_report);
                handle_arrow_emulation(&right_report);
                break;
            case 2:
                handle_scroll_emulation(&left_report);
                handle_scroll_emulation(&right_report);
                break;
            default: break;
        }
        // --- Scaling ---
        pimoroni_adaptive_scaling(&left_report);
        pimoroni_adaptive_scaling(&right_report);

        // Clear button taps before sending
        left_report.buttons = 0;
        right_report.buttons = 0;
    }

    return pointing_device_combine_reports(left_report, right_report);
}

/*

-- Todo List --

-Add better Tap-Hold handling
-Consolidate idank user profile to my own to handle file settings more explicitly
-Does "static void layer_jump_timeout(void)" need to be static?
-Refine keycode placements for my usage style [Needs to be refined slowly over time]
-Add middle mouse button
-Add incrementer with modifer-shifting to change what to increment
-Add more dual purpose keys to layer 2

---------Received Warnings on Compile after implementing Caps Lock Stuff
[WARNINGS]
 | lto-wrapper.exe: warning: using serial compilation of 2 LTRANS jobs
 | lto-wrapper.exe: note: see the '-flto' option documentation for more information

-- Log of changes: --

8.27.2025
Added BSPC_H to repeat on hold. Using matrix_scan_user(). Not sure if it'll have a negative impact or not. But it works for Tap, Hold & Repeat.
Optimized the code a bit to only call functions when they are needed to reduce overhead.
Changed some if statements to switch statements.
-Implement Built in auto mouse LAYER switching (Doe), Auto Mouse Layer may be causing timing issues, missed inputs, delayed inputs. Added more conditionals to stop what may have been a loop.


8.26.2025
-Changed 2 functions to report_mouse_t, as it seemed more performant than void. Specifically auto_mouse_layer_handler() & handle_mouse_mode_rgb() (I NEED TO UNDERSTAND WHY EVEN THOUGH THESE FUNCTIONS ONLY READ AND DON'T MANIPULATE)
-Create my own auto mouse layer as it was easier
-Added Automouse Layer 4 (Couldn't figure out how to make it work)

-Fixed RGB syncing issues during EE_PROM reset when layer Swap Buttons are swapped by sending RPC coms to slave side during keyboard_post_init_user()
-having issues with mouse move mode, and MB1 is pressed, then letter Y gets held. (possibly fixed it, by removing !mb_mouse_move in handle_mouse_mode_rgb())
-Add 2 new functions to consolidate custom keybinds
-Added set_trackball_rgb_for_slave() to consolidate handling Slave RGB through RPC (Maybe combine it with set_trackball_rgb_for_layer() with additional parameters to handle only slave / master / both)
-Added RGB indicators to handle_mouse_mode_rgb() for handle_mouse_buttons(). Indicates arrow/scroll emulation per side.
-Optimize handle_mouse_mode_rgb() as it was interrupting mouse reports, slave RPC calls was unecessarily syncing. Added condicitonals to sync only when needed.
-Removed speed scale for mouse movement.
-Added mouse button handler.

8.25.2025
-Tried to fix slave mouse report double processing with "is_keyboard_master()" check in pointing_device_task_combined_user()
But it made slave reports jumpy. Figured out it was due to what is mentioned below.

-transaction_rpc_send() calls for RGB layer sync. Discovered that it interferes with mouse_report processing, specifically in handle_mouse_mode_rgb()
which caused the slave mouse reports to be jumpy. Could be overloading the transaction system or timing issue.

-Added caps_word_set_user() & led_update_user() to handle caps lock layer RGB change
-Added in config.h #define SPLIT_LED_STATE_ENABLE to enable led_update_user() on slave side
-Added better syncing of layer state on master and slave based on caps lock state in layer_state_set_user()

-Lots of code cleanup and comments.

-Changed drag scroll to unnatural scroll (Imagine Dragging the Scroll Bars, Not the Page) - seems more intuitive for me.

8.24.2025
Started Logging changes

Consolidated RPC handlers for RGB layer sync and BTN_SWAP sync
Moved Arrow Emulation, handle_arrow_emulation(), and Scroll Emulation, handle_scroll_emulation(), to separate functions for clarity
Removed Clamping of Adaptive Scaling to MAX_SCALE to allow full range (added it back)
Added Debugging Utilities (commented out by default)
Cleaned up Mouse Report Handling
Added tap_hold_handler() helper function to reduce code duplication

*/
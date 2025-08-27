// Example of Tap for 1 key, Hold for another, and it will repeat on hold.
bool        BS_HOL;
bool        BS_REL;
uint16_t    BS_TIM;
uint8_t     KEY_MATRIX;

enum custom_keycodes {
    BSPC_H = SAFE_RANGE,
    SEL_H,
};

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {

        case BSPC_H:
            if (record->event.pressed) {
                BS_TIM = timer_read();
                BS_HOL = true;
                KEY_MATRIX = 0;
            } else {
                if (timer_elapsed(BS_TIM) < TAPPING_TERM) {
                    tap_code(KC_EQL);
                    BS_HOL = false;
                }
                BS_REL = true;
            }
            return false;

        case SEL_H:
            if (record->event.pressed) {
                right_button.mode = MODE_ARROW; // Turns my pimoroni trackball to arrow emulation state for selection
                BS_TIM = timer_read();
                BS_HOL = true;
                KEY_MATRIX = 1;
            } else {
                if (timer_elapsed(BS_TIM) < TAPPING_TERM) {
                    tap_code(KC_H); // Pimoroni Trackball is right next to this key
                    BS_HOL = false;
                }
                BS_REL = true;
                right_button.mode = MODE_OFF;
            }
            return false;
    }
    return true;
}

// Use matrix_scan_user() with caution and sparingly. It scans constantly.
// Make sure there are conditionals to prevent it from running unecessary statements.
void matrix_scan_user(void) {

    if (is_keyboard_master()) { // Added this to make sure it's only run on master side.
        if ((BS_HOL) && (timer_elapsed(BS_TIM) > TAPPING_TERM)) {
            BS_HOL = false;
            switch (KEY_MATRIX) { // Using a switch requires less variables to deal with.
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
                case 1: // Selection & Copy on release
                    unregister_code(KC_LSFT);
                    register_code16(C(KC_C));
                    unregister_code16(C(KC_C)); break;
            }
            BS_REL = false;
        }
    }
}
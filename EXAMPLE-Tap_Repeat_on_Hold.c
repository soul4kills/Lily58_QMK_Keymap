// Example of Tap for 1 key, Hold for another, and it will repeat on hold.
bool        BS_HOL;
bool        BS_REL;
uint16_t    BS_TIM;

enum custom_keycodes {
    BSPC_H = SAFE_RANGE,
};

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case BSPC_H:
            if (record->event.pressed) {
                BS_TIM = timer_read();
                BS_HOL = true;
            } else {
                if (timer_elapsed(BS_TIM) < TAPPING_TERM) {
                    tap_code(KC_EQL);
                    BS_HOL = false;
                }
                BS_REL = true;
            }
            return false;
    }
    return true;
}

// Use matrix_scan_user() with caution and sparingly. It scans constantly.
void matrix_scan_user(void) {
    if ((BS_HOL) && (timer_elapsed(BS_TIM) > TAPPING_TERM)) {
        register_code(KC_BSPC);
        BS_HOL = false;
    }
    if (BS_REL) {
        unregister_code(KC_BSPC);
        BS_REL = false;
    }
}
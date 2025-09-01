// https://docs.qmk.fm/custom_quantum_functions#deferred-execution

#define MOUSE_LAYER 2
#define MOUSE_LAYER_TIMEOUT 2000

static deferred_token layer_off_token = INVALID_DEFERRED_TOKEN;

// Gets called by deferred_exec once it's been scheduled, then resets the token
uint32_t mouse_layer_off_callback(uint32_t trigger_time, void *cb_arg) {
    layer_off(MOUSE_LAYER);
    layer_off_token = INVALID_DEFERRED_TOKEN;
    return 0;
}

// Called on every update except for when inactive
void ps2_mouse_moved_user(report_mouse_t *mouse_report) {
    if (!layer_state_is(MOUSE_LAYER)) { // Mouse Layer not on, turn on, change this to condition to you layer needs
        layer_on(MOUSE_LAYER);
    }
// If deferred token has been set by any of below, meaning layer is still active, this will reset the timeout
    if (layer_off_token != INVALID_DEFERRED_TOKEN) {
        extend_deferred_exec(layer_ff_token, MOUSE_LAYER_TIMEOUT);
// This sets the initial delay if layer_off_token is still INVALID_DEFERRED_TOKEN
    } else {
        layer_off_token = defer_exec(MOUSE_LAYER_TIMEOUT, mouse_layer_off_callback, NULL);
    }
}

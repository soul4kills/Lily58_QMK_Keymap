// Example build commands:
// make lily58/rev1:via:flash -e POINTING_DEVICE=trackball_trackball -e POINTING_DEVICE_POSITION=left -j8
// make lily58/rev1:via:flash -e POINTING_DEVICE=trackball_trackball -e POINTING_DEVICE_POSITION=right -j8

/*
This is the c configuration file for the keymap

Copyright 2012 Jun Wako <wakojun@gmail.com>
Copyright 2015 Jack Humbert

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Example build commands:
#pragma once

//#define USE_MATRIX_I2C

/* Select hand configuration */

// #define MASTER_LEFT
// #define MASTER_RIGHT
// #define EE_HANDS

// #define QUICK_TAP_TERM 0
#define TAPPING_TERM 175
#define BW_TAP_TIME 175
#define TAPPING_TERM_PER_KEY
#define TAPPING_FORCE_HOLD
#define PERMISSIVE_HOLD
#define BOTH_SHIFTS_TURNS_ON_CAPS_WORD

#define PIMORONI_TRACKBALL_SCALE 2
// #define MOUSE_EXTENDED_REPORT
// #define POINTING_DEVICE_DEBUG
// #define POINTING_DEVICE_TASK_THROTTLE_MS 1
// #define POINTING_DEVICE_AUTO_MOUSE_ENABLE

// Define a custom transaction ID for RGB layer sync
#define SPLIT_TRANSACTION_IDS_USER USER_SYNC
// Added for led_update_user to work on slave side to handle caps lock layer RGB change, GIVES COMPILE WARNINGS, Removed this and caps lock syncing still works
// #define SPLIT_LED_STATE_ENABLE

// Allow more than 4 layers
#define DYNAMIC_KEYMAP_LAYER_COUNT 5

//----
#define COMBO_COUNT 15  // N is the number of combos you want
#define COMBO_TERM  20
//----
// Vendor driver is used for RP2040 PIO serial
#define SERIAL_USART_TX_PIN GP1
// Testing for full duplex. idank profile was set without this.
// Not sure if actually made a difference in slave mouse reports being more responsive
#define SERIAL_USART_RX_PIN GP2

#ifdef POINTING_DEVICE_POSITION_LEFT
    #define MASTER_LEFT
#else
    #define MASTER_RIGHT
#endif

// Configuration for dual trackballs.
#ifdef POINTING_DEVICE_CONFIGURATION_PIMORONI_PIMORONI
    // A Pimoroni on the left side can only go in this orientation.
    #define POINTING_DEVICE_ROTATION_270

    // Determine right side rotation based on POINTING_DEVICE_POSITION flag.
    #if POINTING_DEVICE_POSITION_THUMB_OUTER
        #define POINTING_DEVICE_ROTATION_270_RIGHT
    #elif defined(POINTING_DEVICE_POSITION_THUMB) || defined(POINTING_DEVICE_POSITION_THUMB_INNER)
        // No additional rotation defined in this case
    #else
        #define POINTING_DEVICE_ROTATION_90_RIGHT
    #endif
#endif

// Configuration for single trackball.
#ifdef POINTING_DEVICE_CONFIGURATION_PIMORONI
    #ifdef POINTING_DEVICE_POSITION_LEFT
        #define POINTING_DEVICE_ROTATION_270
    #elif POINTING_DEVICE_POSITION_RIGHT
        #define POINTING_DEVICE_ROTATION_90
    #elif POINTING_DEVICE_POSITION_THUMB_OUTER
        #define POINTING_DEVICE_ROTATION_270
    #elif defined(POINTING_DEVICE_POSITION_THUMB) || defined(POINTING_DEVICE_POSITION_THUMB_INNER) || defined(POINTING_DEVICE_POSITION_MIDDLE)
        // No rotation defined here
    #endif
#endif
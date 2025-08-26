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

#pragma once

//#define USE_MATRIX_I2C

/* Select hand configuration */

// #define MASTER_LEFT
// #define MASTER_RIGHT
// #define EE_HANDS

// #define QUICK_TAP_TERM 0
#define TAPPING_TERM 100
#define BW_TAP_TIME 200            // Max tap time in ms

#define TAPPING_FORCE_HOLD
#define PERMISSIVE_HOLD
#define BOTH_SHIFTS_TURNS_ON_CAPS_WORD

//#define POINTING_DEVICE_TASK_THROTTLE_MS 1
#define PIMORONI_TRACKBALL_SCALE 2
//#define MOUSE_EXTENDED_REPORT
// #define POINTING_DEVICE_DEBUG

// Define a custom transaction ID for RGB layer sync
#define SPLIT_TRANSACTION_IDS_USER USER_SYNC
// Added for led_update_user to work on slave side to handle caps lock layer RGB change
#define SPLIT_LED_STATE_ENABLE


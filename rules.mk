# Example build commands:
# make lily58/rev1:via:flash -e POINTING_DEVICE=trackball_trackball -e POINTING_DEVICE_POSITION=left -j8
# make lily58/rev1:via:flash -e POINTING_DEVICE=trackball_trackball -e POINTING_DEVICE_POSITION=right -j8

VALID_POINTING_DEVICE_CONFIGURATIONS := trackball_trackball
ifdef POINTING_DEVICE
    ifeq ($(filter $(POINTING_DEVICE),$(VALID_POINTING_DEVICE_CONFIGURATIONS)),)
        $(call CATASTROPHIC_ERROR,Invalid POINTING_DEVICE,POINTING_DEVICE="$(POINTING_DEVICE)" is not a valid pointing device configuration)
	endif
endif

# TAP_DANCE_ENABLE	= yes
# CONSOLE_ENABLE	= yes
CAPS_WORD_ENABLE    = yes
EXTRAKEY_ENABLE     = yes
MOUSEKEY_ENABLE     = yes     # Mouse keys
VIA_ENABLE          = yes     # Enable VIA
LTO_ENABLE          = yes
NKRO_ENABLE 		= yes

COMBO_ENABLE		= yes
FORCE_NKRO			= yes
SEND_STRING_ENABLE	= yes

# Vendor driver is used for RP2040 PIO serial
SERIAL_DRIVER 		= vendor

# Not really needed for RPC communications, still works without these
SPLIT_KEYBOARD		= yes
SPLIT_TRANSPORT		= yes

# Disable things we don't support that take up unnecessary space.
OLED_ENABLE			= no
OLED_DRIVER			= no
RGBLIGHT_SUPPORTED	= no
RGB_MATRIX_SUPPORTED= no
NO_DEBUG			= yes
NO_PRINT			= yes


# Sets up for Left or Right ( -e POINTING_DEVICE_POSITION=right/left )
ifeq ($(strip $(POINTING_DEVICE_POSITION)), left)
	OPT_DEFS += -DPOINTING_DEVICE_POSITION_LEFT
else
	OPT_DEFS += -DPOINTING_DEVICE_POSITION_RIGHT
endif

# Sets up Pointing Device if set ( -e POINTING_DEVICE=trackball_trackball )
ifeq ($(strip $(POINTING_DEVICE)), trackball_trackball)
	OPT_DEFS += -DSPLIT_POINTING_ENABLE
	OPT_DEFS += -DPOINTING_DEVICE_COMBINED
	OPT_DEFS += -DPOINTING_DEVICE_CONFIGURATION_PIMORONI_PIMORONI

	POINTING_DEVICE_ENABLE = yes
	POINTING_DEVICE_DRIVER = pimoroni_trackball

	ifeq ($(strip $(TRACKBALL_RGB_RAINBOW)), yes)
		SRC += quantum/color.c $(USER_PATH)/trackball_rgb_rainbow.c
	endif
endif

# Sets up Pointing Device if set ( -e POINTING_DEVICE=trackball )
ifeq ($(strip $(POINTING_DEVICE)), trackball)
	OPT_DEFS += -DPOINTING_DEVICE_CONFIGURATION_PIMORONI
	POINTING_DEVICE_ENABLE = yes
	POINTING_DEVICE_DRIVER = pimoroni_trackball

	ifeq ($(strip $(TRACKBALL_RGB_RAINBOW)), yes)
		SRC += quantum/color.c $(USER_PATH)/trackball_rgb_rainbow.c
	endif
endif

# Stops this warning during compile
CFLAGS += -flto=auto
#Linking: .build/lily58_rev1_via.elf                                                                 [WARNINGS]
# |
# | lto-wrapper.exe: warning: using serial compilation of 2 LTRANS jobs
# | lto-wrapper.exe: note: see the '-flto' option documentation for more information Stops
# The console payload, built with ps5-payload-sdk. Called from Makefile.

include $(PS5_PAYLOAD_SDK)/toolchain/prospero.mk

VERSION := $(shell sed -n 's/^\#define ANYPAD_VERSION "\(.*\)"/\1/p' src/version.h)
ELF   := dist/AnyPad-PS5-$(VERSION).elf
BUILD := build/ps5

CFLAGS  := -std=c11 -Wall -Wextra -O2 -Isrc
LDLIBS  += -lScePad -lSceUserService -lSceSystemService -lSceAppInstUtil -ldl

SRCS := src/util.c src/log.c src/crc32.c src/smp_crypto.c src/profiles.c src/generic.c \
        src/host.c src/le.c \
        src/web.c src/web_page.c \
        src/config.c src/evstream.c src/hotkey.c src/lock.c src/netinfo.c src/usb_desc.c \
        src/hci_usb.c src/ps5_power.c src/ps5_ui.c src/ps5_apps.c src/ps5_sysinfo.c src/launcher.c src/icon_data.c src/ps5_vpad.c src/ps5_main.c
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

$(ELF): $(OBJS)
	@mkdir -p dist
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# AnyPad PS5: Bluetooth game controllers on a jailbroken PS5.
#
#   make test                 builds and runs the tests on this machine
#   make ps5                  builds the payload (needs PS5_PAYLOAD_SDK)
#   make send PS5_HOST=ip     sends it to the console's payload loader

PS5_HOST ?= ps5
PS5_PORT ?= 9021

CORE   := src/util.c src/log.c src/crc32.c src/smp_crypto.c src/profiles.c src/generic.c src/host.c src/le.c
WEB    := src/web.c src/web_page.c
HOST_CFLAGS := -std=c11 -Wall -Wextra -Werror -O1 -g -Isrc

BUILD := build

.PHONY: all test fuzz demo ps5 send clean

all: test

$(BUILD):
	@mkdir -p $(BUILD)

# The web page, embedded as a byte array (kept in git, so the console build
# does not need xxd).
src/web_page.c: web/index.html
	{ echo '/* Generated from web/index.html by make. */'; \
	  echo 'const unsigned char web_page_html[] = {'; xxd -i < $<; echo '};'; \
	  echo 'const unsigned int web_page_html_len = sizeof web_page_html;'; } > $@

$(BUILD)/test_web: $(CORE) $(WEB) tests/sim.c tests/test_web.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

# The page against a simulated pad, to look at in a browser: make demo
demo: $(CORE) $(WEB) tests/sim.c tests/web_demo.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $(BUILD)/web_demo $^
	./$(BUILD)/web_demo

$(BUILD)/test_host: $(CORE) tests/sim.c tests/test_host.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

$(BUILD)/test_compat: $(CORE) tests/sim.c tests/test_compat.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

$(BUILD)/test_profiles: $(CORE) tests/test_profiles.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

$(BUILD)/test_crypto: src/smp_crypto.c tests/test_crypto.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

$(BUILD)/test_le: $(CORE) tests/sim_le.c tests/test_le.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

# The console source for virtual pads, against stand-ins for the console's libraries.
$(BUILD)/test_vpad: src/ps5_vpad.c src/log.c src/util.c tests/test_vpad.c | $(BUILD)
	cc -std=c11 -Wall -Wextra -Werror -O1 -g -Isrc -D_DEFAULT_SOURCE -D_DARWIN_C_SOURCE -o $@ $^

# Configuration, the console hotkey and the local address.
$(BUILD)/test_config: src/config.c src/hotkey.c src/netinfo.c src/log.c src/util.c tests/test_config.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

# The instance lock.
$(BUILD)/test_lock: src/lock.c src/log.c src/util.c tests/test_lock.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

src/icon_data.c: assets/icon0.png
	{ echo '/* Generated from assets/icon0.png by make. */'; \
	  echo 'const unsigned char icon_png_data[] = {'; xxd -i < $<; echo '};'; \
	  echo 'const unsigned int icon_png_data_len = sizeof icon_png_data;'; } > $@

$(BUILD)/test_launcher: src/launcher.c src/icon_data.c tests/test_launcher.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

# Endpoints from the chip's descriptor (checked against a real dump) and event reassembly.
$(BUILD)/test_usb: src/usb_desc.c src/evstream.c tests/test_usb.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

test: $(BUILD)/test_hearbridge $(BUILD)/test_usb $(BUILD)/test_lock $(BUILD)/test_config $(BUILD)/test_vpad $(BUILD)/test_crypto $(BUILD)/test_profiles $(BUILD)/test_host $(BUILD)/test_le $(BUILD)/test_compat $(BUILD)/test_web $(BUILD)/test_launcher
	./$(BUILD)/test_usb
	./$(BUILD)/test_lock
	./$(BUILD)/test_config
	./$(BUILD)/test_vpad
	./$(BUILD)/test_crypto
	./$(BUILD)/test_profiles
	./$(BUILD)/test_host
	./$(BUILD)/test_le
	./$(BUILD)/test_compat
	./$(BUILD)/test_web
	./$(BUILD)/test_launcher
	./$(BUILD)/test_hearbridge

# Malformed input through every parser, under the sanitizers.
SAN := -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
fuzz: | $(BUILD)
	cc $(HOST_CFLAGS) $(SAN) -o $(BUILD)/test_fuzz $(CORE) tests/test_fuzz.c
	./$(BUILD)/test_fuzz

# ---- the console payload ----------------------------------------------------

ps5:
ifndef PS5_PAYLOAD_SDK
	$(error PS5_PAYLOAD_SDK is undefined)
endif
	$(MAKE) -f ps5.mk

send: ps5
	$(PS5_PAYLOAD_SDK)/bin/prospero-deploy -h $(PS5_HOST) -p $(PS5_PORT) dist/AnyPad-PS5-*.elf

clean:
	rm -rf $(BUILD)

# Targeted regressions from the direct HearBridge review.
$(BUILD)/test_hearbridge: $(CORE) tests/sim.c tests/test_hearbridge.c tests/test_host.c | $(BUILD)
	cc $(HOST_CFLAGS) -DEXPECT_CFG=1 -DEXPECT_BACKOFF=1 -o $@ $(CORE) tests/sim.c tests/test_hearbridge.c

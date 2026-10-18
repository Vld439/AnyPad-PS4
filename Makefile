# AnyPad PS4: other game controllers on a jailbroken PS4.
#
#   make test                 builds and runs the tests on this machine
#   make ps4                  builds the payload (needs PS4_PAYLOAD_SDK)
#   make send PS4_HOST=ip     sends it to the console's payload loader

PS4_HOST ?= ps4
PS4_PORT ?= 9020

CORE   := src/util.c src/log.c src/crc32.c src/profiles.c src/generic.c
HOST_CFLAGS := -std=c11 -Wall -Wextra -Werror -O1 -g -Isrc -D_DEFAULT_SOURCE

BUILD := build

.PHONY: all test fuzz ps4 send clean

all: test

$(BUILD):
	@mkdir -p $(BUILD)

# The web page, embedded as a byte array (kept in git, so the console build
# does not need xxd).
src/web_page.c: web/index.html
	{ echo '/* Generated from web/index.html by make. */'; \
	  echo 'const unsigned char web_page_html[] = {'; xxd -i < $<; echo '};'; \
	  echo 'const unsigned int web_page_html_len = sizeof web_page_html;'; } > $@

$(BUILD)/test_profiles: $(CORE) tests/test_profiles.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

# The menu's server against stand-in pads.
$(BUILD)/test_web: src/web.c src/web_page.c src/log.c src/util.c tests/test_web.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

# The console source for virtual pads, against stand-ins for the console's libraries.
$(BUILD)/test_vpad: src/ps4_vpad.c src/log.c src/util.c tests/test_vpad.c | $(BUILD)
	cc -std=c11 -Wall -Wextra -Werror -O1 -g -Isrc -D_DEFAULT_SOURCE -D_DARWIN_C_SOURCE -o $@ $^

# The instance lock.
$(BUILD)/test_lock: src/lock.c src/log.c src/util.c tests/test_lock.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

src/icon_data.c: assets/icon0.png
	{ echo '/* Generated from assets/icon0.png by make. */'; \
	  echo 'const unsigned char icon_png_data[] = {'; xxd -i < $<; echo '};'; \
	  echo 'const unsigned int icon_png_data_len = sizeof icon_png_data;'; } > $@

$(BUILD)/test_launcher: src/launcher.c src/icon_data.c tests/test_launcher.c | $(BUILD)
	cc $(HOST_CFLAGS) -o $@ $^

test: $(BUILD)/test_lock $(BUILD)/test_vpad $(BUILD)/test_profiles $(BUILD)/test_web $(BUILD)/test_launcher
	./$(BUILD)/test_lock
	./$(BUILD)/test_vpad
	./$(BUILD)/test_profiles
	./$(BUILD)/test_web
	./$(BUILD)/test_launcher

# Malformed input through every parser, under the sanitizers.
SAN := -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
fuzz: | $(BUILD)
	cc $(HOST_CFLAGS) $(SAN) -o $(BUILD)/test_fuzz $(CORE) tests/test_fuzz.c
	./$(BUILD)/test_fuzz

ps4:
ifndef PS4_PAYLOAD_SDK
	$(error PS4_PAYLOAD_SDK is undefined)
endif
	@mkdir -p build
	@printf '#define ANYPAD_BUILD_ID "%s"\n' "$$(git describe --always --dirty --abbrev=7 2>/dev/null || echo unknown)" > build/build_id.h
	$(MAKE) -f ps4.mk

send: ps4
	@echo "Enviando payload a PS4 en $(PS4_HOST):$(PS4_PORT)..."
	nc -w 3 $(PS4_HOST) $(PS4_PORT) < dist/AnyPad-PS4-*.elf

clean:
	rm -rf $(BUILD)

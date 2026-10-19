# The PS4 console payload, built with ps4-payload-sdk. Called from Makefile.

include $(PS4_PAYLOAD_SDK)/toolchain/orbis.mk

VERSION := $(shell sed -n 's/^\#define ANYPAD_VERSION "\(.*\)"/\1/p' src/version.h)
ELF   := dist/AnyPad-PS4-$(VERSION).elf
BUILD := build/ps4

CFLAGS  := -std=c11 -Wall -Wextra -Werror -O2 -Isrc -Ibuild
# libSceMbus: nothing of it is called, but libScePad needs it loaded first,
# as it was when this was tested on a console.
LDLIBS  += -lScePad -lSceMbus -lSceUsbd -lSceUserService -lSceSystemService -lpthread

SRCS := src/util.c src/log.c src/crc32.c src/profiles.c src/generic.c src/web.c \
        src/web_page.c src/lock.c src/netinfo.c src/ps4_power.c src/ps4_ui.c \
        src/ps4_apps.c src/ps4_sysinfo.c src/launcher.c src/icon_data.c src/ps4_vpad.c \
        src/usb_pad.c src/netpad.c src/ps4_main.c
OBJS := $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

# Link stubs for the system libraries the SDK has none for (names only; the
# payload loader resolves them), built the way the SDK builds its own.
STUBS := $(patsubst tools/sce_stubs/%.c,$(BUILD)/stubs/%.so,$(wildcard tools/sce_stubs/*.c))
STUB_LD ?= $(PS4_PAYLOAD_SDK)/bin/ld.lld
STUB_CFLAGS := -ffreestanding -fno-builtin -nostdlib -fPIC -target x86_64-scie-ps4 -fno-plt \
               -fno-stack-protector -Wall -Werror -fvisibility-nodllstorageclass=default

# The same file as .bin too: Payload Guest only lists .bin payloads (and
# hands them to GoldHEN's BinLoader, which takes the ELF as it is).
$(ELF): $(OBJS) $(STUBS)
	@mkdir -p dist
	$(CC) $(LDFLAGS) -L$(BUILD)/stubs -o $@ $(OBJS) $(LDLIBS)
	cp $@ $(@:.elf=.bin)

# The commit it is built from, in its log's first line (written by the Makefile).
$(BUILD)/ps4_main.o: $(wildcard build/build_id.h)

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/stubs/%.so: tools/sce_stubs/%.c
	@mkdir -p $(dir $@)
	$(PS4_PAYLOAD_SDK)/bin/clang -c $(STUB_CFLAGS) -o $(@:.so=.o) $<
	$(STUB_LD) -m elf_x86_64 -shared -soname $*.sprx -o $@ $(@:.so=.o)

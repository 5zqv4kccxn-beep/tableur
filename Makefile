.RECIPEPREFIX = >
Q ?= @
CC = arm-none-eabi-gcc
BUILD_DIR = output
NWLINK = npx --yes -- nwlink@0.0.19

src = src/main.c src/content.c
objs = $(addprefix $(BUILD_DIR)/,$(src:src/%.c=%.o))

CFLAGS = -std=c99 $(shell $(NWLINK) eadk-cflags-device) -Os -Wall -fdata-sections -ffunction-sections
LDFLAGS = -Wl,--relocatable -nostartfiles --specs=nano.specs -Wl,-e,main -Wl,-u,eadk_app_name -Wl,-u,eadk_api_level -Wl,--gc-sections

.PHONY: build clean

build: $(BUILD_DIR)/app.nwa

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
> $(Q)$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/app.elf: $(objs)
> $(Q)$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/app.nwa: $(BUILD_DIR)/app.elf
> $(Q)$(NWLINK) elf-nwa $< $@

$(BUILD_DIR):
> $(Q)mkdir -p $@

clean:
> $(Q)rm -rf $(BUILD_DIR)

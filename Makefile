.RECIPEPREFIX = >
Q ?= @
CC = arm-none-eabi-gcc
BUILD_DIR = output
NWLINK = npx --yes -- nwlink@0.0.16

src = src/main.c src/content.c
objs = $(addprefix $(BUILD_DIR)/,$(src:src/%.c=%.o))

CFLAGS = -std=c99 $(shell $(NWLINK) eadk-cflags) -Os -Wall -fdata-sections -ffunction-sections
LDFLAGS = -Wl,--relocatable -nostartfiles --specs=nano.specs -Wl,-e,main -Wl,-u,eadk_app_name -Wl,-u,eadk_api_level -Wl,--gc-sections

.PHONY: build clean

build: $(BUILD_DIR)/app.nwa

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
> $(Q)$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/icon.o: src/icon.png | $(BUILD_DIR)
> $(Q)$(NWLINK) png-icon-o $< $@

$(BUILD_DIR)/app.nwa: $(objs) $(BUILD_DIR)/icon.o
> $(Q)$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

$(BUILD_DIR):
> $(Q)mkdir -p $@

clean:
> $(Q)rm -rf $(BUILD_DIR)

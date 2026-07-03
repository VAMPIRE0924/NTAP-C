ifeq ($(OS),Windows_NT)
EXEEXT := .exe
BUILD_FLAVOR := msys2
MKDIR_P := /usr/bin/mkdir -p
RM_RF := /usr/bin/rm -rf
NET_LIBS := -lws2_32 -ladvapi32
THREAD_LIBS :=
GUI_LIBS := -mwindows -luser32 -lgdi32 -lshell32
ifeq ($(origin CC),default)
CC := /ucrt64/bin/gcc
endif
ifeq ($(origin PKG_CONFIG),undefined)
PKG_CONFIG := /ucrt64/bin/pkg-config
endif
else
EXEEXT :=
BUILD_FLAVOR := linux
MKDIR_P := mkdir -p
RM_RF := rm -rf
NET_LIBS :=
THREAD_LIBS := -pthread
GUI_LIBS :=
ifeq ($(origin CC),default)
CC := gcc
endif
PKG_CONFIG ?= pkg-config
endif

BUILD_DIR ?= build/$(BUILD_FLAVOR)
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin

CPPFLAGS += -Isrc
CFLAGS += -std=c11 -O2 -g -Wall -Wextra -Wpedantic -Werror

OPENSSL_CFLAGS := $(shell $(PKG_CONFIG) --cflags openssl 2>/dev/null)
OPENSSL_LIBS := $(shell $(PKG_CONFIG) --libs openssl 2>/dev/null)
ifeq ($(strip $(OPENSSL_LIBS)),)
OPENSSL_LIBS := -lcrypto
endif

COMPONENT_CFLAGS := $(OPENSSL_CFLAGS)
COMPONENT_LIBS := $(OPENSSL_LIBS)

COMMON_SRCS := \
	src/common/buffer.c \
	src/common/config.c \
	src/common/direct_token.c \
	src/common/hash.c \
	src/common/log.c \
	src/common/net.c \
	src/common/proto.c \
	src/common/tap_linux.c \
	src/common/time.c \
	src/common/wire.c

CLI_SRCS := \
	src/c/config.c \
	src/c/env_check.c \
	src/c/main.c \
	src/c/tap_client.c

GUI_SRCS := \
	src/c/config.c \
	src/c/env_check.c \
	src/c/gui_win.c \
	src/c/tap_client.c

COMMON_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(COMMON_SRCS))
CLI_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CLI_SRCS))
GUI_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(GUI_SRCS))

ifeq ($(OS),Windows_NT)
TARGETS := $(BIN_DIR)/ntap-c$(EXEEXT) $(BIN_DIR)/ntap-c-cli$(EXEEXT)
CONFIG_TEST_BIN := $(BIN_DIR)/ntap-c-cli$(EXEEXT)
else
TARGETS := $(BIN_DIR)/ntap-c$(EXEEXT)
CONFIG_TEST_BIN := $(BIN_DIR)/ntap-c$(EXEEXT)
endif

.PHONY: all config-test clean

all: $(TARGETS)

config-test: all
	./$(CONFIG_TEST_BIN) -c conf/ntap-c.conf.example -t

ifeq ($(OS),Windows_NT)
$(BIN_DIR)/ntap-c-cli$(EXEEXT): $(COMMON_OBJS) $(CLI_OBJS)
	@$(MKDIR_P) $(@D)
	$(CC) $^ $(COMPONENT_LIBS) $(NET_LIBS) $(THREAD_LIBS) -o $@

$(BIN_DIR)/ntap-c$(EXEEXT): $(COMMON_OBJS) $(GUI_OBJS)
	@$(MKDIR_P) $(@D)
	$(CC) $^ $(COMPONENT_LIBS) $(NET_LIBS) $(THREAD_LIBS) $(GUI_LIBS) -o $@
else
$(BIN_DIR)/ntap-c$(EXEEXT): $(COMMON_OBJS) $(CLI_OBJS)
	@$(MKDIR_P) $(@D)
	$(CC) $^ $(COMPONENT_LIBS) $(NET_LIBS) $(THREAD_LIBS) -o $@
endif

$(OBJ_DIR)/%.o: %.c
	@$(MKDIR_P) $(@D)
	$(CC) $(CPPFLAGS) $(COMPONENT_CFLAGS) $(CFLAGS) -c $< -o $@

clean:
	$(RM_RF) $(BUILD_DIR)
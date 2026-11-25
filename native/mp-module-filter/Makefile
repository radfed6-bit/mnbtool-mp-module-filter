# Standalone Makefile for Android NDK (GitHub Actions) or aarch64-linux-gnu-gcc
#
# Android NDK usage:
#   make CC=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang
#
# Local Linux aarch64 verification:
#   make CC=aarch64-linux-gnu-gcc

ifeq ($(origin CC),default)
CC := aarch64-linux-android24-clang
endif

ifneq (,$(findstring clang,$(CC)))
SAFE_STACK_FLAG ?= -fno-sanitize=safe-stack
else
SAFE_STACK_FLAG ?=
endif

ifneq (,$(findstring android,$(CC)))
LDLIBS ?= -llog -ldl
else
LDLIBS ?= -ldl
endif

COMMON_CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror
ETREL_CFLAGS  := $(COMMON_CFLAGS) -fno-PIC -fno-common -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-optimize-sibling-calls $(SAFE_STACK_FLAG)
SO_CFLAGS     := $(COMMON_CFLAGS) -fPIC

.PHONY: all clean check-whitelist

all: build/mod.arm64.o build/libmp_module_filter.so

build:
	mkdir -p build

build/mod.arm64.o: mod_etrel.c mf_abi.h | build
	$(CC) $(ETREL_CFLAGS) -c mod_etrel.c -o $@

build/libmp_module_filter.so: mod_so.c mf_abi.h | build
	$(CC) $(SO_CFLAGS) -shared mod_so.c -o $@ $(LDLIBS)

# Verify build/mod.arm64.o imports ONLY symbols permitted by libmodlauncher.so
check-whitelist: build/mod.arm64.o
	python3 -c 'import subprocess, sys; \
allowed = {"mf_game_pointer_v1","__safestack_pointer_address","memcmp","memcpy","memmove","memset","strlen","strnlen","strcmp","strncmp","strstr","malloc","calloc","realloc","free","clock_gettime"}; \
out = subprocess.check_output(["nm", "-u", "build/mod.arm64.o"], text=True); \
undef = {l.split()[-1] for l in out.splitlines() if l.strip()}; \
bad = sorted(undef - allowed); \
print("Undefined symbols:", sorted(undef)); \
sys.exit(f"DISALLOWED SYMBOLS: {bad}") if bad else print("Whitelist check PASSED")'

clean:
	rm -rf build

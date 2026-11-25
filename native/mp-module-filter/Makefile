# Build wrapper around the official M&BTool ModFramework SDK (remixer-dec/MNBTool).
#
# Usage:
#   export ANDROID_NDK_ROOT=/path/to/android-ndk
#   export MNBTOOL_SDK=/path/to/MNBTool/modframework
#   make               # builds both ARMv7 and ARM64 objects in build/
#   make package       # creates build/mp-module-filter.zip (manifest.json at root)

MNBTOOL_SDK ?= ../../../MNBTool/modframework
ANDROID_ABI ?= both

.PHONY: all package clean

all:
	@test -x "$(MNBTOOL_SDK)/build-mod.sh" || { \
		echo "ERROR: Official M&BTool ModFramework SDK not found at $(MNBTOOL_SDK)." >&2; \
		echo "Clone https://github.com/remixer-dec/MNBTool and set MNBTOOL_SDK=/path/to/MNBTool/modframework" >&2; \
		exit 1; \
	}
	ANDROID_ABI=$(ANDROID_ABI) "$(MNBTOOL_SDK)/build-mod.sh" . ./build

package: all
	rm -f build/mp-module-filter.zip
	cd build && zip -r -X mp-module-filter.zip manifest.json mod.arm64.o mod.armv7.o source/

clean:
	rm -rf build

# Voxel Engine — Build System
#
# Targets:
#   make android    — build for Android (aarch64 via NDK)
#   make clean      — remove all build artifacts
#
# For Windows, use: build_windows.bat

# ── Android NDK Toolchain ──────────────────────────────────────────────────
NDK_HOME ?= $(LOCALAPPDATA)/Android/Sdk/ndk/27.0.12077973
TOOLCHAIN = $(NDK_HOME)/toolchains/llvm/prebuilt/windows-x86_64

# Target API level (minimum supported on device)
API_LEVEL = 24

# Compiler and linker
CXX      = $(TOOLCHAIN)/bin/aarch64-linux-android$(API_LEVEL)-clang++.exe
CC       = $(TOOLCHAIN)/bin/aarch64-linux-android$(API_LEVEL)-clang.exe
SYSROOT  = $(TOOLCHAIN)/sysroot

# ── Flags ──────────────────────────────────────────────────────────────────
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -ffast-math -DNDEBUG
LDFLAGS  = -lEGL -lGLESv2 -landroid -lnativewindow -llog -lm

# ── Sources (Android only uses main.cpp) ───────────────────────────────────
APP_GLUE_SRC = $(NDK_HOME)/sources/android/native_app_glue/android_native_app_glue.c

ANDROID_SOURCES = src/main.cpp src/core/renderer.cpp src/rendering/sky.cpp \
                  src/rendering/texture_atlas.cpp \
                  src/world/chunk.cpp src/world/chunk_manager.cpp
INCLUDES = -Isrc/core -Isrc/world -Isrc/rendering -Isrc/utils -Isrc/physics
APP_GLUE_OBJ = android_native_app_glue.o

# ── Output ─────────────────────────────────────────────────────────────────
TARGET = voxel_engine

# ── Rules ──────────────────────────────────────────────────────────────────
.PHONY: android clean

android: $(TARGET)

$(TARGET): $(ANDROID_SOURCES) $(APP_GLUE_OBJ)
	$(CXX) $(CXXFLAGS) -I$(SYSROOT)/usr/include $(INCLUDES) \
		$(ANDROID_SOURCES) $(APP_GLUE_OBJ) \
		-o $(TARGET) $(LDFLAGS)

$(APP_GLUE_OBJ): $(APP_GLUE_SRC)
	$(CC) -c -O2 -I$(SYSROOT)/usr/include \
		$(APP_GLUE_SRC) -o $(APP_GLUE_OBJ)

clean:
	rm -f $(TARGET) $(APP_GLUE_OBJ)
	del /q voxel_engine.exe *.obj *.pdb *.ilk 2>nul

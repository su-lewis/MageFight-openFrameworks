# OF_SHARED_MAKEFILE
# This file is included by the main Makefile.

APPNAME = MageFight

# ---------------------------------------------------------------------------
# CORE SETTINGS
# ---------------------------------------------------------------------------
OF_GL_PROGRAMMABLE_RENDERER = 1
OPTIMIZATION_CFLAGS = -O3

# ---------------------------------------------------------------------------
# STEAMWORKS SDK (Common Includes for both OSes)
# ---------------------------------------------------------------------------
USER_INCLUDE_PATHS = libs/steam/include
PROJECT_CXXFLAGS += -Ilibs/steam/include
USER_CXXFLAGS += -Ilibs/steam/include
USER_CFLAGS += -Ilibs/steam/include

# ---------------------------------------------------------------------------
# OS-SPECIFIC LINKING
# ---------------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
    # ==========================================
    # WINDOWS (GitHub Actions / MSYS2)
    # ==========================================
    # MinGW GCC Superpower: We link DIRECTLY to the runtime DLLs!
    # This completely bypasses the incompatible Microsoft .lib formats.
    PROJECT_EXTERNAL_LIBS += bin/steam_api64.dll
    PROJECT_EXTERNAL_LIBS += bin/fmod.dll

else
    # ==========================================
    # LINUX (Your Bazzite Machine)
    # ==========================================
    PROJECT_LDFLAGS += -Llibs/steam/lib -lsteam_api
    PROJECT_LDFLAGS += -Wl,-rpath=./libs/steam/lib
    PROJECT_LDFLAGS += -Wl,-rpath-link,/usr/lib

    # Corrected FMOD path for Linux
    PROJECT_LDFLAGS += ../../../libs/fmod/linux64/libfmod.so

endif

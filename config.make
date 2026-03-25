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
    # MinGW GCC Superpower: Link DIRECTLY to the runtime DLLs inside your bin/ folder!
    # This completely bypasses the incompatible Microsoft .lib formats.
    PROJECT_LDFLAGS += -Lbin -lsteam_api64 -lfmod

else
    # ==========================================
    # LINUX (Your Bazzite Machine)
    # ==========================================
    PROJECT_LDFLAGS += -Llibs/steam/lib -lsteam_api
    PROJECT_LDFLAGS += -Wl,-rpath=./libs/steam/lib
    PROJECT_LDFLAGS += -Wl,-rpath-link,/usr/lib
    PROJECT_LDFLAGS += ../../../libs/fmod/linux64/libfmod.so

endif

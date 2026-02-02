# OF_SHARED_MAKEFILE
# This file is included by the main Makefile.
# It defines project specific variables.

APPNAME = MageFight

# ---------------------------------------------------------------------------
# CORE SETTINGS
# ---------------------------------------------------------------------------
OF_GL_PROGRAMMABLE_RENDERER = 1
OPTIMIZATION_CFLAGS = -O3

# ---------------------------------------------------------------------------
# STEAMWORKS SDK
# ---------------------------------------------------------------------------

# 1. Include Path (Relative to this file)
#    This tells the compiler where to look for "steam_api.h"
USER_INCLUDE_PATHS = libs/steam/include

# 2. Linker Flags (Relative to this file)
#    -L tells it where the folder is
#    -l tells it to look for libsteam_api.so
PROJECT_LDFLAGS = -Llibs/steam/lib -lsteam_api

# 3. Runtime Path (RPATH)
#    This ensures the game finds the library when you actually run it.
PROJECT_LDFLAGS += -Wl,-rpath=./libs/steam/lib
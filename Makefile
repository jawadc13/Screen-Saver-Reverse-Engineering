# Cross-compiles the screensavers with MinGW-w64 (also works with MSYS2's
# native MinGW on Windows: `mingw32-make`).
#
#   make            -> build/*.scr (64-bit, Windows 10/11)
#   make ARCH=x86   -> 32-bit build
ARCH    ?= x64
ifeq ($(ARCH),x64)
PREFIX  ?= x86_64-w64-mingw32-
else
PREFIX  ?= i686-w64-mingw32-
endif
CXX      = $(PREFIX)g++
WINDRES  = $(PREFIX)windres
CXXFLAGS = -O2 -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -municode -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0A00 -std=c++14
LDFLAGS  = -municode -mwindows -static -static-libgcc -static-libstdc++ -s
LIBS     = -ld3d11 -ldxgi -luuid -lgdiplus -lcomctl32 -lcomdlg32 -lgdi32 -lwinmm -luser32 -ladvapi32

SAVERS   = pipes starfield polyhedra mystify matrix tunnel ribbons bubbles plasma fireworks flowerbox aurora orbs meadow aquarium glasspanes bokeh
OUT      = build

all: $(SAVERS:%=$(OUT)/%.scr)

$(OUT):
	mkdir -p $(OUT)

$(OUT)/saver.o: common/saver.cpp common/saver.h common/render.h common/mesh.h common/common_res.h | $(OUT)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT)/simplecfg.o: common/simplecfg.cpp common/simplecfg.h common/saver.h | $(OUT)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT)/render.o: common/render.cpp common/render.h common/mesh.h | $(OUT)
	$(CXX) $(CXXFLAGS) -c $< -o $@

define SAVER_RULES
$(OUT)/$(1).o: savers/$(1)/$(1).cpp common/saver.h common/render.h common/mesh.h common/scenekit.h common/aerokit.h savers/$(1)/resource.h | $(OUT)
	$$(CXX) $$(CXXFLAGS) -c $$< -o $$@

$(OUT)/$(1)_res.o: savers/$(1)/$(1).rc savers/$(1)/resource.h savers/$(1)/$(1).ico common/common.rc common/saver.manifest | $(OUT)
	$$(WINDRES) -I common -I savers/$(1) $$< -o $$@

$(OUT)/$(1).scr: $(OUT)/$(1).o $(OUT)/$(1)_res.o $(OUT)/saver.o $(OUT)/render.o $(OUT)/simplecfg.o
	$$(CXX) $$(LDFLAGS) $$^ -o $$@ $$(LIBS)
endef
$(foreach s,$(SAVERS),$(eval $(call SAVER_RULES,$(s))))

clean:
	rm -rf $(OUT)

.PHONY: all clean
.SECONDARY:

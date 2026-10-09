# Cross-compiles the screensavers with MinGW-w64 (also works with MSYS2's
# native MinGW on Windows: `mingw32-make`).
#
#   make            -> build/*.scr (32-bit; runs on XP .. Windows 11)
#   make ARCH=x64   -> 64-bit build
ARCH    ?= x86
ifeq ($(ARCH),x64)
PREFIX  ?= x86_64-w64-mingw32-
else
PREFIX  ?= i686-w64-mingw32-
endif
CXX      = $(PREFIX)g++
WINDRES  = $(PREFIX)windres
CXXFLAGS = -O2 -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -municode -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0501 -std=c++11
LDFLAGS  = -municode -mwindows -static -static-libgcc -static-libstdc++ -s
LIBS     = -lopengl32 -lglu32 -lgdiplus -lcomctl32 -lcomdlg32 -lgdi32 -luser32 -ladvapi32

SAVERS   = pipes starfield polyhedra
OUT      = build

all: $(SAVERS:%=$(OUT)/%.scr)

$(OUT):
	mkdir -p $(OUT)

$(OUT)/saver.o: common/saver.cpp common/saver.h common/common_res.h | $(OUT)
	$(CXX) $(CXXFLAGS) -c $< -o $@

define SAVER_RULES
$(OUT)/$(1).o: savers/$(1)/$(1).cpp common/saver.h common/mesh.h savers/$(1)/resource.h | $(OUT)
	$$(CXX) $$(CXXFLAGS) -c $$< -o $$@

$(OUT)/$(1)_res.o: savers/$(1)/$(1).rc savers/$(1)/resource.h savers/$(1)/$(1).ico common/common.rc common/saver.manifest | $(OUT)
	$$(WINDRES) -I common -I savers/$(1) $$< -o $$@

$(OUT)/$(1).scr: $(OUT)/$(1).o $(OUT)/$(1)_res.o $(OUT)/saver.o
	$$(CXX) $$(LDFLAGS) $$^ -o $$@ $$(LIBS)
endef
$(foreach s,$(SAVERS),$(eval $(call SAVER_RULES,$(s))))

clean:
	rm -rf $(OUT)

.PHONY: all clean
.SECONDARY:

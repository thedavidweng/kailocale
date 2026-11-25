.SUFFIXES:

TARGET		:=	KaiLocale
APP_TITLE	:=	KaiLocale
APP_AUTHOR	:=	thedavidweng
APP_VERSION	:=	0.1.0
ICON		:=	assets/icon.jpg

BUILD		:=	build
HOST_BUILD	:=	build-host
DIST_DIR	:=	dist/sd/switch/$(TARGET)
DIST_NRO	:=	$(DIST_DIR)/$(TARGET).nro
DOCKER_IMAGE	:=	kailocale-toolchain:20261123

HOST_CXX	?=	c++
HOST_CXXFLAGS	:=	-std=c++17 -Wall -Wextra -Werror -pedantic -O2

SOURCES		:=	src/core src/platform src
INCLUDES	:=	include src

#---------------------------------------------------------------------------------
# Top-level invocation vs. devkitPro $(BUILD) sub-make invocation
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))

.PHONY: all switch2 host-test test docker-image docker-build audit release clean clean-host

ifeq ($(strip $(DEVKITPRO)),)
all: host-test
else
all: switch2
endif

host-test:
	@mkdir -p $(HOST_BUILD)
	$(HOST_CXX) $(HOST_CXXFLAGS) -Iinclude src/core/app_shell.cpp src/core/language_catalog.cpp src/core/localization.cpp src/core/picker_layout.cpp src/core/language_picker.cpp tests/test_app_shell.cpp -o $(HOST_BUILD)/test_app_shell
	./$(HOST_BUILD)/test_app_shell
	$(HOST_CXX) $(HOST_CXXFLAGS) -Iinclude src/core/language_catalog.cpp tests/test_language_catalog.cpp -o $(HOST_BUILD)/test_language_catalog
	./$(HOST_BUILD)/test_language_catalog
	$(HOST_CXX) $(HOST_CXXFLAGS) -Iinclude src/core/language_catalog.cpp src/core/localization.cpp tests/test_localization_theme.cpp -o $(HOST_BUILD)/test_localization_theme
	./$(HOST_BUILD)/test_localization_theme
	$(HOST_CXX) $(HOST_CXXFLAGS) -Iinclude src/core/language_catalog.cpp src/core/localization.cpp src/core/picker_layout.cpp src/core/language_picker.cpp tests/test_language_picker.cpp -o $(HOST_BUILD)/test_language_picker
	./$(HOST_BUILD)/test_language_picker
	$(HOST_CXX) $(HOST_CXXFLAGS) -Iinclude src/core/text_runs.cpp tests/test_text_runs.cpp -o $(HOST_BUILD)/test_text_runs
	./$(HOST_BUILD)/test_text_runs

test: host-test

docker-image:
	docker build -t $(DOCKER_IMAGE) .

docker-build: docker-image
	docker run --rm --user "$$(id -u):$$(id -g)" -v "$(CURDIR):/work" -w /work $(DOCKER_IMAGE) \
		bash -lc 'make clean && make switch2 && ./scripts/audit-artifacts.sh'

audit:
	./scripts/audit-artifacts.sh

release:
	./scripts/package-release.sh

switch2:
ifeq ($(strip $(DEVKITPRO)),)
	$(error "DEVKITPRO is not set. Run 'make docker-build' on host or build inside the toolchain container.")
endif
	@mkdir -p $(BUILD) $(DIST_DIR)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
	@cp -f $(BUILD)/$(TARGET).nro $(DIST_NRO)
	@echo "staged ... $(DIST_NRO)"

clean-host:
	@rm -rf $(HOST_BUILD)

clean: clean-host
	@rm -rf $(BUILD) dist

else
#---------------------------------------------------------------------------------
# Sub-make executed inside $(BUILD) with DEVKITPRO set
#---------------------------------------------------------------------------------
TOPDIR		:=	$(patsubst %/,%,$(dir $(CURDIR)))
export APP_ICON	:=	$(TOPDIR)/$(ICON)
include $(DEVKITPRO)/libounce/switch2_rules

ARCH		:=	$(SWITCH2_CPU) -mtp=soft -fPIE

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export VPATH	:=	$(foreach dir,$(SOURCES),$(TOPDIR)/$(dir)) $(TOPDIR)/assets/fonts
export DEPSDIR	:=	$(CURDIR)

CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(TOPDIR)/$(dir)/*.cpp)))
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(TOPDIR)/$(dir)/*.c)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(TOPDIR)/$(dir)/*.s)))
TTFFILES	:=	$(notdir $(wildcard $(TOPDIR)/assets/fonts/*.ttf))

export LD	:=	$(CXX)
export OFILES_BIN	:=	$(TTFFILES:.ttf=.ttf.o)
export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES	:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_TTF	:=	$(TTFFILES:.ttf=_ttf.h)

LIBDIRS		:=	$(PORTLIBS) $(LIBNX)
export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(TOPDIR)/$(dir)) \
			-isystem $(PORTLIBS)/include/freetype2 \
			-isystem $(PORTLIBS)/include/harfbuzz \
			$(foreach dir,$(LIBDIRS),-isystem $(dir)/include) \
			-I$(CURDIR)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export CFLAGS	:=	-g -Wall -Wextra -Werror -O2 -ffunction-sections \
			$(ARCH) $(INCLUDE) -D__SWITCH__ -D__SWITCH2__
export CXXFLAGS	:=	$(CFLAGS) -std=c++17 -fno-exceptions -fno-rtti
export ASFLAGS	:=	-g $(ARCH)
export LDFLAGS	=	-specs=$(LIBNX)/switch.specs -g $(ARCH) -Wl,-Map,$(CURDIR)/$(TARGET).map
export LIBS	:=	-lfreetype -lharfbuzz -lpng16 -lbz2 -lz -lm -lnx

export NROFLAGS	+=	--icon=$(APP_ICON) --nacp=$(CURDIR)/$(TARGET).nacp

DEPENDS		:=	$(OFILES_SRC:.o=.d)

.PHONY: all
all: $(OUTPUT).nro

$(OUTPUT).nro: $(OUTPUT).elf $(OUTPUT).nacp

$(OUTPUT).elf: $(OFILES)

$(OFILES_SRC): $(HFILES_TTF)

%.ttf.o %_ttf.h: %.ttf
	@echo $(notdir $<)
	@(bin2s -a 4 -H $*_ttf.h $<; printf '\n') | $(AS) -o $*.ttf.o

-include $(DEPENDS)

endif

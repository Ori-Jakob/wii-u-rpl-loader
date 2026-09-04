#-------------------------------------------------------------------------------
# rpl-loader -- a WUPS plugin. Builds rpl_loader.wps.
#
#   make                 errors only in the log
#   make DEBUG=1         plus info lines
#   make DEBUG=VERBOSE   plus every hook and chain edit
#
# libwupatch is taken from ../libwupatch by default; override with
#   make LIBWUPATCH=path/to/libwupatch
# (relative to this directory).
#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/wups/share/wups_rules

WUT_ROOT   := $(DEVKITPRO)/wut
WUMS_ROOT  := $(DEVKITPRO)/wums
LIBWUPATCH ?= ../libwupatch

#-------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# INCLUDES is a list of directories containing header files
#-------------------------------------------------------------------------------
TARGET   := rpl_loader
BUILD    := build
SOURCES  := src src/rplloader $(LIBWUPATCH)/src
INCLUDES := include $(LIBWUPATCH)/include

#-------------------------------------------------------------------------------
# options for code generation
#-------------------------------------------------------------------------------
# libwupatch sizes its tables at compile time and an RPL asking for more hooks
# than these can hold is clamped. Each patch and each site also costs a 64-byte
# shim slot in .bss. Neither may exceed 255.
WUPATCH_MAX_PATCHES ?= 192
WUPATCH_MAX_SITES   ?= 160

CFLAGS   := -Wall -Wextra -O2 -ffunction-sections -fdata-sections \
            $(MACHDEP) $(INCLUDE) -D__WIIU__ -D__WUT__ -D__WUPS__ \
            -DWUPATCH_MAX_PATCHES=$(WUPATCH_MAX_PATCHES) \
            -DWUPATCH_MAX_SITES=$(WUPATCH_MAX_SITES)

ifeq ($(DEBUG),VERBOSE)
CFLAGS   += -DDEBUG -DVERBOSE_DEBUG
else ifeq ($(DEBUG),1)
CFLAGS   += -DDEBUG
endif

CXXFLAGS := $(CFLAGS) -std=c++20 -fno-exceptions -fno-rtti
ASFLAGS  := -g $(MACHDEP)
# wups.ld extends wut.ld rather than replacing it, so wut's specs come first.
# libmappedmemory's allocator pointers are imports from the MemoryMapping
# module; its linker script places their .fimport section where the plugin
# loader resolves them. Without -T libmappedmemory.ld they stay zero.
# The script must come BEFORE the WUPS specs: after wups.ld the section lands
# in the data region and ld refuses it ("not within region loadmem").
LDFLAGS   = -g $(MACHDEP) $(RPXSPECS) -T$(WUMS_ROOT)/share/libmappedmemory.ld $(WUPSSPECS) \
            -Wl,-Map,$(notdir $*.map)

LIBS     := -lwups -lfunctionpatcher -lmappedmemory
ifneq ($(wildcard $(WUMS_ROOT)/lib/libnotifications.a),)
LIBS     += -lnotifications
CFLAGS   += -DRPL_HAVE_NOTIFICATIONS
endif
LIBS     += -lwut

#-------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level
# containing include and lib
#-------------------------------------------------------------------------------
LIBDIRS  := $(PORTLIBS) $(WUPS_ROOT) $(WUMS_ROOT) $(WUT_ROOT)

#-------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add
# additional rules for different file extensions
#-------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#-------------------------------------------------------------------------------

export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)

export VPATH  := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

#-------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#-------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES     := $(OFILES_SRC)
export HFILES_BIN :=

export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all module

#-------------------------------------------------------------------------------
all: $(BUILD) module

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@rm -f $(BUILD)/rpl_build.o
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#-------------------------------------------------------------------------------
# FunctionPatcherModule with the null-name fix, from the submodule. The plugin
# refuses to patch by executable name while an unnamed module is loaded, so the
# stock module is not enough. Goes in sd:/wiiu/environments/<env>/modules/ and
# needs a reboot, like any module.
#-------------------------------------------------------------------------------
FPMODULE     := external/FunctionPatcherModule
FPMODULE_WMS := $(FPMODULE)/FunctionPatcherModule.wms

module:
	@if [ ! -f $(FPMODULE)/Makefile ]; then \
		echo "$(FPMODULE) is empty. Run: git submodule update --init"; exit 1; fi
	@if [ ! -f $(WUMS_ROOT)/lib/libkernel.a ]; then \
		echo "libkernel is missing from $(WUMS_ROOT)."; \
		echo "Get it from https://github.com/wiiu-env/libkernel and run 'make install' there."; \
		exit 1; fi
	@$(MAKE) --no-print-directory -C $(FPMODULE)

#-------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).wps $(TARGET).elf $(TARGET).lst $(TARGET).map
	@if [ -f $(FPMODULE)/Makefile ]; then \
		$(MAKE) --no-print-directory -C $(FPMODULE) clean; fi

#-------------------------------------------------------------------------------
# deploy: build, then send the plugin to a console running Aroma's wiiload
# plugin, which stores it in the plugins directory and loads it.
#
#   make deploy WIIU_IP=192.168.1.50
#   WIILOAD=tcp:192.168.1.50 make deploy      (the tool's own variable)
#-------------------------------------------------------------------------------
WIIU_IP ?=
ifneq ($(strip $(WIIU_IP)),)
export WIILOAD := tcp:$(WIIU_IP)
endif

.PHONY: deploy
deploy: $(BUILD)
	@if [ -z "$$WIILOAD" ]; then \
		echo "deploy: set WIIU_IP=<console ip> (or WIILOAD=tcp:<ip>)"; exit 1; fi
	@echo deploying $(TARGET).wps via $$WIILOAD
	@wiiload $(TARGET).wps

#-------------------------------------------------------------------------------
# deploy-ftp: build, then copy the plugin into the environment's plugins
# directory over FTP (Aroma's ftpiiu plugin), for an install that survives a
# reboot. Takes effect on the next title launch.
#
#   make deploy-ftp WIIU_IP=192.168.1.50 [ENVIRONMENT=aroma] [FTP_PORT=21]
#-------------------------------------------------------------------------------
ENVIRONMENT ?= aroma
FTP_PORT    ?= 21

.PHONY: deploy-ftp
deploy-ftp: $(BUILD)
	@if [ -z "$(WIIU_IP)" ]; then echo "deploy-ftp: set WIIU_IP=<console ip>"; exit 1; fi
	@echo deploying $(TARGET).wps to sd:/wiiu/environments/$(ENVIRONMENT)/plugins/
	@curl --silent --show-error --ftp-create-dirs -T $(TARGET).wps \
		"ftp://$(WIIU_IP):$(FTP_PORT)/fs/vol/external01/wiiu/environments/$(ENVIRONMENT)/plugins/$(TARGET).wps"
	@echo
	@echo "  *** REBOOT THE CONSOLE BEFORE TESTING ***"
	@echo "  Aroma caches plugins in memory: the file on the card is now the new"
	@echo "  build, but the running one is still the copy loaded at the last boot."
	@echo "  Relaunching the title is NOT enough. 'make deploy' (wiiload) reloads"
	@echo "  a plugin live and needs no reboot."
	@echo "  Check the first log line -- the plugin prints the build it is."
	@echo

#-------------------------------------------------------------------------------
else
.PHONY: all

DEPENDS := $(OFILES:.o=.d)

#-------------------------------------------------------------------------------
# main targets
#-------------------------------------------------------------------------------
all: $(OUTPUT).wps

$(OUTPUT).wps: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

$(OFILES_SRC): $(HFILES_BIN)

-include $(DEPENDS)

#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------

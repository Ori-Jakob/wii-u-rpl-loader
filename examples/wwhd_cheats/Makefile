#-------------------------------------------------------------------------------
# wwhd_cheats -- per-frame cheats for The Wind Waker HD, as an injected RPL.
#
#   make                          -> wwhd_cheats.rpl
#   make LIBWWHD=path/to/libwwhd  libwwhd from somewhere other than
#                                 ../../../wwhd-tools/extern/libwwhd
#   make deploy WIIU_IP=<ip> TITLE_ID=0005000010143500
#
# Same shape as the tests: this project's crt/ for the entry, -msdata=none,
# exports.def through rplexportgen.
#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/wut/share/wut_rules

TARGET   := wwhd_cheats
BUILD    := build
CRT      := ../../crt
LIBWWHD  ?= ../../../wwhd-tools/extern/libwwhd
SOURCES  := . $(CRT)
INCLUDES := ../../include $(LIBWWHD)/include

CFLAGS   := -Wall -Wextra -O2 -ffunction-sections -msdata=none \
            $(MACHDEP) $(INCLUDE) -D__WIIU__ -D__WUT__
CXXFLAGS := $(CFLAGS) -std=c++20 -fno-exceptions -fno-rtti
ASFLAGS  := -g $(MACHDEP)
LDFLAGS   = -g $(MACHDEP) -specs=$(WUT_ROOT)/share/wut.specs -specs=$(TOPDIR)/$(CRT)/rplloader.specs \
            -Wl,-Map,$(notdir $*.map)

LIBS     := -lwut
LIBDIRS  := $(PORTLIBS) $(WUT_ROOT)

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

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES     := $(OFILES_SRC) exports.o
export HFILES_BIN :=

export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).rpl $(TARGET).elf $(TARGET).lst $(TARGET).map

#-------------------------------------------------------------------------------
# deploy: build, then copy the RPL into the title's directory on the console's
# SD card over FTP (Aroma's ftpiiu plugin; empty credentials, port 21).
#-------------------------------------------------------------------------------
WIIU_IP  ?=
TITLE_ID ?= 0005000010143500
FTP_PORT ?= 21
NAME     ?= $(TARGET).rpl

.PHONY: deploy
deploy: $(BUILD)
	@if [ -z "$(WIIU_IP)" ]; then echo "deploy: set WIIU_IP=<console ip> (TITLE_ID defaults to USA)"; exit 1; fi
	@echo deploying $(TARGET).rpl to sd:/wiiu/rpl-loader/$(TITLE_ID)/$(NAME)
	@curl --silent --show-error --ftp-create-dirs -T $(TARGET).rpl \
		"ftp://$(WIIU_IP):$(FTP_PORT)/fs/vol/external01/wiiu/rpl-loader/$(TITLE_ID)/$(NAME)"
	@echo deploying $(TARGET).cfg
	@curl --silent --show-error -T $(TARGET).cfg \
		"ftp://$(WIIU_IP):$(FTP_PORT)/fs/vol/external01/wiiu/rpl-loader/$(TITLE_ID)/$(TARGET).cfg"
	@echo done

#-------------------------------------------------------------------------------
else
.PHONY: all

DEPENDS := $(OFILES:.o=.d)

all: $(OUTPUT).rpl

# elf2rpl, then the file-info name the loader lists the module by; without
# it the module is listed with a NULL name and no hook can be applied.
# Windows ships a python3/py shim that exists but is not an interpreter,
# so probe by running each candidate rather than trusting PATH.
PYTHON ?= $(firstword $(foreach p,python3 python py,$(shell $(p) -c "" >/dev/null 2>&1 && echo $(p))))
ifeq ($(strip $(PYTHON)),)
$(error no working Python found (tried python3, python, py); set PYTHON=<path>)
endif
$(OUTPUT).rpl: $(OUTPUT).elf
	@elf2rpl --rpl $< $@ $(ERROR_FILTER)
	@$(PYTHON) $(TOPDIR)/../../tools/rplname.py $@ $(notdir $@)
	@echo built ... $(notdir $@)

$(OUTPUT).elf: $(OFILES)

exports.s: $(TOPDIR)/exports.def
	@echo exports ... $(notdir $<)
	@rplexportgen $< $@

exports.o: exports.s

$(OFILES_SRC): $(HFILES_BIN)

-include $(DEPENDS)

#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------

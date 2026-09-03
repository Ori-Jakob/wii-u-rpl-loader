#-------------------------------------------------------------------------------
# chain_test -- two RPLs that hook the SAME game function, to test chaining.
#
# One source, built twice; NAME selects which half:
#
#   make NAME=chain_a           low priority,  loads first, runs second
#   make NAME=chain_b           high priority, loads second, runs first
#   make both                   both of the above
#   make deploy-both WIIU_IP=<ip>
#
#   make NAME=chain_a EXCLUSIVE=1
#       chain_a claims the site alone. The other half then cannot chain onto
#       it, its required hook reports `collided`, and it unloads itself while
#       chain_a keeps running. Objects go to their own build directory, so
#       the two variants never share a stale .o.
#
# See chain_common.h for what the two are meant to prove and how to read the
# log. Everything else matches the other examples: this project's crt/ for the
# entry, -msdata=none, rplexportgen, then tools/rplname.py for the file-info
# name the loader lists the module by.
#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)

include $(DEVKITPRO)/wut/share/wut_rules

NAME      ?= chain_a
EXCLUSIVE ?= 0
TARGET    := $(NAME)
BUILD     := build_$(NAME)$(if $(filter 1,$(EXCLUSIVE)),_x,)
CRT       := ../../crt
SOURCES   := . $(CRT)
INCLUDES  := ../../include

# -msdata=none: r2 and r13 belong to the title, so an RPL may never address
# its own globals through them.
CFLAGS   := -Wall -Wextra -O2 -ffunction-sections -msdata=none \
            -DCHAIN_EXCLUSIVE=$(EXCLUSIVE) \
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
export NAME

export VPATH  := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

# Only this half's source, not every .c in the directory.
CFILES   := $(NAME).c
CPPFILES :=
SFILES   := crt0.s

export LD := $(CC)

export OFILES_SRC := $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES     := $(OFILES_SRC) exports.o
export HFILES_BIN :=

export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all both deploy deploy-both

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

both:
	@$(MAKE) --no-print-directory NAME=chain_a
	@$(MAKE) --no-print-directory NAME=chain_b

clean:
	@echo clean ...
	@rm -fr build_chain_a build_chain_a_x build_chain_b build_chain_b_x \
		chain_a.rpl chain_b.rpl chain_a.elf chain_b.elf *.lst *.map

#-------------------------------------------------------------------------------
WIIU_IP  ?=
TITLE_ID ?= 0005000010143500
FTP_PORT ?= 21

deploy: $(BUILD)
	@if [ -z "$(WIIU_IP)" ]; then echo "deploy: set WIIU_IP=<console ip>"; exit 1; fi
	@echo deploying $(TARGET).rpl to sd:/wiiu/rpl-loader/$(TITLE_ID)/
	@curl --silent --show-error --ftp-create-dirs -T $(TARGET).rpl \
		"ftp://$(WIIU_IP):$(FTP_PORT)/fs/vol/external01/wiiu/rpl-loader/$(TITLE_ID)/$(TARGET).rpl"

# One at a time: the console's FTP server handles a single transfer.
deploy-both: both
	@$(MAKE) --no-print-directory deploy NAME=chain_a WIIU_IP=$(WIIU_IP) TITLE_ID=$(TITLE_ID)
	@$(MAKE) --no-print-directory deploy NAME=chain_b WIIU_IP=$(WIIU_IP) TITLE_ID=$(TITLE_ID)
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

exports.s: $(TOPDIR)/$(NAME).def
	@echo exports ... $(notdir $<)
	@rplexportgen $< $@

exports.o: exports.s

$(OFILES_SRC): $(HFILES_BIN)

-include $(DEPENDS)

#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------

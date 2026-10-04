.SUFFIXES:

ifeq ($(strip $(DEVKITPPC)),)
$(error "Set DEVKITPPC in your environment")
endif

include $(DEVKITPPC)/gamecube_rules

PROJECT_DIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))
TARGET := jellycube
BUILD := build
VERSION := $(strip $(shell cat $(PROJECT_DIR)VERSION 2>/dev/null || echo 0.0.0-unknown))
GIT_SHA := $(strip $(shell git -C $(PROJECT_DIR) rev-parse --short HEAD 2>/dev/null || echo nogit))
RELEASE_TAG := v$(VERSION)+$(GIT_SHA)
SOURCES := source
DATA := data
INCLUDES := include third_party

CFLAGS = -g -O2 -Wall -Wextra -Werror $(MACHDEP) $(INCLUDE) $(EXTRA_CFLAGS) -DJC_VERSION='"$(VERSION)+$(GIT_SHA)"'
CXXFLAGS = $(CFLAGS)
LDFLAGS = -g $(MACHDEP) -Wl,-Map,$(notdir $@).map

LIBS := -lbba -lfat -logc -lm

LIBDIRS := $(PORTLIBS)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
    export LD := $(CC)
else
    export LD := $(CXX)
endif

export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(sFILES:.s=.o) $(SFILES:.S=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)
export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD) \
                  -I$(LIBOGC_INC)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
                   -L$(LIBOGC_LIB)

.PHONY: $(BUILD) clean run dolphin release

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) build-dolphin $(TARGET).elf $(TARGET).dol jellycube-dolphin.elf jellycube-dolphin.dol

run:
	wiiload $(TARGET).dol

dolphin:
	@test -f source/dolphin_test.local.c || { echo "Create source/dolphin_test.local.c from source/dolphin_test.local.c.example"; exit 1; }
	@[ -d build-dolphin ] || mkdir -p build-dolphin
	@$(MAKE) --no-print-directory -C build-dolphin -f $(CURDIR)/Makefile TARGET=jellycube-dolphin BUILD=build-dolphin OUTPUT=$(CURDIR)/jellycube-dolphin EXTRA_CFLAGS=-DJC_DOLPHIN_TEST

release: clean
	@$(MAKE) --no-print-directory
	@$(MAKE) --no-print-directory dolphin
	@mkdir -p dist
	@cp jellycube.dol dist/jellycube-$(RELEASE_TAG)-gamecube.dol
	@cp jellycube-dolphin.dol dist/jellycube-$(RELEASE_TAG)-dolphin.dol
	@printf 'JellyCube %s\nCommit: %s\n' "$(VERSION)" "$(GIT_SHA)" > dist/jellycube-$(RELEASE_TAG)-manifest.txt
	@sha256sum dist/jellycube-$(RELEASE_TAG)-*.dol >> dist/jellycube-$(RELEASE_TAG)-manifest.txt
	@$(MAKE) --no-print-directory clean
	@echo "Created dist/jellycube-$(RELEASE_TAG)-{gamecube,dolphin}.dol"

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

$(OFILES_SOURCES) : $(HFILES)

%.jpg.o %jpg.h : %.jpg
	@echo $(notdir $<)
	$(bin2o)

-include $(DEPENDS)

endif

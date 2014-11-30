# build/output directory
O        ?= build

# project name
PROJECT   = automash

CSRC     := $(shell find src/ -name '*.c')
CXXSRC   := $(shell find src/ -name '*.cpp')
ASRC     := $(shell find src/ -name '*.S')

INC      := $(shell find src/ -type d)

LIBRARIES = m stdc++

OBJ      := $(patsubst %.S,$(O)/%.o,$(notdir $(ASRC))) $(patsubst %.c,$(O)/%.o,$(notdir $(CSRC))) $(patsubst %.cpp,$(O)/%.o,$(notdir $(CXXSRC)))
DEP      := $(patsubst %o,%d,$(OBJ))

OUT      := $(O)/$(PROJECT)

TOOLCHAIN_PATH =
ARCH     ?=
PREFIX    = $(ARCH)

# ARCH     := arm
# PREFIX    = $(ARCH)-

FLAGS    += -O$(OPTIMIZE)
FLAGS    += -ffunction-sections -fdata-sections
FLAGS    += -Wall -g
FLAGS    += -funsigned-char -funsigned-bitfields -fshort-enums
FLAGS    += $(patsubst %,-I%,$(INC))
FLAGS    += $(patsubst %,-D%,$(CDEFS))

CFLAGS   += $(FLAGS) -std=gnu11 -pipe

ASFLAGS  += $(FLAGS)

CXXFLAGS += $(FLAGS) -fno-rtti -fno-exceptions -std=gnu++11 -pipe

LDFLAGS  += $(FLAGS) -Wl,--as-needed,--gc-sections
LDFLAGS  += -Wl,-Map=$(O)/$(PROJECT).map

LIBS     += $(patsubst %,-l%,$(LIBRARIES))

###

# depend on eigen3
# CFLAGS   += $(shell pkg-config --cflags eigen3)
# LIBS     += $(shell pkg-config --libs eigen3)

###

VPATH     = $(O) $(shell find src/ -type d)

CC        = $(TOOLCHAIN_PATH)$(PREFIX)gcc
CXX       = $(TOOLCHAIN_PATH)$(PREFIX)g++
OBJCOPY   = $(TOOLCHAIN_PATH)$(PREFIX)objcopy
OBJDUMP   = $(TOOLCHAIN_PATH)$(PREFIX)objdump
AR        = $(TOOLCHAIN_PATH)$(PREFIX)ar
SIZE      = $(TOOLCHAIN_PATH)$(PREFIX)size
READELF   = $(TOOLCHAIN_PATH)$(PREFIX)readelf
NM        = $(TOOLCHAIN_PATH)$(PREFIX)nm
RM        = $(TOOLCHAIN_PATH)$(PREFIX)rm -f
RMDIR     = $(TOOLCHAIN_PATH)$(PREFIX)rmdir
MKDIR     = $(TOOLCHAIN_PATH)$(PREFIX)mkdir -p


.PHONY: all clean run

all: $(OUT)

clean:
	$(RM) $(OUT)
	$(RM) $(OBJ)
	$(RM) $(DEP)
	$(RM) $(O)/$(PROJECT).map
	$(RMDIR) $(O)

run: $(OUT)
	-./$(OUT)

$(O):
	$(MKDIR) $(O)

$(OUT): $(OBJ) | $(O)
	@echo "  LINK  " $@
	@$(CXX) $(LDFLAGS) -o $@ $^ $(LIBS)

$(O)/%.o: %.cpp | $(O)
	@echo "  CXX   " $@
	@$(CXX) $(CXXFLAGS) -c -o $@ $^

$(O)/%.o: %.c | $(O)
	@echo "  CC    " $@
	@$(CC) $(CFLAGS) -c -o $@ $^

$(O)/%.o: $.S | $(O)
	@echo "  AS    " $@
	@$(AS) $(ASFLAGS) -c -o $@ $^

#autodep

$(O)/%.d: %.cpp | $(O)
	@echo "  DEP   " $@
	@$(CXX) -MM $(CXXFLAGS) $< | sed -e 's!\($*\)\.o[ :]+!\1.o \1.d:!' > $@

$(O)/%.d: %.c | $(O)
	@echo "  DEP   " $@
	@$(CC) -MM $(CFLAGS) $< | sed -e 's!\($*\)\.o[ :]+!\1.o \1.d:!' > $@

include $(DEP)

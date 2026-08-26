EE_CC      = mips64r5900el-ps2-elf-gcc
EE_STRIP   = mips64r5900el-ps2-elf-strip

EE_OBJS = \
	main.o \
	config.o \
	graphics.o \
	font.o

EE_LIBS = \
	-lfreetype \
	-lgskit_toolkit \
	-lgskit \
	-ldmakit \
	-ljpeg_ps2_addons \
	-ljpeg \
	-lpng16 \
	-lz \
	-lpad \
	-lfileXio \
	-lkernel

EE_INCS = \
	-I$(PS2SDK)/ports/include \
	-I$(PS2SDK)/ports/include/freetype2 \
	-I$(PS2SDK)/ee/include \
	-I$(PS2SDK)/common/include \
	-I$(GSKIT)/include \
	-I.

EE_LDFLAGS = \
	-L$(PS2SDK)/ports/lib \
	-L$(PS2SDK)/ee/lib \
	-L$(GSKIT)/lib \
	-T$(PS2SDK)/ee/startup/linkfile

EE_CFLAGS = \
	-D_EE \
	-G0 \
	-O2 \
	-Wall \
	-Wextra \
	-Wno-unused-parameter \
	-gdwarf-2 \
	-gz

EE_LDFLAGS += \
	-Wl,-z,max-page-size=128

EE_TARGET = LOADER.ELF

all: $(EE_TARGET)

$(EE_TARGET): $(EE_OBJS)
	$(EE_CC) $(EE_CFLAGS) \
		$(EE_LDFLAGS) \
		-o $@ \
		$(EE_OBJS) \
		$(EE_LIBS)

%.o: %.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

clean:
	rm -f $(EE_TARGET) $(EE_OBJS)

.PHONY: all clean

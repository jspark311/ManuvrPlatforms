################################################################################
# Makefile for WorldCrafter
# Author: J. Ian Lindsay
#
#
FIRMWARE_NAME      = WorldCrafter
CXX_STANDARD       = gnu++17
OPTIMIZATION       = -O2

C3P_CONF  = -DCONFIG_C3P_CBOR
C3P_CONF += -DCONFIG_C3P_IMG_SUPPORT -DCONFIG_C3P_WITH_LIBPNG
C3P_CONF += -DCONFIG_C3P_TRACE_ENABLED
#C3P_CONF += -DCONFIG_C3P_WITH_BZ2


###########################################################################
# Environmental awareness...
###########################################################################
SHELL              = /bin/bash
export CXX         = $(shell which g++)
export AR          = $(shell which ar)
export SZ          = $(shell which size)
export MAKE        = $(shell which make)
export BUILD_ROOT  = $(shell pwd)
export OUTPUT_PATH = $(BUILD_ROOT)/build/

C3P_PATH  = $(BUILD_ROOT)/../CppPotpourri

###########################################################################
# Source files, includes, and linker directives...
###########################################################################
INCLUDES  = -I../CppPotpourri/src -I../../ -Isrc

CXX_SRCS  = $(wildcard src/*.cpp)
CXX_SRCS += $(wildcard ../../src/GUI/X11/*.cpp)
CXX_SRCS += ../../src/Linux.cpp
CXX_SRCS += ../../src/LinuxStdIO.cpp
CXX_SRCS += ../../src/ImagePNG_IO.cpp

# Libraries to link against.
LIBS	= -L$(OUTPUT_PATH) -lc3p_64 -lstdc++ -lm -lpthread -lX11 -lpng -lz
#LIBS += -lbz2


###########################################################################
# Paramter packaging and error-checking...
###########################################################################
CXXFLAGS  = $(OPTIMIZATION) -std=$(CXX_STANDARD)
CXXFLAGS += -Wl,--gc-sections
CXXFLAGS += -fsingle-precision-constant -fno-rtti -fno-exceptions
CXXFLAGS += -Wdouble-promotion -Wall
CXXFLAGS += $(C3P_CONF)

export CXXFLAGS

OBJS = $(C_SRCS:.c=.o) $(CXX_SRCS:.cpp=.o)


###########################################################################
# Rules for building the firmware follow...
###########################################################################

%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $^

%.o : %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $^


default:	$(FIRMWARE_NAME)

libc3p:
	@echo "\033[1;37m=== libc3p ==========================================\033[0m"
	$(MAKE) lib -C $(C3P_PATH)

$(FIRMWARE_NAME):	libc3p $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $(FIRMWARE_NAME) $(OBJS) $(LIBS)

clean:
	rm -f *.d *.o *.su *~ $(OBJS) $(DEP_FILES) $(FIRMWARE_NAME) *.png

fullclean: clean
	rm -f $(OUTPUT_PATH)/*.a
	$(MAKE) clean -C $(C3P_PATH)

################################################################################
# Makefile for ImageAPI
# Author: J. Ian Lindsay
# Date:   2025.06.26
#
# This makefile builds an optional linux tool for managing other firmware built
#   with support for M2M. The build will match the host's ALU width.
# The options this program is compiled with will dictate what hardware features
#   it can support.
#
FIRMWARE_NAME      = ImageAPI
CXX_STANDARD       = gnu++17
OPTIMIZATION       = -O2

MANUVR_CONF  = -DCONFIG_C3P_CBOR
MANUVR_CONF += -DCONFIG_C3P_IMG_SUPPORT
MANUVR_CONF += -DCONFIG_C3P_TRACE_ENABLED


###########################################################################
# Environmental awareness...
###########################################################################
SHELL              = /bin/bash
export CXX         = g++  # $(shell which g++)
export AR          = ar   # $(shell which ar)
export SZ          = size # $(shell which size)
export MAKE        = make # $(shell which make)
export BUILD_ROOT  = ./   # $(shell pwd)


###########################################################################
# Source files, includes, and linker directives...
###########################################################################
INCLUDES  = -I../CppPotpourri/src -I../../ -Isrc

CXX_SRCS  = src/*.cpp
CXX_SRCS += ../CppPotpourri/src/*.cpp
CXX_SRCS += ../CppPotpourri/src/TimerTools/*.cpp
CXX_SRCS += ../CppPotpourri/src/cbor-cpp/*.cpp
CXX_SRCS += ../CppPotpourri/src/C3PValue/*.cpp
CXX_SRCS += ../CppPotpourri/src/Pipes/TripleAxisPipe/*.cpp
CXX_SRCS += ../CppPotpourri/src/Pipes/BufferAccepter/*.cpp
CXX_SRCS += ../CppPotpourri/src/Console/*.cpp
CXX_SRCS += ../CppPotpourri/src/Image/*.cpp
CXX_SRCS += ../CppPotpourri/src/Image/GfxUI/*.cpp
CXX_SRCS += ../CppPotpourri/src/Image/ImageUtils/*.cpp
CXX_SRCS += ../CppPotpourri/src/C3PRandom/*.cpp
CXX_SRCS += ../../src/GUI/X11/*.cpp
CXX_SRCS += ../../src/Linux.cpp
CXX_SRCS += ../../src/LinuxStdIO.cpp


# Libraries to link against.
LIBS	= -L$(OUTPUT_PATH) -lstdc++ -lm -lpthread -lX11


###########################################################################
# Paramter packaging and error-checking...
###########################################################################
CXXFLAGS  = $(OPTIMIZATION) -std=$(CXX_STANDARD)
CXXFLAGS += -Wl,--gc-sections
CXXFLAGS += -fsingle-precision-constant -fno-rtti -fno-exceptions
CXXFLAGS += -Wdouble-promotion -Wall
CXXFLAGS += $(INCLUDES)
CXXFLAGS += $(MANUVR_CONF)

LIBS     += $(CRYPTO_LIBS)

OBJS = $(C_SRCS:.c=.o) $(CXX_SRCS:.cpp=.o)


###########################################################################
# Rules for building the firmware follow...
###########################################################################

default:	$(FIRMWARE_NAME)

debug:  $(FIRMWARE_NAME).o
	$(CXX) $(CXXFLAGS) -ggdb -g -pg -o $(FIRMWARE_NAME) *.o $(LIBS)

$(FIRMWARE_NAME):	$(FIRMWARE_NAME).o $(LIBS_TO_BUILD)
	$(CXX) $(CXXFLAGS) -o $(FIRMWARE_NAME) *.o $(LIBS)

$(FIRMWARE_NAME).o:
	$(CXX) $(CXXFLAGS) $(CFLAGS) -c $(CXX_SRCS) -fno-exceptions

clean:
	rm -f $(FIRMWARE_NAME) *.o *~

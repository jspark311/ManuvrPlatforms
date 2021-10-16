###########################################################################
# Makefile for rhom (Right-Hand of Manuvr)
# Author: J. Ian Lindsay
# Date:   2021.10.16
#
# This makefile builds an optional linux tool for opening serial ports and
#   excercising the M2M classes against hardware.
###########################################################################
FIRMWARE_NAME      = rhom
OPTIMIZATION       = -O2
CXX_STANDARD       = gnu++11

###########################################################################
# Environmental awareness...
###########################################################################
SHELL          = /bin/sh
export CXX     = $(shell which g++)
export AR      = $(shell which ar)
export SZ      = $(shell which size)
export MAKE    = $(shell which make)

export BUILD_ROOT    = $(shell pwd)
export OUTPUT_PATH   = $(BUILD_ROOT)/build/

CXXFLAGS  = -I../CppPotpourri/src -I../../ -Wl,--gc-sections -Wall
CXXFLAGS += -fsingle-precision-constant -Wdouble-promotion -fno-rtti -fno-exceptions
CXXFLAGS += $(OPTIMIZATION) -std=$(CXX_STANDARD) -m32
CXXFLAGS += -DCONFIG_MANUVR_CBOR

LIBS	= -L$(OUTPUT_PATH) -lstdc++ -lm -lpthread


###########################################################################
# Source files, includes, and linker directives...
###########################################################################

SRCS    = rhom.cpp
SRCS   += ../CppPotpourri/src/SensorFilter.cpp
SRCS   += ../CppPotpourri/src/StopWatch.cpp
SRCS   += ../CppPotpourri/src/StringBuilder.cpp
SRCS   += ../CppPotpourri/src/EnumeratedTypeCodes.cpp
SRCS   += ../CppPotpourri/src/KeyValuePair.cpp
SRCS   += ../CppPotpourri/src/AbstractPlatform.cpp
SRCS   += ../CppPotpourri/src/UARTAdapter.cpp
SRCS   += ../CppPotpourri/src/I2CAdapter.cpp
SRCS   += ../CppPotpourri/src/uuid.cpp
SRCS   += ../CppPotpourri/src/BusQueue.cpp
SRCS   += ../CppPotpourri/src/GPSWrapper.cpp
SRCS   += ../CppPotpourri/src/ParsingConsole.cpp
SRCS   += ../CppPotpourri/src/Quaternion.cpp
SRCS   += ../CppPotpourri/src/Storage.cpp
SRCS   += ../CppPotpourri/src/TripleAxisPipe.cpp
SRCS   += ../CppPotpourri/src/Image/*.cpp
SRCS   += ../CppPotpourri/src/cbor-cpp/*.cpp
SRCS   += ../CppPotpourri/src/ManuvrLink/*.cpp
SRCS   += ../CppPotpourri/src/Identity/*.cpp
SRCS   += ../../src/Linux.cpp
SRCS   += ../../src/LinuxStdIO.cpp
SRCS   += ../../src/LinuxUART.cpp
#SRCS   += ../../src/I2CAdapter.cpp


###########################################################################
# Rules for building the firmware follow...
###########################################################################

default:	$(FIRMWARE_NAME)

builddir:
	mkdir -p $(OUTPUT_PATH)

debug:  $(FIRMWARE_NAME).o
	$(CXX) $(CXXFLAGS) -ggdb -g -pg -o $(FIRMWARE_NAME) *.o $(LIBS)

$(FIRMWARE_NAME):	$(FIRMWARE_NAME).o
	$(CXX) $(CXXFLAGS) -o $(FIRMWARE_NAME) *.o $(LIBS)

$(FIRMWARE_NAME).o:
	$(CXX) $(CXXFLAGS) $(CFLAGS) -c $(SRCS) -fno-exceptions

clean:
	rm -rf $(OUTPUT_PATH)
	rm -f $(FIRMWARE_NAME) *.o *~

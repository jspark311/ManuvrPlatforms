###########################################################################
# Makefile for generic linux example.
# Author: J. Ian Lindsay
# Date:   2021.05.16
#
###########################################################################

FIRMWARE_NAME      = linux-example

CC		= g++
CXXFLAGS = -I../CppPotpourri/src -I../../ -Wl,--gc-sections -Wall
LIBS	= -L$(OUTPUT_PATH) -L$(BUILD_ROOT)/lib -lstdc++ -lcrypto -lm -lpthread

export BUILD_ROOT    = $(shell pwd)
export OUTPUT_PATH   = $(BUILD_ROOT)/build/

###########################################################################
# Source files, includes, and linker directives...
###########################################################################

SRCS    = main.cpp
SRCS   += ../CppPotpourri/src/*.cpp
SRCS   += ../CppPotpourri/src/Identity/*.cpp
SRCS   += ../CppPotpourri/src/cbor-cpp/*.cpp
SRCS   += ../../src/Linux.cpp
SRCS   += ../../src/LinuxStdIO.cpp


###########################################################################
# Rules for building the firmware follow...
###########################################################################

default:	$(FIRMWARE_NAME)

builddir:
	mkdir -p $(OUTPUT_PATH)

debug:  linux-example.o
	$(CC) $(CXXFLAGS) -ggdb -g -pg -o $(FIRMWARE_NAME) *.o $(LIBS)

$(FIRMWARE_NAME):	$(FIRMWARE_NAME).o
	$(CC) $(CXXFLAGS) -o $(FIRMWARE_NAME) *.o $(LIBS)

$(FIRMWARE_NAME).o:
	$(CC) $(CXXFLAGS) $(CFLAGS) -c $(SRCS) -fno-exceptions

clean:
	rm -rf $(OUTPUT_PATH)
	rm -f $(FIRMWARE_NAME) *.o *~

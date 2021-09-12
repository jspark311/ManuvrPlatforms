# Makefile for generic linux example.
# Author: J. Ian Lindsay

CC		= g++
CXXFLAGS = -I./lib/CppPotpourri/src -I../ -Wl,--gc-sections -Wall
LIBS	= -L$(OUTPUT_PATH) -L$(BUILD_ROOT)/lib -lstdc++ -lcrypto 

export BUILD_ROOT    = $(shell pwd)
export OUTPUT_PATH   = $(BUILD_ROOT)/build/

SRCS    = src/main.cpp
SRCS   += lib/CppPotpourri/src/*.cpp
SRCS   += ../src/Linux.cpp
SRCS   += ../src/LinuxStdIO.cpp

default:	linux-example

builddir:
	mkdir -p $(OUTPUT_PATH)

debug:  linux-example.o
	$(CC) $(CXXFLAGS) -ggdb -g -pg -o linux-example *.o $(LIBS)

linux-example:	linux-example.o
	$(CC) $(CXXFLAGS) -o linux-example *.o $(LIBS)

linux-example.o:
	$(CC) $(CXXFLAGS) $(CFLAGS) -c $(SRCS) -fno-exceptions

clean:
	rm -rf $(OUTPUT_PATH)
	rm -f librarian *.o *~

fullclean: clean
	$(MAKE) clean -C lib/

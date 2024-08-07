################################################################################
# Makefile for rhom (Right Hand of Manuvr)
# Author: J. Ian Lindsay
# Date:   2021.10.16
#
# This makefile builds an optional linux tool for managing other firmware built
#   with support for M2M. The build will match the host's ALU width.
# The options this program is compiled with will dictate what hardware features
#   it can support.
#
################################################################################
# Build configuration
# NOTE: In ManuvrOS, these directives were built from ManuvrConf.h. This is
#   a simpler solution, but it would still be better to use kconfig.
################################################################################
FIRMWARE_NAME      = c3pdemo
CXX_STANDARD       = gnu++11
OPTIMIZATION       = -O2

CONFIG_C3P_CRYPTO = mbedtls
C3P_CONF  = -DCONFIG_C3P_CBOR
C3P_CONF += -DCONFIG_C3P_IMG_SUPPORT
C3P_CONF += -DCONFIG_C3P_M2M_SUPPORT
C3P_CONF += -DCONFIG_C3P_SOCKET_WRAPPER
C3P_CONF += -DCONFIG_C3P_TRACE_ENABLED


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


###########################################################################
# Source files, includes, and linker directives...
###########################################################################
INCLUDES  = -I../CppPotpourri/src -I../../ -Isrc

CXX_SRCS  = src/*.cpp
CXX_SRCS += ../CppPotpourri/src/*.cpp
CXX_SRCS += ../CppPotpourri/src/BusQueue/*.cpp
CXX_SRCS += ../CppPotpourri/src/TimerTools/*.cpp
CXX_SRCS += ../CppPotpourri/src/Identity/*.cpp
CXX_SRCS += ../CppPotpourri/src/cbor-cpp/*.cpp
CXX_SRCS += ../CppPotpourri/src/C3PValue/*.cpp
CXX_SRCS += ../CppPotpourri/src/TimeSeries/*.cpp
CXX_SRCS += ../CppPotpourri/src/Pipes/TripleAxisPipe/*.cpp
CXX_SRCS += ../CppPotpourri/src/Pipes/BufferAccepter/*.cpp
CXX_SRCS += ../CppPotpourri/src/Storage/*.cpp
CXX_SRCS += ../CppPotpourri/src/Storage/RecordTypes/*.cpp
CXX_SRCS += ../CppPotpourri/src/Console/*.cpp
CXX_SRCS += ../CppPotpourri/src/Image/*.cpp
CXX_SRCS += ../CppPotpourri/src/Image/GfxUI/*.cpp
CXX_SRCS += ../CppPotpourri/src/Image/ImageUtils/*.cpp
CXX_SRCS += ../CppPotpourri/src/M2MLink/*.cpp
CXX_SRCS += ../CppPotpourri/src/CryptoBurrito/*.cpp
CXX_SRCS += ../CppPotpourri/src/CryptoBurrito/Providers/*.cpp
CXX_SRCS += ../../src/GUI/X11/*.cpp
CXX_SRCS += ../../src/Linux.cpp
CXX_SRCS += ../../src/LinuxStdIO.cpp
CXX_SRCS += ../../src/LinuxUART.cpp
CXX_SRCS += ../../src/LinuxSocketPipe.cpp
CXX_SRCS += ../../src/LinuxSockListener.cpp
CXX_SRCS += ../../src/C3PLinuxFile.cpp
#CXX_SRCS += ../../src/I2CAdapter.cpp

# Libraries to link against.
LIBS	= -L$(OUTPUT_PATH) -lstdc++ -lm -lpthread -lX11


###########################################################################
# Paramter packaging and error-checking...
###########################################################################
ifeq ($(CONFIG_C3P_CRYPTO),mbedtls)
INCLUDES    += -I$(BUILD_ROOT)/lib/mbedtls/include/
LIBS        += -lmbedtls
LIBS        += -lmbedx509
LIBS        += -lmbedcrypto
C3P_CONF    += -DCONFIG_C3P_MBEDTLS
# mbedTLS will require this in order to use our chosen options.
CXXFLAGS    += -DMBEDTLS_CONFIG_FILE='<mbedTLS_conf.h>'
LIBS_TO_BUILD  = mbedtls.a
endif

CXXFLAGS  = $(OPTIMIZATION) -std=$(CXX_STANDARD)
CXXFLAGS += -Wl,--gc-sections
CXXFLAGS += -fsingle-precision-constant -fno-rtti -fno-exceptions
CXXFLAGS += -Wdouble-promotion -Wall
CXXFLAGS += $(INCLUDES)
CXXFLAGS += $(C3P_CONF)

LIBS     += $(CRYPTO_LIBS)

OBJS = $(C_SRCS:.c=.o) $(CXX_SRCS:.cpp=.o)


###########################################################################
# Rules for building the firmware follow...
###########################################################################

default:	$(FIRMWARE_NAME)

builddir:
	mkdir -p $(OUTPUT_PATH)

debug:  $(FIRMWARE_NAME).o
	$(CXX) $(CXXFLAGS) -ggdb -g -pg -o $(FIRMWARE_NAME) *.o $(LIBS)

$(FIRMWARE_NAME):	builddir $(FIRMWARE_NAME).o $(LIBS_TO_BUILD)
	$(CXX) $(CXXFLAGS) -o $(FIRMWARE_NAME) *.o $(LIBS)

$(FIRMWARE_NAME).o: builddir
	$(CXX) $(CXXFLAGS) $(CFLAGS) -c $(CXX_SRCS) -fno-exceptions

mbedtls.a: builddir
	@echo "=== Building mbedtls =========================================="
	$(MAKE) lib -C lib/mbedtls/
	cp -RP lib/mbedtls/library/*.a $(OUTPUT_PATH)

clean:
	rm -rf $(OUTPUT_PATH)
	rm -f $(FIRMWARE_NAME) *.o *~

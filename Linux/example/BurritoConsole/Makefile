###########################################################################
# Makefile for linux CryptoBurrito example.
# Author: J. Ian Lindsay
# Date:   2022.05.07
#
###########################################################################

FIRMWARE_NAME = BurritoConsole
CXX_STANDARD = gnu++11
CC	         = g++

export BUILD_ROOT    = $(shell pwd)
export OUTPUT_PATH   = $(BUILD_ROOT)/

###########################################################################
# Source files, includes, and linker directives...
###########################################################################
CXXFLAGS  = -Wl,--gc-sections -Wall
INCLUDES  = -I../CppPotpourri/src -I../../
LIBS	    = -L$(OUTPUT_PATH) -L$(BUILD_ROOT)/lib -lstdc++ -lm -lpthread

SRCS    = main.cpp
SRCS   += ../CppPotpourri/src/*.cpp
SRCS   += ../CppPotpourri/src/Identity/*.cpp
SRCS   += ../CppPotpourri/src/cbor-cpp/*.cpp
SRCS   += ../CppPotpourri/src/CryptoBurrito/*.cpp
SRCS   += ../CppPotpourri/src/CryptoBurrito/Providers/*.cpp
SRCS   += ../../src/Linux.cpp
SRCS   += ../../src/LinuxStdIO.cpp


###########################################################################
# Option conditionals
###########################################################################
#ifeq ($(CRYPTO),MBED)
INCLUDES    += -I$(BUILD_ROOT)/lib/mbedtls/include/
CRYPTO_LIBS += -lmbedtls
CRYPTO_LIBS += -lmbedx509
CRYPTO_LIBS += -lmbedcrypto
CXXFLAGS    += -DWITH_MBEDTLS
# mbedTLS will require this in order to use our chosen options.
BURRITO_OPTIONS += -DMBEDTLS_CONFIG_FILE='<mbedTLS_conf.h>'
LIBS_TO_BUILD  += mbedtls.a
#endif


# TODO: OpenSSL

# TODO: TinyDTLS

# TODO: Sodium

LIBS     += $(CRYPTO_LIBS)
CXXFLAGS += -std=$(CXX_STANDARD)
CXXFLAGS += $(INCLUDES)

OBJS  = $(C_SRCS:.c=.o) $(CXX_SRCS:.cpp=.o)


###########################################################################
# Rules for building the firmware follow...
###########################################################################

default:	$(FIRMWARE_NAME)

builddir:
	mkdir -p $(OUTPUT_PATH)

debug:  $(FIRMWARE_NAME).o
	$(CC) $(CXXFLAGS) -ggdb -g -pg -o $(FIRMWARE_NAME) *.o $(LIBS)

$(FIRMWARE_NAME):	$(FIRMWARE_NAME).o $(LIBS_TO_BUILD)
	$(CC) $(CXXFLAGS) -o $(FIRMWARE_NAME) *.o $(LIBS)

$(FIRMWARE_NAME).o:
	$(CC) $(CXXFLAGS) $(CFLAGS) -c $(SRCS) -fno-exceptions

mbedtls.a:
	@echo "\033[1;37m=== mbedtls ==========================================\033[0m"
	$(MAKE) lib -C lib/mbedtls/
	cp -RP lib/mbedtls/library/*.a $(OUTPUT_PATH)

clean:
	$(MAKE) clean -C lib/mbedtls/
	rm -f $(OBJS) $(FIRMWARE_NAME) *.a *.o

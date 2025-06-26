###########################################################################
# ManuvrLinux platform Makefile
# Author: J. Ian Lindsay
#
# This is where we conditionally compile platform-support code.
#
# Cryptographic support is also handled by this Makefile.
###########################################################################

C_STANDARD         = gnu99
CXX_STANDARD       = gnu++11

# Includes
INCLUDES = -I../ -iquote.

# Libraries
LIBS   =

# Driver-specific flags.
LOCAL_FLAGS  =  -std=$(CXX_STANDARD) -nostdlib

# Sources
CPP_SRCS    = src/Linux.cpp
CPP_SRCS   += src/LinuxStdIO.cpp
CPP_SRCS   += src/C3PLinuxFile.cpp
CPP_SRCS   += src/LinuxSocketPipe.cpp
CPP_SRCS   += src/LinuxUART.cpp
#CPP_SRCS   += src/LinuxStorage.cpp
#CPP_SRCS   += src/I2CAdapter.cpp
#CPP_SRCS   += src/SPIAdapter.cpp

#CPP_SRCS   += Cryptographic/Cryptographic.cpp
#CPP_SRCS   += Cryptographic/MbedTLS.cpp
#CPP_SRCS   += Cryptographic/OpenSSL.cpp
#CPP_SRCS   += Cryptographic/Blind.cpp

ifeq ($(MANUVR_BOARD),RASPI)
CPP_SRCS   += src/Raspi/DieThermometer.cpp
CPP_SRCS   += src/Raspi/Raspi.cpp
endif  # Board selection: Raspi

COV_FILES = $(CPP_SRCS:.cpp=.gcda) $(CPP_SRCS:.cpp=.gcno) $(C_SRCS:.c=.gcno) $(C_SRCS:.c=.gcno)

all:
	@echo ''
	@echo ''
	@echo '======================================================'
	@echo 'Platform source files:'
	@echo $(C_SRCS)
	@echo $(CPP_SRCS)
	@echo '======================================================'
	$(CXX) -r $(INCLUDES) $(LOCAL_FLAGS) $(CXXFLAGS) $(LIBS) $(C_SRCS) $(CPP_SRCS) -o Platform.o


clean:
	rm -f Platform.o $(COV_FILES) *.gcno *.gcda

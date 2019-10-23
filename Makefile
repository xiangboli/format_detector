# Makefile for Linux / OSX

CC = gcc

OS := $(shell uname -s)

CFLAGS = -Wall -std=c99 -Wno-unused-function -D_BSD_SOURCE -DVERSION=\"$(RELEASE_VERSION)\"

LIBS   = -lm

ifeq ($(OS),Linux)
  CFLAGS += -Wno-sequence-point -Wno-maybe-uninitialized -Wno-unused-but-set-variable -D_POSIX_C_SOURCE=199309L
  LIBS   += -lrt
endif

ifeq ($(OS),Darwin)
  CC = gcc-4.9
endif

TARGET = format_detector
TARGET_D = format_detector_d

INCLUDE = -I include/ -I common/timer/include/

SRC = src/format_detector.c \
	  src/format_detector_utils.c \
	  src/loss_funcs_avx2.c \
	  common/timer/src/timer.c 

INSTALLDIR=/usr/local/bin/

OBJ = $(SRC:%.c=%.o)

# Compile all matched pattern .c files to .o files
%.o: %.c 
	$(CC) -c -O $(CFLAGS) $(INCLUDE) $< -o $@

# Override .o object file with extra flags
src/loss_funcs_avx2.o: CFLAGS += -mavx2

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(LIBS) $^ -o $@

$(TARGET_D): $(OBJ)
	$(CC) $(CFLAGS) $(LIBS) $^ -o $@

all: $(TARGET)

debug: CFLAGS += -g -DDEBUG
debug: $(TARGET_D)

clean:
	rm -f $(TARGET) $(TARGET_D) $(OBJ)
 
install: all
	cp $(TARGET) $(INSTALLDIR)

uninstall:
	rm -f $(INSTALLDIR)/$(TARGET)
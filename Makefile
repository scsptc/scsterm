#
# SCS Term
#
# Copyright (C) 2020-2021 SCS GmbH & Co. KG
#

# Version number of the project
TARGET = "scsterm"
VERSION = 0.1

# define compiler flags
CFLAGS += -DVERSION=\"$(VERSION)\"
CFLAGS += -O3 -Wall -pedantic -Wno-unused-result -Werror=implicit-function-declaration

# define used libraries
LIBS += -lusb-1.0

# source files
OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

# general rules
$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Build the executable
all: $(TARGET)
	strip $(TARGET)

.PHONY: zip
zip:
	zip -r $(TARGET)_$(VERSION).zip README.md LICENSE Makefile *.c *.h

.PHONY: clean
clean:
	-$(RM) $(TARGET) *.o

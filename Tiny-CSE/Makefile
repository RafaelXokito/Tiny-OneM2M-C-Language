CC = gcc
ifeq ($(UNAME_S),Linux)
    CFLAGS = -Wall -Werror -std=c99 -Iinclude -fPIC
else
    CFLAGS = -Iinclude -fPIC
endif
DEPS = HTTP_Server.h Sqlite.h
EXEC = server
SOURCES = $(wildcard src/*.c)
OBJECTS = $(SOURCES:.c=.o)
FLAGS = -g -Wall -lm -ldl -fPIC -rdynamic -I./include
LIBRARIES = -lsqlite3

ifeq ($(OS),Windows_NT)
    TARGET = $(EXEC).exe
else
    TARGET = $(EXEC)
endif

LDFLAGS = -static

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(FLAGS) $(LDFLAGS) -o $(TARGET) $(LIBRARIES)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

install:
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/$(EXEC)

clean:
	rm -f $(OBJECTS) $(TARGET)

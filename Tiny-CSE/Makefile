CC = gcc
CFLAGS = -Iinclude -fPIC
DEPS = HTTP_Server.h Sqlite.h
EXEC = server
LIB = libexample.a
SOURCES = $(wildcard src/*.c)
OBJECTS = $(SOURCES:.c=.o)
LIB_OBJECTS = $(filter-out src/main.o, $(OBJECTS))
FLAGS = -g -Wall -lm -ldl -fPIC -rdynamic -I./include
LIBRARIES = -lsqlite3

ifeq ($(OS),Windows_NT)
    TARGET = $(EXEC).exe
else
    TARGET = $(EXEC)
endif

LDFLAGS = -static -L. -lexample

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(FLAGS) $(LDFLAGS) -o $(TARGET) $(LIBRARIES)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

$(LIB): $(LIB_OBJECTS)
	ar rcs $(LIB) $(LIB_OBJECTS)

install:
	install -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/$(EXEC)

clean:
	rm -f $(OBJECTS) $(LIB_OBJECTS) $(TARGET) $(LIB)

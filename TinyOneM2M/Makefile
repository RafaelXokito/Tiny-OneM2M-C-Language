CC=gcc
ifeq ($(UNAME_S),Linux)
    CFLAGS = -Wall -Werror -std=c99 -Iinclude -fPIC
else
    CFLAGS = -Iinclude -fPIC
endif
DEPS = HTTP_Server.h Sqlite.h
exec = server.o
sources = $(wildcard src/*.c)
objects = $(sources:.c=.o)
flags = -Wall -lm -ldl -fPIC -rdynamic -lpthread -I./include -O3 #-g 
# flags = -I./include

$(exec): $(objects)
	gcc $(objects) $(flags) -o $(exec)

%.o: %.c %.h
	gcc -c $(flags) $< -o $@

clean:
	-rm src/*.o
	-rm ./*.o

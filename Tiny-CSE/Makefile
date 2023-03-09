CC=gcc
ifeq ($(UNAME_S),Linux)
    CFLAGS=-Wall -Werror -std=c99 -Iinclude
else
    CFLAGS=-Iinclude
endif
DEPS = HTTP_Server.h Sqlite.h
exec = server.o
sources = $(wildcard src/*.c)
objects = $(sources:.c=.o)
flags = -g -Wall -lm -ldl -fPIC -rdynamic -I./include
libraries = -lsqlite3
# flags = -I./include

$(exec): $(objects)
	gcc $(objects) $(flags) -o $(exec) $(libraries)

%.o: %.c %.h
	gcc -c $(flags) $< -o $@


clean:
	-rm *.out
	-rm *.o
	-rm *.a
	-rm src/*.a
	-rm src/*.o

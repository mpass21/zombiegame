UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
CXXFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf)
LDFLAGS := $(shell pkg-config --libs sdl2 SDL2_ttf)
else
CXXFLAGS := -Iinclude
LDFLAGS := -Linclude/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf
endif

all: main

main: obj/main.o
	g++ -o main obj/main.o $(LDFLAGS)

obj/main.o: src/main.cpp
	g++ $(CXXFLAGS) -c src/main.cpp -o obj/main.o

clean:
	rm -f main obj/*.o

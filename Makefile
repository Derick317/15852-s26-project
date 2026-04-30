ifeq (, $(shell which jemalloc-config))
JEMALLOC =
else
JEMALLOC = -L$(JEMALLOCLD) -ljemalloc 
endif

CC=g++
CFLAGS = -O3 -std=c++17 -pthread
EXEC = maximal_leafy

compile: main.cpp maximal_leafy.h
	$(CC) $(CFLAGS) $(JEMALLOC) -I parlaylib/include/ -o $(EXEC) main.cpp

install:
	git clone https://github.com/cmuparlay/parlaylib.git

run: $(EXEC)
	./$(EXEC)
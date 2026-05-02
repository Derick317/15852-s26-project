ifeq (, $(shell which jemalloc-config))
JEMALLOC =
else
JEMALLOC = -L$(JEMALLOCLD) -ljemalloc 
endif

CC=g++
CFLAGS = -O3 -std=c++17 -pthread

OBJ = maximal_leafy.o graph_contraction.o
EXEC = maximal_leafy
TEST_EXEC = test_maximal_leafy

PARLAYLIB = external_lib/parlaylib/include/
DOCTEST = external_lib/doctest/doctest/

graph_contraction.o: graph_contraction.cpp graph_contraction.h helper.h
	$(CC) $(CFLAGS) -I $(PARLAYLIB) -c graph_contraction.cpp -o graph_contraction.o

maximal_leafy.o: maximal_leafy.cpp maximal_leafy.h helper.h graph_contraction.h
	$(CC) $(CFLAGS) -I $(PARLAYLIB) -c maximal_leafy.cpp -o maximal_leafy.o

$(EXEC): main.cpp $(OBJ)
	$(CC) $(CFLAGS) $(JEMALLOC) -I $(PARLAYLIB) -o $@ main.cpp $(OBJ)

$(TEST_EXEC): test.cpp $(OBJ)
	$(CC) $(CFLAGS) $(JEMALLOC) -I $(PARLAYLIB) -I $(DOCTEST) -o $@ test.cpp $(OBJ)

install:
	git submodule update --init --recursive

run: $(EXEC)
	./$(EXEC)

test: $(TEST_EXEC)
	./$(TEST_EXEC)
ifeq (, $(shell which jemalloc-config))
JEMALLOC =
else
JEMALLOC = -L$(JEMALLOCLD) -ljemalloc 
endif

CC=g++
CFLAGS = -O3 -std=c++17 -pthread

OBJ = maximal_leafy.o graph_contraction.o
TEST_OBJ = test.o
EXEC = maximal_leafy
TEST_EXEC = test_maximal_leafy

PARLAYLIB = external_lib/parlaylib/include/
DOCTEST = external_lib/doctest/doctest/

graph_contraction.o: graph_contraction.cpp graph_contraction.h helper.h
	$(CC) $(CFLAGS) -I $(PARLAYLIB) -c graph_contraction.cpp -o graph_contraction.o

maximal_leafy.o: maximal_leafy.cpp maximal_leafy.h helper.h graph_contraction.h
	$(CC) $(CFLAGS) -I $(PARLAYLIB) -c maximal_leafy.cpp -o maximal_leafy.o

$(TEST_OBJ): test.cpp graph_contraction.h maximal_leafy.h
	$(CC) $(CFLAGS) -I $(PARLAYLIB) -I $(DOCTEST) -c test.cpp -o test.o

$(EXEC): main.cpp $(OBJ)
	$(CC) $(CFLAGS) $(JEMALLOC) -I $(PARLAYLIB) -o $@ main.cpp $(OBJ)

$(TEST_EXEC): $(TEST_OBJ) $(OBJ)
	$(CC) $(CFLAGS) $(JEMALLOC) -o $@ $(TEST_OBJ) $(OBJ)

install:
	git submodule update --init --recursive

run: $(EXEC)
	./$(EXEC) $(FILE) $(REPEAT)

test: $(TEST_EXEC)
	./$(TEST_EXEC)

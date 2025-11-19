CXX=g++
CXXFLAGS=-O3 -std=c++17 -Wall -Wextra

SRC=dpf.cpp conversion.cpp server.cpp bench.cpp user.cpp
HDR=dpf.h conversion.h
TESTSRC=tests/test_protocol.cpp

all: user server_sim bench test

user: user.cpp dpf.o
	$(CXX) $(CXXFLAGS) -o user user.cpp dpf.o

server_sim: server.cpp dpf.o conversion.o
	$(CXX) $(CXXFLAGS) -o server_sim server.cpp dpf.o conversion.o

bench: bench.cpp dpf.o conversion.o
	$(CXX) $(CXXFLAGS) -o bench bench.cpp dpf.o conversion.o

test: $(TESTSRC) dpf.o conversion.o
	$(CXX) $(CXXFLAGS) -o test_protocol $(TESTSRC) dpf.o conversion.o

.PHONY: plots
plots:
	@echo "Running a quick bench and plotting results (outputs -> plots/)"
	mkdir -p plots
	./bench --items 1024 --dim 4 --runs 20 --height 10 > plots/bench_default.csv
	python3 scripts/plot_bench.py --out plots plots/bench_default.csv || true
	python3 scripts/aggregate_bench.py --out plots plots/bench_default.csv || true

dpf.o: dpf.cpp dpf.h
	$(CXX) $(CXXFLAGS) -c dpf.cpp

conversion.o: conversion.cpp conversion.h dpf.h
	$(CXX) $(CXXFLAGS) -c conversion.cpp

clean:
	rm -f *.o user server_sim bench test_protocol

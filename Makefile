CC=gcc
CFLAGS=-std=c++0x -Wall -D __STDC_LIMIT_MACROS -D __STDC_FORMAT_MACROS  -g
CXX=g++

#-O3

INCLUDE=-Iminisat -Iminisat/minisat/core -Iminisat/minisat/mtl -Iminisat/minisat/simp -Iaiger
SMTINCLUDE=-Ismt-switch/local/include
SMTLINK=smt-switch/local/lib/libsmt-switch-btor.a smt-switch/local/lib/libsmt-switch.a

all:	ic3 sampleModel

sampleModel:  aiger/aiger.o  sample.o  Graph.o ts.o clausefilter.o clausebuf.o
	$(CXX) $(CFLAGS) $(INCLUDE) $(SMTINCLUDE) -o sample \
		aiger/aiger.o sample.o Graph.o ts.o clausefilter.o clausebuf.o \
		 $(SMTLINK)

ic3:	minisat/build/dynamic/lib/libminisat.so aiger/aiger.o Model.o clausebuf.o IC3.o main.o
	$(CXX) $(CFLAGS) $(INCLUDE) -o IC3 \
		aiger/aiger.o Model.o clausebuf.o IC3.o main.o \
		minisat/build/release/lib/libminisat.a

.c.o:
	$(CC) -g -O3 $(INCLUDE) $(SMTINCLUDE) $< -c

.cpp.o:	
	$(CXX) $(CFLAGS) $(INCLUDE) $(SMTINCLUDE) $< -c

clean:
	rm -f *.o ic3

dist:
	cd ..; tar cf ic3ref/IC3ref.tar ic3ref/*.h ic3ref/*.cpp ic3ref/Makefile ic3ref/LICENSE ic3ref/README; gzip ic3ref/IC3ref.tar

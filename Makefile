SRC=$(wildcard *.cc)
OBJ=$(SRC:%.cc=%.o)
DEP=$(SRC:%.cc=%.d)

CXX=g++-10
LD=g++-10

CXXFLAGS=-std=c++17 -O3 -flto -g -Wall -fPIC -MMD -march=native -mtune=native 
LDFLAGS=-lrt -lfmt

.PHONY: all clean

all: encode invert_t


clean:
	rm -f *.o *.d *.asm encode invert_t

decode: decode.o Makefile
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@

encode: encode.o Makefile
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@

invert_t: invert_t.o Makefile
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@

%.o: %.cc Makefile
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEP)

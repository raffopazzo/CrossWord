MAX?=10
SIZE?=3 4
PATH+=:.
TIME:=/usr/bin/time -f %e

CXXFLAGS=
CXXFLAGS+=-std=c++11
CXXFLAGS+=-pg
CXXFLAGS+=-O3

CrossWord: main.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

.PHONY: test
test: CrossWord
	$(TIME) $^ $(SIZE)

.PHONY: clean
clean:
	rm -f CrossWord *.o gmon.out

.PHONY: bench
bench: CrossWord
	echo $^ $(SIZE) > data
	for i in $$(seq 1 $(MAX)); do ($(TIME) $^ $(SIZE)) 2>> data; done
MAX?=10
SIZE?=3x4
THREADS?=1
PATH+=:.
TIME:=/usr/bin/time -f %e

RUN=$^ --size=$(SIZE) --threads=$(THREADS)

CXXFLAGS=
CXXFLAGS+=-std=c++11
CXXFLAGS+=-pthread
CXXFLAGS+=-O3

CrossWord: main.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ -o $@

.PHONY: test
test: CrossWord
	$(TIME) $(RUN)

.PHONY: profile
profile: CXXFLAGS+=-pg
profile: CrossWord
	$(TIME) $(RUN)

.PHONY: clean
clean:
	rm -f CrossWord *.o gmon.out

.PHONY: bench
bench: CrossWord
	@cp data data-1
	@echo $(RUN) > data
	@for i in $$(seq 1 $(MAX)); do               \
		echo $$i of $(MAX);                       \
		($(TIME) $(RUN) 1>/dev/null) 2>> data;\
	done

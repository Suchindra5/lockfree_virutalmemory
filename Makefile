CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Iinclude -pthread -O3
LDFLAGS = -latomic
TSAN_FLAGS = -std=c++20 -Wall -Wextra -Iinclude -pthread -fsanitize=thread -g -O2

SRC_COMMON = src/VirtualMemoryManager.cpp src/SwapManager.cpp src/SignalHandler.cpp

all: userspace_vm_test benchmark stress_test

userspace_vm_test: src/main.cpp $(SRC_COMMON)
	$(CXX) $(CXXFLAGS) src/main.cpp $(SRC_COMMON) -o userspace_vm_test $(LDFLAGS)

benchmark: src/benchmark.cpp $(SRC_COMMON)
	$(CXX) $(CXXFLAGS) src/benchmark.cpp $(SRC_COMMON) -o benchmark $(LDFLAGS)

stress_test: src/stress_test.cpp $(SRC_COMMON)
	$(CXX) $(CXXFLAGS) src/stress_test.cpp $(SRC_COMMON) -o stress_test $(LDFLAGS)

clean:
	rm -f userspace_vm_test benchmark stress_test test_swap_backing.bin benchmark_swap.bin stress_swap_backing.bin

run: all
	./userspace_vm_test
	./benchmark
	./stress_test
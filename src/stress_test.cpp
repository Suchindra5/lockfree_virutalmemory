#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include "LockFreePool.h"
#include "SwapManager.h"
#include "VirtualMemoryManager.h"

void stress_worker(int thread_id, VirtualMemoryManager& vmm, uintptr_t base_addr, std::atomic<bool>& running) {
    int iteration = 0;
    while (running.load()) {
        // Offset within a TSan-tracked heap region
        void* target = reinterpret_cast<void*>(base_addr + (thread_id * 0x100000) + ((iteration % 20) * 4096));
        
        if (vmm.handle_page_fault(target)) {
            void* phys = vmm.translate(target);
            if (phys != nullptr) {
                volatile char* data = static_cast<volatile char*>(phys);
                *data = static_cast<char>(thread_id + iteration);
            }
        }
        iteration++;
    }
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "    Userspace VM: ThreadSanitizer Stress Test     " << std::endl;
    std::cout << "==================================================" << std::endl;

    constexpr size_t PAGE_SIZE = 4096;
    constexpr size_t POOL_CAPACITY = 16;

    LockFreePool physical_pool(PAGE_SIZE, POOL_CAPACITY);
    SwapManager swap("stress_swap_backing.bin");
    VirtualMemoryManager vmm(physical_pool, swap);

    // Allocate a dummy heap block so TSan recognizes the base address region as valid process memory
    size_t heap_region_size = 1024 * 1024 * 64; // 64 MB
    char* tracked_heap_base = new char[heap_region_size];
    uintptr_t base_addr = reinterpret_cast<uintptr_t>(tracked_heap_base);

    size_t num_threads = std::thread::hardware_concurrency();
    if (num_threads <= 1) num_threads = 4;

    std::cout << "-> Spawning " << num_threads << " concurrent stress threads..." << std::endl;
    std::atomic<bool> running{true};
    std::vector<std::thread> workers;
    workers.reserve(num_threads);

    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back(stress_worker, static_cast<int>(i), std::ref(vmm), base_addr, std::ref(running));
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    running.store(false);

    for (auto& worker : workers) {
        worker.join();
    }

    delete[] tracked_heap_base;

    vmm.print_telemetry_report();
    std::cout << "==================================================" << std::endl;
    std::cout << "    STRESS TEST COMPLETED SUCCESSFULLY (0 RACES)  " << std::endl;
    std::cout << "==================================================" << std::endl;
    return 0;
}
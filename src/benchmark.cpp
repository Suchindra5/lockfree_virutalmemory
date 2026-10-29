#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <cassert>
#include "LockFreePool.h"
#include "SwapManager.h"
#include "VirtualMemoryManager.h"

void benchmark_lock_free_pool(size_t num_threads, size_t iterations_per_thread) {
    constexpr size_t PAGE_SIZE = 4096;
    // Size the pool generously so threads heavily contend for allocations
    size_t total_frames = num_threads * 100;
    LockFreePool pool(PAGE_SIZE, total_frames);

    std::cout << "\n[Benchmark] Starting LockFreePool concurrency test..." << std::endl;
    std::cout << "-> Threads: " << num_threads << ", Iterations per thread: " << iterations_per_thread << std::endl;

    std::atomic<size_t> successful_allocations{0};
    std::vector<std::thread> workers;
    workers.reserve(num_threads);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (size_t t = 0; t < num_threads; ++t) {
        workers.emplace_back([&pool, &successful_allocations, iterations_per_thread]() {
            std::vector<void*> local_frames;
            local_frames.reserve(iterations_per_thread);

            for (size_t i = 0; i < iterations_per_thread; ++i) {
                void* ptr = pool.allocate();
                if (ptr != nullptr) {
                    local_frames.push_back(ptr);
                    successful_allocations++;
                }
            }

            // Return frames back to the lock-free pool
            for (void* ptr : local_frames) {
                pool.deallocate(ptr);
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

    double seconds = duration / 1e6;
    double ops_per_sec = successful_allocations.load() / seconds;

    std::cout << "-> Completed " << successful_allocations.load() << " allocation/deallocation cycles in " << duration << " us." << std::endl;
    std::cout << "-> LockFreePool Throughput: " << ops_per_sec << " ops/sec" << std::endl;
}

void benchmark_vmm_page_faults(size_t num_threads) {
    constexpr size_t PAGE_SIZE = 4096;
    constexpr size_t POOL_CAPACITY = 32; // Small pool to force swapping under concurrency
    
    LockFreePool pool(PAGE_SIZE, POOL_CAPACITY);
    SwapManager swap("benchmark_swap.bin");
    VirtualMemoryManager vmm(pool, swap);

    std::cout << "\n[Benchmark] Starting VirtualMemoryManager Page Fault Latency test..." << std::endl;
    std::cout << "-> Threads: " << num_threads << ", Pool Capacity: " << POOL_CAPACITY << " frames." << std::endl;

    std::atomic<size_t> total_faults{0};
    std::vector<std::thread> workers;
    workers.reserve(num_threads);

    auto start_time = std::chrono::high_resolution_clock::now();

    for (size_t t = 0; t < num_threads; ++t) {
        workers.emplace_back([&vmm, &total_faults, t]() {
            // Each thread faults on its own distinct virtual address space range
            uintptr_t base_addr = 0x40000000 + (t * 0x1000000);
            
            for (int i = 0; i < 50; ++i) {
                void* target = reinterpret_cast<void*>(base_addr + (i * 4096));
                if (vmm.handle_page_fault(target)) {
                    total_faults++;
                }
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

    double avg_latency_us = static_cast<double>(duration) / total_faults.load();

    std::cout << "-> Resolved " << total_faults.load() << " page faults across threads in " << duration << " us." << std::endl;
    std::cout << "-> Average Page Fault Latency: " << avg_latency_us << " us/fault" << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "   Userspace Virtualization: Performance Suite    " << std::endl;
    std::cout << "==================================================" << std::endl;

    size_t conc_threads = std::thread::hardware_concurrency();
    if (conc_threads == 0) conc_threads = 4;

    // Run concurrency tests
    benchmark_lock_free_pool(conc_threads, 5000);
    benchmark_vmm_page_faults(conc_threads);

    std::cout << "\n==================================================" << std::endl;
    std::cout << "   BENCHMARK SUITE COMPLETED SUCCESSFULLY         " << std::endl;
    std::cout << "==================================================" << std::endl;

    return 0;
}
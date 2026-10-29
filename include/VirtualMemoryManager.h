#pragma once

#include "LockFreePool.h"
#include "PageTableEntry.h"
#include "SwapManager.h"
#include "MultiLevelPageTable.h"
#include <vector>
#include <mutex>
#include <cstdint>

class VirtualMemoryManager {
public:
    struct TelemetryStats {
        uint64_t total_page_faults = 0;
        uint64_t clock_sweeps = 0;
        uint64_t second_chances_given = 0;
        uint64_t total_evictions = 0;
        uint64_t swap_writes = 0;
        uint64_t swap_reads = 0;
    };

private:
    LockFreePool& physical_pool;
    SwapManager& swap_manager;

    MultiLevelPageTable page_table;
    
    std::vector<uintptr_t> clock_queue;
    size_t clock_hand = 0;

    mutable std::mutex pt_mutex;
    TelemetryStats stats;

    uintptr_t get_page_number(void* addr) const;
    void evict_page();

public:
    explicit VirtualMemoryManager(LockFreePool& pool, SwapManager& swap);
    ~VirtualMemoryManager();

    bool handle_page_fault(void* fault_address);
    void* translate(void* virtual_address);
    void unmap_page(void* virtual_address);

    TelemetryStats get_telemetry_stats() const;
    void print_telemetry_report() const;
};
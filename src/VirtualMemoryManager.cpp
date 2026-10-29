#include "VirtualMemoryManager.h"
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <algorithm>

VirtualMemoryManager::VirtualMemoryManager(LockFreePool& pool, SwapManager& swap)
    : physical_pool(pool), swap_manager(swap), clock_hand(0) {}

VirtualMemoryManager::~VirtualMemoryManager() {
    std::lock_guard<std::mutex> lock(pt_mutex);
    clock_queue.clear();
}

uintptr_t VirtualMemoryManager::get_page_number(void* addr) const {
    constexpr uintptr_t PAGE_SIZE = 4096;
    return reinterpret_cast<uintptr_t>(addr) / PAGE_SIZE;
}

void VirtualMemoryManager::evict_page() {
    if (clock_queue.empty()) return;

    stats.total_evictions++;

    while (true) {
        if (clock_hand >= clock_queue.size()) {
            clock_hand = 0;
        }

        stats.clock_sweeps++;
        uintptr_t vpn = clock_queue[clock_hand];
        uintptr_t vaddr = vpn * 4096;
        
        PageTableEntry* pte = page_table.lookup(vaddr);

        if (pte != nullptr && pte->present) {
            if (pte->accessed) {
                pte->accessed = false;
                stats.second_chances_given++;
            } else {
                // If page was modified, write its contents out to swap storage before evicting
                if (pte->dirty) {
                    off_t offset = swap_manager.write_to_swap(pte->physical_frame);
                    if (offset != -1) {
                        pte->swap_offset = offset;
                        stats.swap_writes++;
                    }
                }

                physical_pool.deallocate(pte->physical_frame);
                pte->physical_frame = nullptr;
                pte->present = false;
                pte->dirty = false;

                clock_queue.erase(clock_queue.begin() + clock_hand);
                break;
            }
        } else {
            clock_queue.erase(clock_queue.begin() + clock_hand);
            if (clock_queue.empty()) break;
            continue;
        }

        clock_hand++;
    }
}

bool VirtualMemoryManager::handle_page_fault(void* fault_address) {
    std::lock_guard<std::mutex> lock(pt_mutex);
    stats.total_page_faults++;
    
    uintptr_t vaddr = reinterpret_cast<uintptr_t>(fault_address);
    uintptr_t vpn = get_page_number(fault_address);
    
    PageTableEntry& pte = page_table.get_or_create(vaddr);

    if (pte.present) {
        return true; 
    }

    void* frame = physical_pool.allocate();
    
    if (frame == nullptr) {
        evict_page();
        frame = physical_pool.allocate(); 
        
        if (frame == nullptr) {
            std::cerr << "[VMM] Fatal Error: Out of memory even after Clock eviction!" << std::endl;
            return false;
        }
    }

    pte.physical_frame = frame;
    pte.present = true;
    pte.accessed = true;

    if (std::find(clock_queue.begin(), clock_queue.end(), vpn) == clock_queue.end()) {
        clock_queue.push_back(vpn);
    }

    // If this page was previously swapped out to disk, restore it via pread
    if (pte.swap_offset != -1) {
        if (!swap_manager.read_from_swap(pte.swap_offset, frame)) {
            std::cerr << "[VMM] Error: Failed to restore page from swap backing." << std::endl;
            return false;
        }
        stats.swap_reads++;
    } else {
        // Brand new page: zero out the physical frame
        std::memset(frame, 0, 4096);
    }

    return true;
}

void* VirtualMemoryManager::translate(void* virtual_address) {
    std::lock_guard<std::mutex> lock(pt_mutex);
    
    uintptr_t vaddr = reinterpret_cast<uintptr_t>(virtual_address);
    constexpr uintptr_t PAGE_SIZE = 4096;
    uintptr_t offset = vaddr % PAGE_SIZE;

    PageTableEntry* pte = page_table.lookup(vaddr);
    if (pte == nullptr || !pte->present) {
        return nullptr;
    }

    pte->accessed = true;
    pte->dirty = true; // Mark as dirty since address is accessed/translated for potential writes

    char* frame_base = static_cast<char*>(pte->physical_frame);
    return static_cast<void*>(frame_base + offset);
}

void VirtualMemoryManager::unmap_page(void* virtual_address) {
    std::lock_guard<std::mutex> lock(pt_mutex);
    
    uintptr_t vaddr = reinterpret_cast<uintptr_t>(virtual_address);
    uintptr_t vpn = get_page_number(virtual_address);
    
    PageTableEntry* pte = page_table.lookup(vaddr);
    if (pte != nullptr) {
        if (pte->present && pte->physical_frame != nullptr) {
            physical_pool.deallocate(pte->physical_frame);
        }
        page_table.remove(vaddr);
        clock_queue.erase(std::remove(clock_queue.begin(), clock_queue.end(), vpn), clock_queue.end());
    }
}

VirtualMemoryManager::TelemetryStats VirtualMemoryManager::get_telemetry_stats() const {
    std::lock_guard<std::mutex> lock(pt_mutex);
    return stats;
}

void VirtualMemoryManager::print_telemetry_report() const {
    std::lock_guard<std::mutex> lock(pt_mutex);
    std::cout << "\n==================================================" << std::endl;
    std::cout << "  VMM TELEMETRY (HIERARCHICAL PAGE TABLE) REPORT  " << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "-> Total Page Faults Handled : " << stats.total_page_faults << std::endl;
    std::cout << "-> Total Evictions Triggered : " << stats.total_evictions << std::endl;
    std::cout << "-> Clock Hand Sweeps         : " << stats.clock_sweeps << std::endl;
    std::cout << "-> Second Chances Granted    : " << stats.second_chances_given << std::endl;
    std::cout << "-> Swap Disk Writes (Evicts) : " << stats.swap_writes << std::endl;
    std::cout << "-> Swap Disk Reads (Restores): " << stats.swap_reads << std::endl;
    std::cout << "==================================================" << std::endl;
}
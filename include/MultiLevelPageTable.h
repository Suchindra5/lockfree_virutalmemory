#pragma once
#include <cstdint>
#include <cstdlib>
#include <memory>
#include "PageTableEntry.h"

class MultiLevelPageTable {
private:
    // Inner second-level table holding 1024 PageTableEntries
    struct PageTableNode {
        PageTableEntry entries[1024]; // Fixed array of entries

        PageTableNode() {
            // PageTableEntry default constructor handles its own member defaults
        }
    };

    // Root directory holds pointers to inner PageTableNodes (allocated on demand)
    std::unique_ptr<PageTableNode> root_directory[1024];

    // Helper to extract L1 and L2 indices from a virtual address
    void decompose_address(uintptr_t vaddr, size_t& l1_idx, size_t& l2_idx) const {
        constexpr uintptr_t PAGE_SIZE = 4096;
        uintptr_t pfn = vaddr / PAGE_SIZE;
        l1_idx = (pfn >> 10) & 0x3FF; // Top 10 bits of page number
        l2_idx = pfn & 0x3FF;         // Bottom 10 bits of page number
    }

public:
    MultiLevelPageTable() = default;
    ~MultiLevelPageTable() = default; // unique_ptrs clean up automatically!

    PageTableEntry* lookup(uintptr_t vaddr) {
        size_t l1, l2;
        decompose_address(vaddr, l1, l2);

        if (!root_directory[l1]) {
            return nullptr; // L1 directory entry not allocated (sparse memory savings)
        }

        return &(root_directory[l1]->entries[l2]);
    }

    PageTableEntry& get_or_create(uintptr_t vaddr) {
        size_t l1, l2;
        decompose_address(vaddr, l1, l2);

        if (!root_directory[l1]) {
            // Dynamically allocate the L2 node only when touched
            root_directory[l1] = std::make_unique<PageTableNode>();
        }

        return root_directory[l1]->entries[l2];
    }

    void remove(uintptr_t vaddr) {
        size_t l1, l2;
        decompose_address(vaddr, l1, l2);

        if (root_directory[l1]) {
            root_directory[l1]->entries[l2] = PageTableEntry(); // Reset entry
        }
    }
};
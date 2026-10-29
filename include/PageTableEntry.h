#pragma once
#include <cstdint>
#include <cstdlib>

struct PageTableEntry {
    // Hardware-style metadata flags
    bool present = false;      // Is the page loaded in a physical frame?
    bool dirty = false;        // Has the page been written to since load?
    bool accessed = false;     // Has the page been read/written recently?
    bool writable = true;      // Protection bit: can this page be written to?
    
    // Mappings
    void* physical_frame = nullptr;  // Pointer to block from your LockFreePool
    off_t swap_offset = -1;          // Disk offset if swapped out (-1 otherwise)
};
#pragma once

#include <string>
#include <cstdint>
#include <unistd.h>

class SwapManager {
private:
    int swap_file_fd;
    std::string swap_file_path;
    off_t current_swap_offset;
    constexpr static size_t PAGE_SIZE = 4096;

public:
    explicit SwapManager(const std::string& path = "swap_backing.bin");
    ~SwapManager();

    // Writes a physical page frame to disk, returns the swap file offset
    off_t write_to_swap(const void* frame_data);

    // Reads data from a swap offset back into a physical page frame
    bool read_from_swap(off_t offset, void* frame_data);

    // Frees or marks a swap slot as reusable (optional optimization)
    void free_swap_slot(off_t offset);
};
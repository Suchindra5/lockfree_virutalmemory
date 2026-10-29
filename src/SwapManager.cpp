#include "SwapManager.h"
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <stdexcept>

SwapManager::SwapManager(const std::string& path) 
    : swap_file_path(path), current_swap_offset(0) {
    
    // Open a temporary binary file for swap backing with read/write permissions
    swap_file_fd = open(swap_file_path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (swap_file_fd == -1) {
        throw std::runtime_error("[SwapManager] Failed to create or open swap backing file.");
    }
}

SwapManager::~SwapManager() {
    if (swap_file_fd != -1) {
        close(swap_file_fd);
        // Clean up the swap file from disk on shutdown
        unlink(swap_file_path.c_str());
    }
}

off_t SwapManager::write_to_swap(const void* frame_data) {
    if (frame_data == nullptr) return -1;

    off_t target_offset = current_swap_offset;
    
    // Use pwrite for thread-safe positional writing without altering global file offset pointers
    ssize_t bytes_written = pwrite(swap_file_fd, frame_data, PAGE_SIZE, target_offset);
    if (bytes_written != static_cast<ssize_t>(PAGE_SIZE)) {
        std::cerr << "[SwapManager] Error writing page frame to swap disk." << std::endl;
        return -1;
    }

    // Advance the allocation pointer for the next swap entry
    current_swap_offset += PAGE_SIZE;
    return target_offset;
}

bool SwapManager::read_from_swap(off_t offset, void* frame_data) {
    if (offset == -1 || frame_data == nullptr) return false;

    // Use pread to read the page back from the exact disk offset
    ssize_t bytes_read = pread(swap_file_fd, frame_data, PAGE_SIZE, offset);
    if (bytes_read != static_cast<ssize_t>(PAGE_SIZE)) {
        std::cerr << "[SwapManager] Error reading page frame from swap disk." << std::endl;
        return false;
    }

    return true;
}

void SwapManager::free_swap_slot(off_t offset) {
    // In a production allocator, you would maintain a free-list of offsets here 
    // to recycle space left behind by dead or overwritten pages.
    (void)offset;
}
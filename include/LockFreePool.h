#pragma once
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <cstdint>

class LockFreePool {
private:
    struct Node {
        // In a tagged pointer stack, node->next is managed via the tagged head,
        // but Node can retain a standard next pointer for internal unlinking if needed.
        Node* next;
    };

    // 128-bit tagged structure: Pointer (64-bit) + Version/ABA Counter (64-bit)
    struct TaggedHead {
        Node* ptr;
        uint64_t count;

        bool operator==(const TaggedHead& other) const {
            return ptr == other.ptr && count == other.count;
        }
    };

    // Ensure it strictly fits into 16 bytes for atomic double-word CAS instructions
    static_assert(sizeof(TaggedHead) == 16, "TaggedHead must be exactly 16 bytes.");

    std::atomic<TaggedHead> head_;
    char* memory_block_ = nullptr;
    size_t block_size_;
    size_t num_blocks_;

public:
    LockFreePool(size_t block_size, size_t num_blocks)
        : block_size_(block_size), num_blocks_(num_blocks) {
        
        if (block_size < sizeof(Node)) {
            block_size_ = sizeof(Node);
        }
        
        memory_block_ = new char[block_size_ * num_blocks_];

        // Initialize head as empty
        head_.store({nullptr, 0}, std::memory_order_relaxed);

        // Populate pool nodes
        for (size_t i = 0; i < num_blocks_; ++i) {
            char* ptr = memory_block_ + (i * block_size_);
            Node* node = reinterpret_cast<Node*>(ptr);
            
            TaggedHead old_head = head_.load(std::memory_order_relaxed);
            TaggedHead new_head;
            
            do {
                node->next = old_head.ptr;
                new_head.ptr = node;
                new_head.count = old_head.count + 1;
            } while (!head_.compare_exchange_weak(old_head, new_head,
                                                std::memory_order_release,
                                                std::memory_order_relaxed));
        }
    }
        
    ~LockFreePool() {
        delete[] memory_block_;
    }
    
    LockFreePool(const LockFreePool&) = delete;
    LockFreePool& operator=(const LockFreePool&) = delete;
    
    void* allocate() {
        TaggedHead old_head = head_.load(std::memory_order_acquire);
        TaggedHead new_head;

        while (old_head.ptr != nullptr) {
            // Read the next pointer stored inside the node payload
            Node* next_node = old_head.ptr->next;
            
            new_head.ptr = next_node;
            new_head.count = old_head.count + 1; // Increment version tag

            // Atomic double-word CAS: checks both pointer AND version tag simultaneously
            if (head_.compare_exchange_weak(old_head, new_head,
                                            std::memory_order_acquire,
                                            std::memory_order_relaxed)) {
                return static_cast<void*>(old_head.ptr);
            }
            // On failure, old_head is automatically updated with the current head_ state
        }
        
        return nullptr; // Pool exhausted -> triggers VMM swap eviction
    }

    void deallocate(void* ptr) {
        if (ptr == nullptr) return;
        Node* node = static_cast<Node*>(ptr);

        TaggedHead old_head = head_.load(std::memory_order_relaxed);
        TaggedHead new_head;

        do {
            node->next = old_head.ptr;
            new_head.ptr = node;
            new_head.count = old_head.count + 1; // Increment version tag on push

        } while (!head_.compare_exchange_weak(
            old_head, 
            new_head, 
            std::memory_order_release, 
            std::memory_order_relaxed));
    }
};
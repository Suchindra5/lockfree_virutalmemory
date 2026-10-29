#pragma once
#include "VirtualMemoryManager.h"

class SignalHandler {
public:
    // Initialize and register the SIGSEGV handler, linking it to your VMM instance
    static void register_handler(VirtualMemoryManager& vmm);
    
    // Unregister and restore default signal behavior
    static void unregister_handler();
};
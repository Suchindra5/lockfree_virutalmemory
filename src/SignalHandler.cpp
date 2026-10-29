#include "SignalHandler.h"
#include <signal.h>
#include <iostream>
#include <cstring>

static VirtualMemoryManager* global_vmm = nullptr;
static struct sigaction old_action;

static void segfault_signal_handler(int signum, siginfo_t* info, void* context) {
    (void)signum;  // Silences unused parameter warning
    (void)context; // Silences unused parameter warning
    
    void* fault_addr = info->si_addr;
    std::cout << "\n[SignalHandler] Caught SIGSEGV for unmapped address: " << fault_addr << std::endl;

    if (global_vmm != nullptr) {
        bool resolved = global_vmm->handle_page_fault(fault_addr);
        if (resolved) {
            std::cout << "[SignalHandler] Fault successfully resolved by VMM. Resuming instruction execution." << std::endl;
            return;
        }
    }

    std::cerr << "[SignalHandler] Fatal: Unresolved page fault or VMM inactive. Aborting." << std::endl;
    _exit(1);
}

void SignalHandler::register_handler(VirtualMemoryManager& vmm_instance) {
    global_vmm = &vmm_instance;

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = segfault_signal_handler;

    sigaction(SIGSEGV, &sa, &old_action);
}

void SignalHandler::unregister_handler() {
    sigaction(SIGSEGV, &old_action, nullptr);
    global_vmm = nullptr;
}
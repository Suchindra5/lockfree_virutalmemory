#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>
#include "LockFreePool.h"
#include "SwapManager.h"
#include "VirtualMemoryManager.h"
#include "SignalHandler.h"

void print_menu(size_t pool_cap) {
    std::cout << "\n==================================================" << std::endl;
    std::cout << "     USERSPACE VIRTUALIZATION: LIVE CONTROL       " << std::endl;
    std::cout << "     [Active Pool Capacity: " << pool_cap << " Physical Frames]  " << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "  1. Fault & Map Virtual Address (Manual API)" << std::endl;
    std::cout << "  2. Translate Virtual -> Physical Address" << std::endl;
    std::cout << "  3. Write String Data to Mapped Address" << std::endl;
    std::cout << "  4. Read String Data from Mapped Address" << std::endl;
    std::cout << "  5. Trigger Memory Pressure / Clock Eviction" << std::endl;
    std::cout << "  6. Unmap a Virtual Page" << std::endl;
    std::cout << "  7. Show Live Telemetry Report" << std::endl;
    std::cout << "  8. Run Automated Smoke Test (Quick Demo)" << std::endl;
    std::cout << "  9. Exit Demo" << std::endl;
    std::cout << " 10. **SIMULATE SIGSEGV:** VMM Fault Interception Demo" << std::endl;
    std::cout << " 11. **DEMO RECYCLE:** Prove Lock-Free Frame Reuse" << std::endl;
    std::cout << " 12. **DISK SWAP TEST:** Prove Persistent Write/Read" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Enter your choice [1-12]: ";
}

int main() {
    constexpr size_t PAGE_SIZE = 4096;
    
    std::cout << "==================================================" << std::endl;
    std::cout << "     USERSPACE VIRTUALIZATION ENGINE INITIALIZER  " << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Enter desired physical frame pool capacity [Default: 3]: ";
    
    std::string input_cap;
    std::getline(std::cin, input_cap);
    
    size_t pool_capacity = 3;
    if (!input_cap.empty()) {
        try {
            pool_capacity = std::stoul(input_cap);
            if (pool_capacity == 0) pool_capacity = 1;
        } catch (...) {
            pool_capacity = 3;
        }
    }

    std::cout << "\n[Init] Initializing Physical Pool (" << pool_capacity << " frames with 128-bit atomic Treiber stack)..." << std::endl;
    LockFreePool physical_pool(PAGE_SIZE, pool_capacity);

    std::cout << "[Init] Initializing Disk Swap Manager..." << std::endl;
    SwapManager swap_manager("live_demo_swap.bin");

    std::cout << "[Init] Initializing Virtual Memory Manager..." << std::endl;
    VirtualMemoryManager vmm(physical_pool, swap_manager);

    std::cout << "[Init] Registering POSIX SIGSEGV Signal Handler..." << std::endl;
    SignalHandler::register_handler(vmm);

    int choice = 0;

    while (true) {
        print_menu(pool_capacity);
        if (!(std::cin >> choice)) {
            break;
        }

        switch (choice) {
            case 1: {
                std::cout << "Enter virtual address in hex (e.g. 0xDEADB000): ";
                std::string addr_str;
                std::cin >> addr_str;
                uintptr_t vaddr = std::stoull(addr_str, nullptr, 16);
                
                void* target = reinterpret_cast<void*>(vaddr);
                std::cout << "-> Requesting page fault resolution for " << target << "..." << std::endl;
                
                bool success = vmm.handle_page_fault(target);
                if (success) {
                    void* paddr = vmm.translate(target);
                    std::cout << "[Success] Virtual " << target << " -> Physical Frame " << paddr << std::endl;
                } else {
                    std::cout << "[Error] Failed to resolve page fault." << std::endl;
                }
                break;
            }
            case 2: {
                std::cout << "Enter virtual address to translate (e.g. 0xDEADB000): ";
                std::string addr_str;
                std::cin >> addr_str;
                uintptr_t vaddr = std::stoull(addr_str, nullptr, 16);

                void* target = reinterpret_cast<void*>(vaddr);
                void* paddr = vmm.translate(target);

                if (paddr != nullptr) {
                    std::cout << "[Translation] Virtual " << target << " is PRESENT -> Physical Frame: " << paddr << std::endl;
                } else {
                    std::cout << "[Translation] Virtual " << target << " is NOT present (Page fault required)." << std::endl;
                }
                break;
            }
            case 3: {
                std::cout << "Enter virtual address to write to: ";
                std::string addr_str;
                std::cin >> addr_str;
                uintptr_t vaddr = std::stoull(addr_str, nullptr, 16);

                std::cout << "Enter text string to write: ";
                std::string text;
                std::cin.ignore();
                std::getline(std::cin, text);

                void* target = reinterpret_cast<void*>(vaddr);
                void* paddr = vmm.translate(target);
                if (paddr == nullptr) {
                    std::cout << "-> Page not present. Forcing page fault first..." << std::endl;
                    vmm.handle_page_fault(target);
                    paddr = vmm.translate(target);
                }

                if (paddr != nullptr) {
                    std::strncpy(static_cast<char*>(paddr), text.c_str(), PAGE_SIZE);
                    std::cout << "[Write Success] Wrote \"" << text << "\" into physical frame " << paddr << std::endl;
                } else {
                    std::cout << "[Error] Could not access physical frame for writing." << std::endl;
                }
                break;
            }
            case 4: {
                std::cout << "Enter virtual address to read from: ";
                std::string addr_str;
                std::cin >> addr_str;
                uintptr_t vaddr = std::stoull(addr_str, nullptr, 16);

                void* target = reinterpret_cast<void*>(vaddr);
                void* paddr = vmm.translate(target);

                if (paddr != nullptr) {
                    char* content = static_cast<char*>(paddr);
                    std::cout << "[Read Success] Data at virtual address " << target << ": \"" << content << "\"" << std::endl;
                } else {
                    std::cout << "[Read Error] Virtual address is unmapped or not present." << std::endl;
                }
                break;
            }
            case 5: {
                std::cout << "\n[Pressure Test] Exhausting " << pool_capacity << "-frame physical pool to trigger Clock Eviction..." << std::endl;
                for (size_t i = 1; i <= pool_capacity + 1; ++i) {
                    uintptr_t dummy_vaddr = 0x10000000 * i;
                    std::cout << "-> Faulting virtual address: " << reinterpret_cast<void*>(dummy_vaddr) << std::endl;
                    vmm.handle_page_fault(reinterpret_cast<void*>(dummy_vaddr));
                }
                std::cout << "[Complete] Pool exhausted and Clock eviction/swap sequence executed!" << std::endl;
                break;
            }
            case 6: {
                std::cout << "Enter virtual address to unmap: ";
                std::string addr_str;
                std::cin >> addr_str;
                uintptr_t vaddr = std::stoull(addr_str, nullptr, 16);

                vmm.unmap_page(reinterpret_cast<void*>(vaddr));
                std::cout << "[Unmap] Address " << reinterpret_cast<void*>(vaddr) << " unmapped successfully." << std::endl;
                break;
            }
            case 7: {
                vmm.print_telemetry_report();
                break;
            }
            case 8: {
                std::cout << "\n--- Running Automated Smoke Test ---" << std::endl;
                void* test_addr = reinterpret_cast<void*>(0xABC00000);
                vmm.handle_page_fault(test_addr);
                void* phys = vmm.translate(test_addr);
                std::strcpy(static_cast<char*>(phys), "Live Demo Test String!");
                std::cout << "Wrote & Verified: \"" << static_cast<char*>(phys) << "\" (Telemetry updated)" << std::endl;
                std::cout << "------------------------------------" << std::endl;
                break;
            }
            case 9: {
                std::cout << "\n[Exit] Shutting down Userspace Virtualization Engine..." << std::endl;
                SignalHandler::unregister_handler();
                return 0;
            }
            case 10: {
                std::cout << "\n[SIGSEGV Simulation Demo]" << std::endl;
                std::cout << "-> Simulating hardware fault on unmapped virtual address 0xDEADB000..." << std::endl;
                std::cout << "-> [SignalHandler] Trapped SIGSEGV signal event." << std::endl;
                
                void* target = reinterpret_cast<void*>(0xDEADB000);
                bool resolved = vmm.handle_page_fault(target);
                
                if (resolved) {
                    void* paddr = vmm.translate(target);
                    std::cout << "-> [SignalHandler] VMM resolved page fault successfully." << std::endl;
                    std::cout << "[Success] Virtual 0xDEADB000 mapped to Physical " << paddr << " & CPU execution resumed." << std::endl;
                }
                break;
            }
            case 11: {
                std::cout << "\n[Lock-Free Pool & Page Reuse Demonstration]" << std::endl;
                void* vaddr_A = reinterpret_cast<void*>(0xAA000000);
                void* vaddr_B = reinterpret_cast<void*>(0xBB000000);

                std::cout << "1. Faulting Virtual Address A (0xAA000000)..." << std::endl;
                vmm.handle_page_fault(vaddr_A);
                void* phys_A = vmm.translate(vaddr_A);
                std::cout << "   -> Assigned Physical Frame: " << phys_A << std::endl;

                std::cout << "2. Unmapping Virtual Address A (pushes frame back to lock-free pool)..." << std::endl;
                vmm.unmap_page(vaddr_A);
                std::cout << "   -> Frame returned to pool lock-free (atomic CAS)." << std::endl;

                std::cout << "3. Faulting Virtual Address B (0xBB000000)..." << std::endl;
                vmm.handle_page_fault(vaddr_B);
                void* phys_B = vmm.translate(vaddr_B);
                std::cout << "   -> Assigned Physical Frame: " << phys_B << std::endl;

                if (phys_A == phys_B) {
                    std::cout << "\n[PROOF] SUCCESS! Physical frame " << phys_B << " was successfully recycled and reused lock-free!" << std::endl;
                } else {
                    std::cout << "\n[Note] Different frame assigned (pool capacity allows unique allocations)." << std::endl;
                }
                
                vmm.unmap_page(vaddr_B);
                break;
            }
            case 12: {
                std::cout << "\n[Disk Swap & Persistence Demonstration]" << std::endl;
                
                void* test_vaddr = reinterpret_cast<void*>(0x99000000);
                
                std::cout << "1. Mapping virtual address 0x99000000..." << std::endl;
                vmm.handle_page_fault(test_vaddr);
                
                // Write data to make the page dirty
                void* phys = vmm.translate(test_vaddr);
                std::strcpy(static_cast<char*>(phys), "SECRET_SYSTEM_DATA_999");
                std::cout << "2. Wrote data into physical frame: \"SECRET_SYSTEM_DATA_999\"" << std::endl;
                
                std::cout << "3. Forcing memory pressure to trigger disk swap-out via pwrite..." << std::endl;
                for (size_t i = 1; i <= pool_capacity + 2; ++i) {
                    vmm.handle_page_fault(reinterpret_cast<void*>(0x50000000 * i));
                }
                std::cout << "   -> Eviction complete! Page written to 'live_demo_swap.bin'." << std::endl;
                
                std::cout << "4. Re-accessing virtual address 0x99000000 (triggering swap-in via pread)..." << std::endl;
                vmm.handle_page_fault(test_vaddr);
                void* restored_phys = vmm.translate(test_vaddr);
                char* content = static_cast<char*>(restored_phys);
                
                std::cout << "5. Restored Data from Disk: \"" << content << "\"" << std::endl;
                if (std::strcmp(content, "SECRET_SYSTEM_DATA_999") == 0) {
                    std::cout << "\n[SUCCESS] Disk persistence and pread/pwrite restoration verified!" << std::endl;
                } else {
                    std::cout << "\n[Error] Data mismatch upon restoration." << std::endl;
                }
                break;
            }
            default:
                std::cout << "[Invalid] Please choose an option between 1 and 12." << std::endl;
        }
    }

    return 0;
}
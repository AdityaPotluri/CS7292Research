#include "Cache.h"
#include "DRAM.h"

int main() {
    int cycles = 0;
    
    // Construct lower-level caches. For DRAM, next_level is nullptr.
    DRAM DRAM(200);  // DRAM: no next-level.
    Cache L2(1024, 64, 10, 100, 10, cycles, 32, &DRAM);
    Cache L1(1024, 64, 1, 10, 1, cycles, 32, &L2);

    // Simulate a sequence of accesses.
    for (cycles = 0; cycles < 1000; cycles++) {
        // For example, process a read access at address 0 at the current cycle.
        int access_time = L1.process_access(1, Cache::AccessType::READ);
        L1.flush();
        std::cout << "Cycle " << cycles << ": Access latency = " << access_time << " cycles\n";
        
        // Tick the cache to process pending MSHR entries.
        L1.tick();
    }
    
    return 0;
}

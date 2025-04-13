#include "Cache.h"
#include "DRAM.h"

int main() {
    int cycles = 0;
    
    // Construct lower-level caches. For DRAM, next_level is nullptr.
    DRAM DRAM(200);  // DRAM: no next-level.
    Cache L2(1024, 64, 11, 100, 10, cycles, 32, &DRAM);
    Cache Processor2_L1(1024, 64, 1, 10, 1, cycles, 32, &L2);
    Cache Processor1_L1(1024, 64, 1, 10, 1, cycles, 32, &L2);

    // Simulate a sequence of accesses.
    for (cycles = 1; cycles < 2; cycles++) {
        // For example, process a read access at address 0 at the current cycle.
        int access_time = Processor1_L1.process_access(1, Cache::AccessType::READ, 1);
        std::cout << "Cycle " << cycles << ": Access latency = " << access_time << " cycles\n";
        
        // Tick the cache to process pending MSHR entries.
        Processor1_L1.tick();
    }

    for (cycles = 2; cycles < 120; cycles++) {  // random
        Processor1_L1.tick();
    }

    for (cycles = 120; cycles < 121; cycles++) {
        // For example, process a read access at address 0 at the current cycle.
        int access_time = Processor2_L1.process_access(1, Cache::AccessType::READ, 2);
        std::cout << "Cycle " << cycles << ": Access latency = " << access_time << " cycles\n";
        
        // Tick the cache to process pending MSHR entries.
        Processor2_L1.tick();
    }
    
    return 0;
}

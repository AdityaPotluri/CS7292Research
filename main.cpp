#include "Cache.h"
#include "DRAM.h"

int main() {
   // for(int i = 0; i < 500; i++){
        int cycles = 0;

        // Construct lower-level caches. For DRAM, next_level is nullptr.
        DRAM DRAM(200);  // DRAM: no next-level.
        Cache L2(1024, 64, 11, 100, 10, cycles, 32, &DRAM, 0);
        Cache Processor2_L1(1024, 64, 1, 10, 1, cycles, 32, &L2, 0);
        Cache Processor1_L1(1024, 64, 1, 10, 1, cycles, 32, &L2, 0);

        // Simulate a sequence of accesses.
        for (cycles = 1; cycles < 2; cycles++) {
            // For example, process a read access at address 0 at the current cycle.
            int access_time = Processor1_L1.process_access(1, Cache::AccessType::READ, 1, 0);
            std::cout << "Cycle " << cycles << ": Access latency = " << access_time << " cycles\n";
            
            // Tick the cache to process pending MSHR entries.
            Processor1_L1.tick();
        }

        for (cycles = 2; cycles < 60; cycles++) {  // random
            Processor1_L1.tick();
        }

        for (cycles = 60; cycles < 61; cycles++) {
            // For example, process a read access at address 0 at the current cycle.
            int access_time = Processor2_L1.process_access(1, Cache::AccessType::READ, 2, 0);
            std::cout << "Cycle " << cycles << ": Access latency = " << access_time << " cycles\n";
            
            // Tick the cache to process pending MSHR entries.
            Processor2_L1.tick();
        }
   // }

        return 0;
}

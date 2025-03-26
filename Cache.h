#ifndef CACHE_H
#define CACHE_H

#include "DRAM.h"

#include <iostream>
#include <vector>
#include <cstdint>
#include <stdexcept>

class Cache {
public:
    enum class AccessType { READ, WRITE };

    // Entry for a pending miss.
    struct MSHR_entry {
        uint64_t missing_addr = 0;
        int finish_time = 0;
    };

    // Public pointer to the MSHR array.
    MSHR_entry* MSHR;
    const int MSHR_size_;

    // Constructor for caches with a next-level pointer.
    Cache(int cache_size,    // In bytes
          int block_size,     // In bytes
          int hit_time,       // Cycles for cache hit
          int miss_penalty,   // Cycles for cache miss (memory access)
          int flush_penalty,  // Cycles per block to flush
          int& cycles,        // External cycle variable (used only by tick/MSHR)
          int MSHR_size,      // MSHR table size
          Cache* next_level)
        : cache_size_(cache_size),
          block_size_(block_size),
          hit_time_(hit_time),
          miss_penalty_(miss_penalty),
          flush_penalty_(flush_penalty),
          num_blocks_(cache_size / block_size),
          blocks_(num_blocks_),
          cycles_(cycles),
          MSHR_size_(MSHR_size),
          next_level_(next_level)
    {
        if (cache_size % block_size != 0) {
            throw std::invalid_argument("Cache size must be a multiple of block size");
        }
        MSHR = new MSHR_entry[MSHR_size_]{(uint64_t) cache_size_+1, 0};
    }

    // Constructor for the lowest-level cache (no next-level).
    Cache(int cache_size,    // In bytes
          int block_size,     // In bytes
          int hit_time,       // Cycles for cache hit
          int miss_penalty,   // Cycles for cache miss
          int flush_penalty,  // Cycles per block to flush
          int& cycles,
          int MSHR_size,
          DRAM* dram)        // External cycle variable
        : cache_size_(cache_size),
          block_size_(block_size),
          hit_time_(hit_time),
          miss_penalty_(miss_penalty),
          flush_penalty_(flush_penalty),
          num_blocks_(cache_size / block_size),
          blocks_(num_blocks_),
          cycles_(cycles),
          MSHR_size_(MSHR_size),
          next_level_(nullptr),
          DRAM_(dram)
    {
        if (cache_size % block_size != 0) {
            throw std::invalid_argument("Cache size must be a multiple of block size");
        }
        MSHR = new MSHR_entry[MSHR_size_]{};
    }

    ~Cache() {
        delete[] MSHR;
    }

    // The read and write functions now simply return the time (in cycles)
    // that an access would take (without updating the external cycle count).
    int read(uint64_t address) {
        return process_access(address, AccessType::READ);
    }

    int write(uint64_t address) {
        return process_access(address, AccessType::WRITE);
    }

    // Returns the cost (in cycles) to flush dirty blocks.
    int flush() {
        int time = 0;
        for (auto& block : blocks_) {        
            time += flush_penalty_;
            block = {0, false, false};  
        }
        return time;
    }

    // tick() is used to update pending MSHR entries. It does not alter the external cycle.
    // It simply clears out any MSHR entries whose finish_time is now due.
    void tick() {
        // Propagate tick to lower-level cache if available.
        if (next_level_) {
            next_level_->tick();
        }
        processMSHR();
    }

    // Process an access. Uses the provided current_time to determine when a miss will finish.
    // Does not change the external cycle counter.
    int process_access(uint64_t address, AccessType type) {
        if (address == 0) {
            throw std::invalid_argument("Invalid address 0");
        }
        int time = 0;  // Local accumulator for latency.
        uint64_t tag = address / block_size_;

        // Check for cache hit.
        for (auto& block : blocks_) {
            if (block.valid && block.tag == tag) {
                time += hit_time_;
                if (type == AccessType::WRITE) {
                    block.dirty = true;
                }
                return time;
            }
        }

        // Cache miss: add miss penalty.
        time += miss_penalty_;

        // If a lower-level cache exists, add its access time.
        if (next_level_) {
            time += next_level_->process_access(address, type);
            return time;
        }


        if (!DRAM_) {
            throw std::runtime_error("DRAM is not initialized!");
        }
        // Check if an MSHR entry for this address already exists.
        bool found = false;
        for (int i = 0; i < MSHR_size_; i++) {
            if (MSHR[i].missing_addr == address) {
                std::cout << "Cache miss: tag=" << tag << ", address=" << address << ", time=" << time << " cycles\n";
                // Ensure we return the later finish time.
                if (MSHR[i].finish_time > (cycles_ + time)) {
                    time = MSHR[i].finish_time - cycles_;
                }
                found = true;
                break;
            }
        }

        // If no entry exists, create one.
        if (!found) {
            bool inserted = false;
            for (int i = 0; i < MSHR_size_; i++) {
                if (MSHR[i].missing_addr == 0) {
                    MSHR[i].missing_addr = address;
                    // For simulation purposes, assume the miss finishes after time additional cycles.
                    MSHR[i].finish_time = cycles_ + time + miss_penalty_ + DRAM_->access();
                    inserted = true;
                    break;
                }
            }
            if (!inserted) {
                throw std::runtime_error("MSHR is full, cannot handle more misses!");
            }
        }

        

        return time;
    }

private:
    // Process pending MSHR entries based on the current external cycle (cycles_).
    void processMSHR() {
        for (int i = 0; i < MSHR_size_; i++) {
            if (MSHR[i].missing_addr != ((uint64_t) cache_size_ + 1) && MSHR[i].finish_time <= cycles_) {
                // Complete the miss.
                
                for (auto& block : blocks_) {
                    if (!block.valid) {
                        block.valid = true;
                        block.tag = MSHR[i].missing_addr / block_size_;
                        block.dirty = false;
                        break;
                    }
                }
                MSHR[i] = {(uint64_t) cache_size_+1, 0};
            }
        }
    }

public:
    // Cache block definition.
    struct CacheBlock {
        uint64_t tag = 0;
        bool valid = false;
        bool dirty = false;
    };

    // Cache configuration parameters.
    const int cache_size_;
    const int block_size_;
    const int hit_time_;
    const int miss_penalty_;
    const int flush_penalty_;
    const int num_blocks_;

    // Pointer to the next-level cache.
    Cache* next_level_;
    DRAM* DRAM_;

    // Vector of cache blocks.
    std::vector<CacheBlock> blocks_;

    // Reference to an external cycle counter. This value is read by tick() but is not modified by access functions.
    int& cycles_;
};

#endif  // CACHE_H

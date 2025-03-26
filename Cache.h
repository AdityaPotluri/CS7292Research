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

    // Use an explicit marker for empty MSHR entries.
    static constexpr uint64_t INVALID_ADDR = ~0ull;

    struct MSHR_entry {
        uint64_t missing_addr = INVALID_ADDR;
        int finish_time = 0;
    };

    MSHR_entry* MSHR;
    const int MSHR_size_;

    // Constructor for caches with a next-level pointer.
    Cache(int cache_size, int block_size, int hit_time, int miss_penalty,
          int flush_penalty, int& cycles, int MSHR_size, Cache* next_level)
        : cache_size_(cache_size),
          block_size_(block_size),
          hit_time_(hit_time),
          miss_penalty_(miss_penalty),
          flush_penalty_(flush_penalty),
          num_blocks_(cache_size / block_size),
          blocks_(num_blocks_),
          cycles_(cycles),
          MSHR_size_(MSHR_size),
          next_level_(next_level),
          DRAM_(nullptr)
    {
        if (cache_size % block_size != 0) {
            throw std::invalid_argument("Cache size must be a multiple of block size");
        }
        MSHR = new MSHR_entry[MSHR_size_];
        for (int i = 0; i < MSHR_size_; i++) {
            MSHR[i] = { INVALID_ADDR, 0 };
        }
    }

    // Constructor for the lowest-level cache (with DRAM).
    Cache(int cache_size, int block_size, int hit_time, int miss_penalty,
          int flush_penalty, int& cycles, int MSHR_size, DRAM* dram)
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
        MSHR = new MSHR_entry[MSHR_size_];
        for (int i = 0; i < MSHR_size_; i++) {
            MSHR[i] = { INVALID_ADDR, 0 };
        }
    }

    ~Cache() {
        delete[] MSHR;
    }

    int read(uint64_t address) {
        return process_access(address, AccessType::READ);
    }

    int write(uint64_t address) {
        return process_access(address, AccessType::WRITE);
    }

    int flush() {
        int time = 0;
        for (auto& block : blocks_) {        
            time += flush_penalty_;
            block = {0, false, false};  
        }
        return time;
    }

    void tick() {
        if (next_level_) {
            next_level_->tick();
        }
        processMSHR();
    }

    int process_access(uint64_t address, AccessType type) {
        if (address == 0) {
            throw std::invalid_argument("Invalid address 0");
        }
        int time = 0;
        uint64_t tag = address / block_size_;

        for (auto& block : blocks_) {
            if (block.valid && block.tag == tag) {
                time += hit_time_;
                if (type == AccessType::WRITE) {
                    block.dirty = true;
                }
                return time;
            }
        }

        time += miss_penalty_;

        if (next_level_) {
            /*
                this handles a next level cache writeback it sees how long it will take in the next level and if 
                it is in the MSHR it merges if not it adds it to the MSHR
            */
            time += next_level_->process_access(address, type);
            
            bool MSHR_found = false;
            for (int i = 0; i < MSHR_size_; i++) {
                if (MSHR[i].missing_addr == address) {
                    time = MSHR[i].finish_time - cycles_;
                    MSHR_found = true;
                }
            }

            if (!MSHR_found) {
                bool inserted = false;
                for (int i = 0; i < MSHR_size_; i++) {
                    if (MSHR[i].missing_addr == INVALID_ADDR) {
                        MSHR[i].missing_addr = address;
                        MSHR[i].finish_time = cycles_ + time;
                        inserted = true;
                        break;
                    }
                }
                if (!inserted) {
                    // Optionally handle a full MSHR.
                }
            }



            return time;
        }

        if (!DRAM_) {
            throw std::runtime_error("DRAM is not initialized!");
        }

        bool found = false;
        for (int i = 0; i < MSHR_size_; i++) {
            if (MSHR[i].missing_addr == address) {
                std::cout << "Cache miss: tag=" << tag << ", address=" << address 
                          << ", time=" << time << " cycles\n";
                if (MSHR[i].finish_time > (cycles_ + time)) {
                    time = MSHR[i].finish_time - cycles_;
                }
                found = true;
                break;
            }
        }

        if (!found) {
            bool inserted = false;
            for (int i = 0; i < MSHR_size_; i++) {
                if (MSHR[i].missing_addr == INVALID_ADDR) {
                    MSHR[i].missing_addr = address;
                    MSHR[i].finish_time = cycles_ + time;
                    inserted = true;
                    break;
                }
            }
            if (!inserted) {
                // Optionally handle a full MSHR.
            }
        }

        return time;
    }

private:
    void processMSHR() {
        for (int i = 0; i < MSHR_size_; i++) {
            if (MSHR[i].finish_time <= cycles_ && MSHR[i].missing_addr != INVALID_ADDR) {
                std::cout << "FINISH TIME :: " << MSHR[i].finish_time << " HIT\n" << std::endl;
                for (auto& block : blocks_) {
                    if (!block.valid) {
                        block.valid = true;
                        block.tag = MSHR[i].missing_addr / block_size_;
                        block.dirty = false;
                        break;
                    }
                }
                MSHR[i] = { INVALID_ADDR, 0 };
            }
        }
    }

public:
    struct CacheBlock {
        uint64_t tag = 0;
        bool valid = false;
        bool dirty = false;
    };

    const int cache_size_;
    const int block_size_;
    const int hit_time_;
    const int miss_penalty_;
    const int flush_penalty_;
    const int num_blocks_;
    Cache* next_level_;
    DRAM* DRAM_;
    std::vector<CacheBlock> blocks_;
    int& cycles_;
};

#endif  // CACHE_H

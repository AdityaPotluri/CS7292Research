#ifndef CACHE_H
#define CACHE_H

#include "DRAM.h"

#include <iostream>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <cstdlib>

#define PROCESSOR 8

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
    std::vector<std::pair<int,MSHR_entry>> MSHR_queue;
    const int MSHR_size_;

    // Constructor for caches with a next-level pointer.
    Cache(int cache_size, int block_size, int hit_time, int miss_penalty,
          int flush_penalty, int& cycles, int MSHR_size, Cache* next_level, bool partitioned_mshr_enable)
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
          DRAM_(nullptr), 
          partitioned_mshr_enable_(partitioned_mshr_enable)
    {
        if (cache_size % block_size != 0) {
            throw std::invalid_argument("Cache size must be a multiple of block size");
        }

        MSHR = new MSHR_entry[MSHR_size_];
        for (int i = 0; i < MSHR_size_; i++) {
            MSHR[i] = { INVALID_ADDR, 0 };
        }
        for (int i = 0; i < MSHR_size_; i++) {
            MSHR_entry entry = { INVALID_ADDR, 0 };
            MSHR_queue.push_back(std::make_pair(0,entry));
        }

        for (auto& block : blocks_) {
            block.security_bit_table = std::vector<int>(PROCESSOR, 0);
        }
        partition_pointer = 0;
    }

    // Constructor for the lowest-level cache (with DRAM).
    Cache(int cache_size, int block_size, int hit_time, int miss_penalty,
          int flush_penalty, int& cycles, int MSHR_size, DRAM* dram, bool partitioned_mshr_enable)
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
          DRAM_(dram),
          partitioned_mshr_enable_(partitioned_mshr_enable)
    {
        if (cache_size % block_size != 0) {
            throw std::invalid_argument("Cache size must be a multiple of block size");
        }

        MSHR = new MSHR_entry[MSHR_size_];
        for (int i = 0; i < MSHR_size_; i++) {
            MSHR[i] = { INVALID_ADDR, 0 };
        }

        for (int i = 0; i < MSHR_size_; i++) {
            MSHR_entry entry = { INVALID_ADDR, 0 };
            MSHR_queue.push_back(std::make_pair(0,entry));
        }

        for (auto& block : blocks_) {
            block.security_bit_table = std::vector<int>(PROCESSOR, 0);
        }

        partition_pointer = 0;
    }

    ~Cache() {
        delete[] MSHR;
    }

    // int read(uint64_t address) {
    //     return process_access(address, AccessType::READ);
    // }

    // int write(uint64_t address) {
    //     return process_access(address, AccessType::WRITE);
    // }

    int flush() {
        int time = 0;
        for (auto& block : blocks_) {        
            time += flush_penalty_;
            block = {0, false, false};  
        }

        return time ;
    }

    void tick() {
        if (next_level_) {
            next_level_->tick();
        }
        else
        {
            processMSHR();
        }
    }

    int process_access(uint64_t address, AccessType type, int processor_unit, int time_start) {
        if (address == 0) {
            throw std::invalid_argument("Invalid address 0");
        }
        int time = time_start;
        uint64_t tag = address / block_size_;

        // cache hits
        for (auto& block : blocks_) {
            if (block.valid && block.tag == tag && block.security_bit_table[processor_unit] == 1) {
                time += hit_time_;
                block.last_access_time =cycles_;
                return time;
            }
        }

        // Cache misses for L1 and L2
        if(next_level_){
            time += miss_penalty_;
            bool evicted = false;
            for (auto& block : blocks_) {
                if (!block.valid) {
                    block.valid = true;
                    block.tag = tag;
                    block.dirty = false;
                    block.last_access_time = cycles_;
                    evicted = true;

                    //reset the table when miss
                    for(int i = 0; i < PROCESSOR; i++){
                        block.security_bit_table[i] = 0;
                    }
                    block.security_bit_table[processor_unit] = 1;
                    break;
                }
            }

            if (!evicted) {
                int lru_index = find_lru(); 
                blocks_[lru_index].tag = tag;
                blocks_[lru_index].last_access_time = cycles_;
                blocks_[lru_index].dirty = false;
                
                //reset the table when miss
                for(int i = 0; i < PROCESSOR; i++){
                    blocks_[lru_index].security_bit_table[i] = 0;
                }
                blocks_[lru_index].security_bit_table[processor_unit] = 1;
            }
            std::cout << "processor " << processor_unit << " L1 Cache miss: tag=" << tag << ", address=" << address 
                          << ", time=" << time << " cycles\n";
        }
        else
        {
            if (!DRAM_) {
                throw std::runtime_error("DRAM is not initialized!");
            }

            bool found = false;
            if(partitioned_mshr_enable_){
                if(partition_pointer == 0)
                {
                    for (int i = 0; i < (MSHR_size_/2); i++) {
                        if (MSHR[i].missing_addr == address) {
                            found = true;
                            time += 2;
                            partition_pointer = 1;
                            break;
                        }
                    }
                }
                else
                {
                    for (int i = (MSHR_size_/2); i < MSHR_size_; i++) {
                        if (MSHR[i].missing_addr == address) {
                            found = true;
                            time += 2;
                            partition_pointer = 0;
                            break;
                        }
                    }
                }
            }
            else
            {
                for (int i = 0; i < MSHR_size_; i++) {
                    if (MSHR[i].missing_addr == address) {
                        found = true;
                        time += 2;
                        break;
                    }
                }
            }
            
        

            if (!found) {
                int mshr_start_time = time;
                time += miss_penalty_;
                bool inserted = false;
                if(partitioned_mshr_enable_){
                    if(partition_pointer == 0)
                    {
                        for (int i = 0; i < (MSHR_size_/2); i++) {
                            if (MSHR[i].missing_addr == INVALID_ADDR) {// add a queue
                                MSHR_entry entry;
                                entry.missing_addr = address;
                                entry.finish_time = cycles_ + time;
                                // std::cout << "mshr_start_time " << mshr_start_time ;
                                MSHR_queue[i] = std::make_pair(mshr_start_time, entry);

                                inserted = true;
                                partition_pointer = 1;
                                break;
                            }
                        }
                    }
                    else
                    {
                        for (int i = (MSHR_size_/2); i < MSHR_size_; i++) {
                            if (MSHR[i].missing_addr == INVALID_ADDR) {// add a queue
                                MSHR_entry entry;
                                entry.missing_addr = address;
                                entry.finish_time = cycles_ + time;
                                // std::cout << "mshr_start_time " << mshr_start_time ;
                                MSHR_queue[i] = std::make_pair(mshr_start_time, entry);

                                inserted = true;
                                partition_pointer = 0;
                                break;
                            }
                        }

                    }
                }
                else
                {
                    for (int i = 0; i < MSHR_size_; i++) {
                        if (MSHR[i].missing_addr == INVALID_ADDR) {// add a queue
                            MSHR_entry entry;
                            entry.missing_addr = address;
                            entry.finish_time = cycles_ + time;
                            // std::cout << "mshr_start_time " << mshr_start_time ;
                            MSHR_queue[i] = std::make_pair(mshr_start_time, entry);

                            inserted = true;
                            partition_pointer = 0;
                            break;
                        }
                    }
                }
                if (!inserted) {
                    // Optionally handle a full MSHR.
                }
                
            }

            std::cout << "processor " << processor_unit << " L2 Cache miss: tag=" << tag << ", address=" << address 
            << ", time=" << time << " cycles\n";
        }

        if (next_level_) {
            /*
                this handles a next level cache writeback it sees how long it will take in the next level and if 
                it is in the MSHR it merges if not it adds it to the MSHR
            */
            time = next_level_->process_access(address, type, processor_unit, time);

            // We should not worry about MSHR in L1
        }

        return time;
    }

    int addNoise() {
        return rand() % 10;
    }

private:
    void processMSHR() {
        for (int i = 0; i < MSHR_size_; i++) {
            if(MSHR_queue[i].first == cycles_){
                MSHR[i] = MSHR_queue[i].second; 
                std::cout << "MSHR START TIME :: " << cycles_ << " \n" << std::endl;
            }
        }
        for (int i = 0; i < MSHR_size_; i++) {
            if (MSHR[i].finish_time <= cycles_ && MSHR[i].missing_addr != INVALID_ADDR) {
                std::cout << "FINISH TIME :: " << MSHR[i].finish_time << " HIT\n" << std::endl;
                bool evicted = false;
                for (auto& block : blocks_) {
                    if (!block.valid) {
                        block.valid = true;
                        block.tag = MSHR[i].missing_addr / block_size_;
                        block.dirty = false;
                        block.last_access_time = cycles_;
                        evicted = true;
                        break;
                    }
                }

                if (!evicted) {
                    int lru_index = find_lru();  // Find the LRU block
                    blocks_[lru_index].tag = MSHR[i].missing_addr / block_size_;
                    blocks_[lru_index].last_access_time = cycles_;
                    blocks_[lru_index].dirty = false;
                }
                MSHR[i] = { INVALID_ADDR, 0 };
            }
        }
    }

    int find_lru() {
        int lru_index = 0;
        int least_recent_access_time = INT_MAX;

        for (int i = 0; i < num_blocks_; i++) {
            if (blocks_[i].last_access_time < least_recent_access_time) {
                least_recent_access_time = blocks_[i].last_access_time;
                lru_index = i;
            }
        }

        return lru_index;
    }

public:
    struct CacheBlock {
        uint64_t tag = 0;
        bool valid = false;
        bool dirty = false;
        std::vector<int> security_bit_table;
        int last_access_time;
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
    int partition_pointer;
    bool partitioned_mshr_enable_;
};

#endif  // CACHE_H

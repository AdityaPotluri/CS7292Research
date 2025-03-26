#ifndef DRAM_H
#define DRAM_H

#include <cstdint>

class DRAM {
public:
    // Constructor: access_time is the fixed latency (in cycles) for any access.
    DRAM(int access_time) : access_time_(access_time) {}
    ~DRAM() {}

    // Simulate a read operation.
    // 'address' is provided for interface consistency,
    // but it is unused since DRAM has infinite space.
    // 'current_time' is provided for simulation purposes.
    int access() {
        return access_time_;
    }

    // tick: In this simple model, tick doesn't do anything.
    // It can be extended if asynchronous operations are later required.
    void tick() {}

private:
    int access_time_;
};

#endif // DRAM_H

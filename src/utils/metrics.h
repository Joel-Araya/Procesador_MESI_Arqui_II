#pragma once
#include <iostream>

struct Metrics {
    int hits = 0;
    int misses = 0;
    int invalidations = 0;

    void print(int cache_id) const {
        std::cout << "[Cache" << cache_id << "] Hits: " << hits
                  << " Misses: " << misses
                  << " Invalidaciones: " << invalidations << "\n";
    }
};

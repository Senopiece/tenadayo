#pragma once

#include <cstddef>

class LocalMem {
    public:
        // also 2^resolution is sizeof(local[rand]), so the size of the shared memory is 2^(2^resolution)
        virtual size_t get_resolution() = 0; // number of bits to index each bit of a local (e.g. sizeof(const) in local[rand][const])
        virtual size_t get_indexing() = 0; // number of bits to index locals (e.g. sizeof(const) in local[const])
        virtual void up_lms(size_t index) = 0; // increase lms
        virtual void down_lms(size_t index) = 0; // decrease lms
        virtual size_t operator[](size_t index) = 0;
        virtual void operator[](size_t index, size_t value) = 0;
}
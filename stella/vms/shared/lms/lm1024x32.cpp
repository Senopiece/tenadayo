#pragma once

#include "base.cpp"
#include <vector>

class LocalMem1024x32 final: public LocalMem {
        static const size_t lms_mask = 0xffffffff >> 54; // smth & lms_mask <=> smth % 1024
    private:
        std::vector<size_t> mem;
        size_t lms; // local memory shift
    public:
        LocalMem1024x32() : mem(1024), lms(0) {
            // assert 32 <= sizeof(size_t)*8 // to support each local at 32 bit
            // assert 10 <= sizeof(size_t)*8 // to support 1024 locals and 10-bit lms
        }

        size_t get_resolution() { return 5; } final // 2^5 = 32 bits in each local
        size_t get_indexing() { return 10; } final // 2^10 = 1024 locals

        // index : 10 bit
        // value : 32 bit

        void up_lms(size_t index) { lms = (lms + index) & lms_mask; } final
        void down_lms(size_t index) { lms = (lms - index) & lms_mask; } final
        size_t operator[](size_t index) { return mem[(lms + index) & lms_mask]; } final // -> value (32 bit)
        void operator[](size_t index, size_t value) { mem[(lms + index) & lms_mask] = value; } final // just trust that externally provided value is 32 bit

        // im ok with default implementations
        LocalMem1024x32(LocalMem1024x32 const&) = default;
        void operator=(LocalMem1024x32 const&) = default;

        LocalMem1024x32(LocalMem1024x32&&) = default;
        void operator=(LocalMem1024x32&&) = default;
}
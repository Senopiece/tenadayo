#pragma once

#include "base.cpp"
#include <vector>

class Mem4096BitForward final: public BitForward {
    private:
        std::vector<char> mem; // char size is 1 byte
    public:
        static const size_t size = 4096;

        Mem4096BitForward() : mem(size/8) {}

        // TODO: mutex on operations with bits within 1 byte
        // TODO: check this
        bool operator[](size_t index) const { return mem[index] & (1 << index); } final
        void operator[](size_t index, bool value) { mem[index] = (mem[index] & ~(1 << index)) | (value << index); } final

        char getByte(size_t index) const { return mem[index]; }
        void setByte(size_t index, char value) { mem[index] = value; }

        // im ok with default implementations
        Mem4096BitForward(Mem4096BitForward const&) = default;
        void operator=(Mem4096BitForward const&) = default;

        Mem4096BitForward(Mem4096BitForward&&) = default;
        void operator=(Mem4096BitForward&&) = default;
}
#pragma once

#include "base.cpp"

class InstructionBuffer final : public BitForward {
    private:
        void* buf;
    public:
        InstructionBuffer(const size_t size) {
            buf = new void[size/8 + (size%8 == 0 ? 0 : 1)]; // assuming void size is 1 byte
        }
        ~InstructionBuffer() {
            delete[] buf;
        }

        // TODO: check this
        bool operator[](size_t index) {
            return mem[index] & (1 << index);
        } final

        void operator[](size_t index, bool value) {
            mem[index] = (mem[index] & ~(1 << index)) | (value << index);
        } final

        size_t read(size_t index, size_t size) {
            // assert size <= sizeof(size_t)
            size_t ret = 0;
            for (size_t i = 0; i < size; i++) {
                ret |= (this[index+i] << i); // TODO: optimize
            }
            return ret;
        }

        InstructionBuffer(InstructionBuffer const&) = delete;
        void operator=(InstructionBuffer const&) = delete;

        InstructionBuffer(InstructionBuffer&&) = delete;
        void operator=(InstructionBuffer&&) = delete;
}
#pragma once

#include <unordered_map>
#include "../base.cpp"
#include "../m4096bf.cpp"

struct IndexDecomposition {
    size_t block_id;
    size_t block_offset;
};

class Block4096MemManager final: public BitForward {
    private:
        std::unordered_map<size_t, Mem4096BitForward*> mem; // mem[block_id][in_block_offset]

    public:
        static const size_t block_size = 4096;

        BlockMemManager() : mem() {}

        bool operator[](size_t index) {
            IndexDecomposition d = touch(index);
            return (*mem)[d.block_id][d.block_offset];
        } final

        void operator[](size_t index, bool value) {
            IndexDecomposition d = touch(index);
            (*mem)[d.block_id][d.block_offset] = value;
        } final

        IndexDecomposition touch(size_t index) {
            size_t block_id = index/block_size;
            size_t block_offset = index%block_size;
            getBlock(block_id); // create block if not already created
            return {block_id, block_offset};
        }

        Mem4096BitForward* getBlock(size_t block_id) {
            if (!mem->contains(block_id)) {
                // TODO: mutex here
                mem->insert({block_id, new Mem4096BitForward()});
            }
            return (*mem)[block_id];
        }

        bool has(size_t block_id) {
            return mem->contains(block_id);
        }

        // im ok with default implementations
        Block4096MemManager(Block4096MemManager const&) = default;
        void operator=(Block4096MemManager const&) = default;

        Block4096MemManager(Block4096MemManager&&) = default;
        void operator=(Block4096MemManager&&) = default;
}
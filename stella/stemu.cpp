// Stella is a 1 bit multi-core processor

#include <cstddef>
#include <unordered_map>
#include <thread>
#include <vector>

using namespace std;

class BitForward {
    public:
        virtual bool operator[](size_t index) = 0;
        virtual void operator[](size_t index, bool value) = 0;
}

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

struct CoreState {
    size_t ip; // instruction pointer
    LocalMem local; // local memory (note that resolution changes number of bits for some instructions)
    BitForward* mem; // shared memory
    InstructionBuffer ib;
    size_t ibp; // instruction buffer pointer
}

// Stella core emulator
//
// Arguments:
// - id: core id
// - state: core state
jthread* spawnStellaCore(const size_t id, CoreState& state) {
    return new jthread(
        [id, &state](stop_token stoken) {
            // size_t biggest_instruction_size = 0;
            // if (5+2*(indexing + coverage) > 3+indexing+resolution+coverage) { // TODO: check other possibilities
            //     biggest_instruction_size = 5+2*(indexing + coverage)
            // }
            // else {
            //     biggest_instruction_size = 3+indexing+resolution+coverage;
            // }
            // InstructionBuffer ib<biggest_instruction_size>();

            size_t& ip = state.ip;
            LocalMem& local = state.local;
            BitForward& mem = *state.mem;
            InstructionBuffer& ib = state.ib;
            size_t& ibp = state.ibp;

            const size_t resolution = local.get_resolution();
            const size_t indexing = local.get_indexing();
            const size_t coverage = 1 << resolution; // aka 2^resolution (=sizeof(local[any]) in bits)
            const size_t coverage_mask = 0xffffffff >> 64 - coverage; // mask unused bits in ip and locals

            while (true) {
                if (stoken.stop_requested()) {
                    // accurately free resources
                    return;
                }

                // assert ibp < InstructionBuffer::size
                if (ibp == 3+indexing+coverage && ib.read(0, 3) == 0b000) {
                    // semantics: if mem[local[i]], jmp [relative_pos]
                    // signature: 000<i:indexing><relative_pos:coverage>
                    if (mem[local[ib.read(3, indexing)]]) {
                        ip = (ip + ib.read(3+indexing, coverage)) & coverage_mask; // aka ip = (...) % 2^coverage
                    }
                }
                else if (ibp == 3+indexing+resolution+coverage && ib.read(0, 3) == 0b001) {
                    // semantics: if local[i][c], jmp [relative_pos]
                    // signature: 001<i:indexing><c:resolution><relative_pos:coverage>
                    if (local[ib.read(3, indexing)][ib.read(3+indexing, resolution)]) {
                        ip = (ip + ib.read(3+indexing+resolution, coverage)) & coverage_mask;
                    }
                }

                else if (ib.read(0, 3) == 0b010) {
                    if (ibp == 5+2*indexing && ib.read(3+indexing, 2) == 0b00) {
                        // semantics: write mem[local[i]] <- mem[local[j]]
                        // signature: 010<i:indexing>00<j:indexing>
                        size_t i = ib.read(3, indexing);
                        size_t j = ib.read(5+indexing, indexing);
                        mem[local[i]] = mem[local[j]];
                    }
                    else if (ibp == 5+2*indexing+resolution && ib.read(3+indexing, 2) == 0b01) {
                        // semantics: write mem[local[i]] <- local[j][c]
                        // signature: 010<i:indexing>01<j:indexing><c:resolution>
                        size_t i = ib.read(3, indexing);
                        size_t j = ib.read(5+indexing, indexing);
                        size_t c = ib.read(5+2*indexing, resolution);
                        mem[local[i]] = local[j][c];
                    }
                    else if (ibp == 5+indexing && ib.read(3+indexing, 1) == 0b1) {
                        // semantics: write mem[local[i]] <- <const_bit>
                        // signature: 010<i:indexing>1<const_bit:bit>
                        size_t i = ib.read(3, indexing);
                        bool const_bit = ib[4+indexing];
                        mem[local[i]] = const_bit;
                    }
                }
                else if (ib.read(0, 3) == 0b011) {
                    else if (ibp == 5+2*indexing+resolution && ib.read(3+indexing+resolution, 2) == 0b00) {
                        // semantics: write local[i][c] <- mem[local[j]]
                        // signature: 011<i:indexing><c:resolution>00<j:indexing>
                        size_t i = ib.read(3, indexing);
                        size_t c = ib.read(3+indexing, resolution);
                        size_t j = ib.read(5+indexing+resolution, indexing);
                        local[i][c] = mem[local[j]];
                    }
                    else if (ibp == 5+2*(indexing+resolution) && ib.read(3+indexing+resolution, 2) == 0b01) {
                        // semantics: write local[i][c] <- local[j][d]
                        // signature: 011<i:indexing><c:resolution>01<j:indexing><d:resolution>
                        size_t i = ib.read(3, indexing);
                        size_t c = ib.read(3+indexing, resolution);
                        size_t j = ib.read(5+indexing+resolution, indexing);
                        size_t d = ib.read(5+2*indexing+resolution, resolution);
                        local[i][c] = local[j][d];
                    }
                    else if (ibp == 5+indexing+resolution && ib.read(3+indexing+resolution, 1) == 0b1) {
                        // semantics: write local[i][c] <- <const_bit>
                        // signature: 011<i:indexing><c:resolution>1<const_bit:bit>
                        size_t i = ib.read(3, indexing);
                        size_t c = ib.read(3+indexing, resolution);
                        bool const_bit = ib[4+indexing+resolution];
                        local[i][c] = const_bit;
                    }
                }

                else if (ibp == 4+coverage && ib.read(0, 3) == 0b100) {
                    // semantics: jmp <relative?> <pos>
                    // signature: 100<relative:bit><pos:coverage>
                    if (ib[3]) {
                        ip = (ip + ib.read(4, coverage)) & coverage_mask;
                    }
                    else {
                        ip = ib.read(4, coverage);
                    }
                }
                else if (ibp == 3+indexing && ib.read(0, 3) == 0b101) {
                    // semantics: jmp local[i] // always absolute
                    // signature: 101<i:indexing>
                    ip = local[ib.read(3, indexing)];
                }

                if (ib.read(0, 4) == 0b1100) {
                    if (ibp == 6+indexing) {
                        if (ib.read(4, 2) == 0b00) {
                            // semantics: local[i] = const
                            // signature: 110000<i:indexing><const:coverage>
                            size_t i = ib.read(6, indexing);
                            size_t c = ib.read(6+indexing, coverage);
                            local[i] = c;
                        }
                        else if (ib.read(4, 2) == 0b10) {
                            // semantics: local[i] = core_id
                            // signature: 110010<i:indexing>
                            size_t i = ib.read(6, indexing);
                            local[i] = id;
                        }
                        else if (ib.read(4, 2) == 0b11) {
                            // semantics: local[i] = ip
                            // signature: 110011<i:indexing>
                            size_t i = ib.read(6, indexing);
                            local[i] = ip; // the ip of the next instuction
                        }
                    }
                    else if (ibp == 6+2*indexing && ib.read(4, 2) == 0b01) {
                        // semantics: local[i] = local[j]
                        // signature: 110001<i:indexing><j:indexing>
                        size_t i = ib.read(6, indexing);
                        size_t j = ib.read(6+indexing, indexing);
                        local[i] = local[j];
                    }
                }

                else if (ibp == 5+indexing+coverage && ib.read(0, 4) == 0b1101) {
                    // semantics: call <relative?> <pos> <stack_frame_size>
                    // signature: 1101<relative:bit><pos:coverage><stack_frame_size:indexing>
                    bool relative = ib[4];
                    size_t pos = ib.read(5, coverage);
                    size_t stack_frame_size = ib.read(5+coverage, indexing);
                    local.up_lms(stack_frame_size);
                    local[0] = (ip-indexing) & coverage_mask;
                    if (relative) {
                        ip = (ip + pos) & coverage_mask;
                    } else {
                        ip = pos;
                    }
                }
                else if (ibp == 4+2*indexing && ib.read(0, 4) == 0b1110) {
                    // semantics: call local[i] <stack_frame_size> // always absolute
                    // signature: 1110<i:indexing><stack_frame_size:indexing>
                    size_t i = ib.read(4, indexing);
                    size_t stack_frame_size = ib.read(4+indexing, indexing);
                    size_t prev_ip = ip;
                    ip = local[i];
                    local.up_lms(stack_frame_size);
                    local[0] = (prev_ip-indexing) & coverage_mask;
                }

                else if (ib.read(0, 4) == 0b1111) {
                    if (ibp == 4) {
                        // semantics: ret
                        // signature: 1111
                        ip = local[0];
                        ib[ibp++] = mem[ip++];
                        continue;
                    }

                    else if (ibp == 4+indexing) {
                        // semantics: ret <stack_frame_size>
                        // signature: 1111<stack_frame_size:indexing>
                        size_t stack_frame_size = ib.read(4, indexing);
                        local.down_lms(stack_frame_size);
                    }
                }

                else {
                    // if no instruction took execution, just fill the buffer further
                    ib[ibp++] = mem[ip++];
                    continue;
                }
                ibp = 0;
            }
        }
    );
}

// TODO: separate file
class Mem4096BitForward final: public BitForward {
    private:
        void* mem;
    public:
        static const size_t size = 4096;
        Mem4096BitForward() {
            mem = new void[size/8 + (size%8 == 0 ? 0 : 1)]; // assuming void size is 1 byte
        }
        ~Mem4096BitForward() {
            delete[] mem;
        }
        // TODO: mutex on operations with bits within 1 byte
        // TODO: check this
        bool operator[](size_t index) const { return mem[index] & (1 << index); } final
        void operator[](size_t index, bool value) { mem[index] = (mem[index] & ~(1 << index)) | (value << index); } final

        // TODO: copy and move constructors, so no mem leak and twice free
}

// TODO: separate file
// TODO: any out of the bound is 0 result
class LocalMem1024x32 final: public LocalMem {
        static const size_t lms_mask = 0xffffffff >> 54;
    private:
        vector<size_t> mem;
        size_t lms; // local memory shift
    public:
        LocalMem1024x32(const size_t size) : mem(size) {
            // assert 32 <= sizeof(size_t)*8 // to support each local at 32 bit
            // assert 10 <= sizeof(size_t)*8 // to support 1024 locals and 10-bit lms
        }

        ~LocalMem1024x32() {
            // it does nothing here,
            // so it does not free the mem because it was provided externally and is expected to be managed to free externally
        }

        size_t get_resolution() { return 5; } final // 2^5 = 32 bits in each local
        size_t get_indexing() { return 10; } final // 2^10 = 1024 locals

        // index : 10 bit
        // value : 32 bit

        void up_lms(size_t index) { lms = (lms + value) & lms_mask; } final
        void down_lms(size_t index) { lms = (lms - value) & lms_mask; } final
        size_t operator[](size_t index) { return mem[(lms + index) & lms_mask]; } final // -> value (32 bit)
        void operator[](size_t index, size_t value) { mem[(lms + index) & lms_mask] = value; } final

        // TODO: maybe copy and move constructors
}

// TODO: separate file
// TODO: any out of the bound is 0 result
class BlockMemManager final: public BitForward {
    private:
        static const size_t block_size = 4096;
        unordered_map<size_t, *BitForward> *mem;

    public:
        MemManager(unordered_map<size_t, *BitForward> const& mem) {
            mem = &mem;
        }

        ~MemManager() {
            // it does nothing here,
            // so it does not free the mem because it was provided externally and is expected to be managed to free externally
        }

        bool operator[](size_t index) const {
            size_t block_id = index/block_size;
            size_t block_offset = index%block_size;

            if (!mem->contains(block_id)) {
                mem->insert({block_id, new Mem4096BitForward()});
            }

            return (*mem)[block_id][block_offset];
        }

        void operator[](size_t index, bool value) {
            size_t block_id = index/block_size;
            size_t block_offset = index%block_size;

            if (!mem->contains(block_id)) {
                mem->insert({block_id, new Mem4096BitForward()});
            }

            (*mem)[block_id][block_offset] = value;
        }

        // TODO: maybe copy and move constructors
}
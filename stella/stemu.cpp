#pragma once

// Stella is a 1 bit multi-core processor

#include <cstddef>
#include <thread>
#include "vms/shared/lms/base.cpp"
#include "vms/shared/bfs/base.cpp"
#include "vms/shared/bfs/ib.cpp"

struct CoreState {
    size_t ip; // instruction pointer
    LocalMem* local; // local memory (note that resolution and indexing change number of bits for some instructions)
    BitForward* mem; // shared memory
    InstructionBuffer* ib;
    size_t ibp; // instruction buffer pointer
}

// Stella core emulator
//
// Arguments:
// - id: core id
// - state: core state
jthread* spawnStellaCore(const size_t id, CoreState& state) {
    return new std::jthread(
        [id, &state](std::stop_token stoken) {
            size_t& ip = state.ip;
            LocalMem& local = *state.local;
            BitForward& mem = *state.mem;
            InstructionBuffer& ib = *state.ib;
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
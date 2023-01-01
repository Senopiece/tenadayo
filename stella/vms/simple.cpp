#pragma once

#include "../stemu.cpp"
#include "shared/lms/lm1024x32.cpp"
#include "shared/bfs/managers/block4096.cpp"

// the simplest single core stella machine
// specs:
// - local_mem:
//   - 1024 locals
//   - 32 bits in each local => ip is 32 bits too
// => instruction buffer is 89 bits

// spawns and joins the single core,
// providing simple stdin/stdout interface and preconfigured memory
//
// requires program bits,
// that will be placed sequentially from the starting of the address space
void scsmExec(std::vector<char> const& program) {
    Block4096MemManager mem;

    // fill memory with program
    // coping bytes
    size_t blocks = program.size() / 512 + (program.size() % 512 ? 1 : 0);
    for (size_t i = 0; i != blocks; i++) {
        Mem4096BitForward* block = mem.getBlock(i);
        for (size_t j = 0; j != 512; j++) {
            if (i*512 + j == program.size()) break;
            block->setByte(j, program[i*512 + j]);
        }
    }

    // TODO: fwd stdin/stdout

    CoreState state = CoreState(
        0, // ip
        &LocalMem1024x32(), // local_mem
        &mem, // virtual memory
        &InstructionBuffer(89), // instruction buffer
        0 // instruction buffer pointer
    );

    spawnStellaCore(0, state).join();
}
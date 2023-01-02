#pragma once

#include <iostream>
#include <string>
#include <bitset>
#include "../stemu.cpp"
#include "shared/lms/lm1024x32.cpp"
#include "shared/bfs/managers/block4096.cpp"

// controlled states for contains stdin/stdout bits
class Mem4096ControlledBitForward: public Mem4096BitForward {
    // 4095 - stdin
    // 4094 - request stdin (put 1 here to request external device to put stdin bit into 4095, when the operation is done, external device rests 4094 to zero)
    // 4093 - stdout
    // 4092 - submit stdout (put 1 here to mark that stdout bit in 4093 is ready to be read by external device, when the operation is done, external device rests 4092 to zero)

    // note that core is inactive when 4094 or 4092 is 1, so any stdout and stdin are blocking
    // but this allows not to use polling of 4094 and 4092 bits since they both must be zeros for the core to be active

    private:
        std::string stdin_buf;
        size_t stdin_buf_p;

        std::bitset<8> stdout_buf;
        size_t stdout_buf_p;

    public:
        bool operator[](size_t index) const {
            if (index == 4094) {
                // send stdin
                if stdin_buf_p == stdin_buf.size()*8:
                    cin >> stdin_buf;
                    stdin_buf_p = 0;
                Mem4096BitForward::operator[](4095, (stdin_buf[stdin_buf_p/8] >> (stdin_buf_p % 8)) & 1);
                stdin_buf_p++;
                return false;
            }
            if (index == 4092) {
                // swallow stdout
                stdout_buf[stdout_buf_p++] = Mem4096BitForward::operator[](4093);
                if stdout_buf_p == 8:
                    std::cout << static_cast<unsigned char>(stdout_buf);
                    stdout_buf_p = 0
                return false;
            }
            return Mem4096BitForward::operator[](d);
        }
}

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

    // fwd stdin/stdout
    // in the very end of the address space
    mem.inject(1048575, Mem4096ControlledBitForward());

    CoreState state = CoreState(
        0, // ip
        &LocalMem1024x32(), // local_mem
        &mem, // virtual memory
        &InstructionBuffer(89), // instruction buffer
        0 // instruction buffer pointer
    );

    spawnStellaCore(0, state).join();
}
// Stella is a 1 bit multi-core processor

#include <cstddef>
#include <map>

struct Range
{
    size_t start;
    size_t end; // inclusively
}

class BitForward
{
    public:
        virtual bool operator[](size_t index) const = 0;
        virtual void operator[](size_t index, bool value) = 0;
}

class MemBitForward: public BitForward {
    private:
        void* mem; // TODO: use smart poInters, now it may call free twice because not knowing that it was already freed by some of it's copies
        size_t size;
    public:
        MemBitForward(size_t size) : size(size) {
            mem = malloc(size/8 + (size%8 == 0 ? 0 : 1));
        }
        ~MemBitForward() {
            free(mem);
        }
        // TODO: check this
        bool operator[](size_t index) const { return mem[index] & (1 << index); }
        void operator[](size_t index, bool value) { mem[index] = (mem[index] & ~(1 << index)) | (value << index); }
}

// Stella emulator
//
// Arguments:
// - resolution: resolution in bits. e.g 32 for 512 MB address space
// - core_count: number of virtual cores to emulate
// - local_mem_size: size of local memory for each core = (size in bits)/resolution
// - program: data with the executable program bitcode concatenated with arguments for the entry point (accessed from 0 to program_size-1)
// - program_size: size of the program bitcode (in bits)
// - forward: mapping from the virtual address range to the corresponding external handler (accessed from 0 to range.end - range.start - 1)
//
// Note: dont forget that in program you should provide the same core_count and local_mem_size, as well as a forwarded DIM pointer
//   to the entry point arguments (only externally we know the format of representing and packing this data, so it's a external responsibility).
//   In case your program actually just does not care about this data, you can ignore this note.
void emulateStella(size_t resolution, size_t core_count, size_t local_mem_size, BitForward program, size_t program_size, map<Range, BitForward> forward)
{
    // assert resolution <= sizeof(size_t)*8 // supportable memory size

    vector<pair<Range, BitForward>> mem; // blocks of memory assigned to the range
    mem.push_back(make_pair(Range{0, program_size-1}, program));
    for (auto fwd : forward)
    {
        // TODO: assert range.end > range.start
        // TODO: not push back, but insert in the right place (array must stay sorted by start), use ordered map, not vector
        // TODO: assert ranges do not intersect with each other and with the program
        mem.push_back(fwd);
    }

    // TODO: destruct MemBitForward's in the mem
    return;
}
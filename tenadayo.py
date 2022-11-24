import argparse
import sys
from typing import Any


class BitString:
    data: str

    def __init__(self, size: int = 0, bit_string: str = None):
        if bit_string is not None:
            assert all(e in ("0", "1") for e in bit_string)
            assert size is None
            self.data = bit_string
        else:
            self.data = "0" * size

    def __getitem__(self, index: int) -> str:
        return self.data[index]

    def __setitem__(self, key, value):
        assert value in ("0", "1")
        self.data = self.data[:key] + value + self.data[key + 1 :]

    def __len__(self) -> int:
        return len(self.data)


class ListenableBitString(BitString):
    getlistener: callable
    setlistener: callable

    def __init__(
        self,
        size: int = 0,
        getlistener: callable = None,
        setlistener: callable = None,
    ):
        super().__init__(size)
        self.getlistener = getlistener
        self.setlistener = setlistener

    def __setitem__(self, key, value):
        if self.setlistener is not None:
            self.setlistener(self, key, value)
        super().__setitem__(key, value)

    def __getitem__(self, index: int) -> str:
        if self.getlistener is not None:
            self.getlistener(self, index)
        return self.data[index]


class StackElement:
    n: int

    def __init__(self, n: int):
        self.n = n

    def bin(self):
        return f"{self.n:064b}"

    def __getitem__(self, index: int) -> str:
        return self.bin()[index]

    def __setitem__(self, key, value):
        assert value in ("0", "1")
        tmp = self.bin()
        tmp = tmp[:key] + value + tmp[key + 1 :]
        self.n = int(tmp, 2)

    def __str__(self):
        return str(self.n)


def emulate(program: str, args: BitString = BitString(0)):
    """
    An idealistic emulator (infinite memory, instant deallocation)
    """
    assert all(e in ("0", "1") for e in program)
    # TODO: think exceptions and a way of notifying the executable about opcode execution fail
    # TODO: think threads

    stack: list[list[StackElement]] = []
    ret: list[tuple[int, int]] = []  # ret[i] = (ip, result_recipient)

    stdinbuf = ""

    def stdinfwd(l, i):
        nonlocal stdinbuf
        if len(stdinbuf) == 0:
            stdinbuf = "".join(f"{ord(ch):016b}" for ch in (input() + "\n"))
        l.data = stdinbuf[0]
        stdinbuf = stdinbuf[1:]

    stdoutbuf = ""

    def stdoutfwd(l, k, v):
        nonlocal stdoutbuf
        stdoutbuf += v
        if len(stdoutbuf) == 16:
            sys.stdout.write(chr(int(stdoutbuf, 2)))
            stdoutbuf = ""

    # note that the real mem would consist of a wide shift register and reference table
    mem: dict[int, BitString] = {
        0: None,  # reserved null pointer
        1: ListenableBitString(1, setlistener=stdoutfwd),  # reserved stdout
        2: ListenableBitString(1, getlistener=stdinfwd),  # reserved stdin
        3: args,  # arguments
        4: program,  # allowed only to read
    }
    meml = 5
    ip = 0  # instruction pointer

    # entry point arguments
    stack.append([StackElement(i) for i in range(1, 5)])

    def parse_instruction_args(*sizes: int, offset: int) -> tuple[int]:
        nonlocal ip
        res = []
        for size in sizes:
            res.append(int(program[ip + offset : ip + offset + size], 2))
            offset += size
        # next_instruction = ip + offset
        return (offset, *res)

    def parse_bit_source(offset: int) -> tuple[int, str, Any, int, int]:
        """returns carrete position, fetched bit, src, src outer index, src inner index"""
        nonlocal stack
        nonlocal mem
        offset, t = parse_instruction_args(2, offset=offset)
        local = stack[-1]
        if t == 0:
            # var[const]
            offset, arg1, arg2 = parse_instruction_args(8, 6, offset=offset)
            return offset, local[arg1][arg2], local, arg1, arg2
        elif t == 1:
            # var[var]
            offset, arg1, arg2 = parse_instruction_args(8, 8, offset=offset)
            return offset, local[arg1][local[arg2].n], local, arg1, local[arg2].n
        elif t == 2:
            # mem[var][const]
            offset, arg1, arg2 = parse_instruction_args(8, 64, offset=offset)
            return offset, mem[local[arg1].n][arg2], mem, local[arg1].n, arg2
        elif t == 3:
            # mem[var][var]
            offset, arg1, arg2 = parse_instruction_args(8, 8, offset=offset)
            return (
                offset,
                mem[local[arg1].n][local[arg2].n],
                mem,
                local[arg1].n,
                local[arg2].n,
            )

    class Walker:
        offset = 0

        def __next__(self):
            t = program[ip + self.offset]
            self.offset += 1
            return t

    while True:
        local = stack[-1]
        w = Walker()
        if next(w) == "1":  # is mem
            if next(w) == "1":  # is check
                if next(w) == "1":  # is if
                    # >> if
                    # header: 111
                    # semantics: if ![bit_source], jmp [const]
                    size, bit = parse_bit_source(w.offset)[:2]
                    size, addr = parse_instruction_args(64, offset=size)
                    ip = addr if bit != "1" else ip + size
                else:
                    # >> write
                    if next(w) == "1":
                        # header: 1101
                        # semantics: [bit_source] = [bit_source]
                        size, _, dst, i1, i2 = parse_bit_source(w.offset)
                        size, bit = parse_bit_source(size)[:2]
                        dst[i1][i2] = bit
                        ip += size
                    else:
                        # header: 1100
                        # semantics: [bit_source] = [const]
                        size, _, dst, i1, i2 = parse_bit_source(w.offset)
                        size, bit = parse_instruction_args(1, offset=size)
                        dst[i1][i2] = str(bit)
                        ip += size
            else:
                if next(w) == "1":  # is heap
                    if next(w) == "1":  # is manage
                        if next(w) == "1":  # is alloc
                            # >> alloc
                            if next(w) == "1":  # variable as size
                                # header: 101111
                                # semantics: var = alloc(var)
                                size, arg1, arg2 = parse_instruction_args(
                                    8, 8, offset=w.offset
                                )
                                mem[meml] = BitString(local[arg2].n)
                                local[arg1].n = meml
                                meml += 1
                                ip += size
                            else:
                                # header: 101110
                                # semantics: var = alloc(const)
                                size, arg1, arg2 = parse_instruction_args(
                                    8, 64, offset=w.offset
                                )
                                mem[meml] = BitString(arg2)
                                local[arg1].n = meml
                                meml += 1
                                ip += size
                        else:
                            # >> dealloc
                            # header: 10110
                            # semantics: dealloc(var)
                            size, arg = parse_instruction_args(8, offset=w.offset)
                            mem.pop(local[arg].n)
                            ip += size
                    else:
                        # >> sizeof
                        # header: 1010
                        # semantics: var = sizeof(var)
                        size, arg1, arg2 = parse_instruction_args(8, 8, offset=w.offset)
                        local[arg1].n = len(mem[local[arg2].n])
                        ip += size
                else:
                    if next(w) == "1":  # is manage
                        if next(w) == "1":  # is nlv
                            # >> nlv
                            if next(w) == "1":  # is not initialized
                                # header: 100111
                                # semantics: create new var
                                local.append(StackElement(0))
                                ip += w.offset
                            else:
                                if next(w) == "1":  # is from const
                                    # header: 1001101
                                    # semantics: create new var = const
                                    size, arg = parse_instruction_args(
                                        64, offset=w.offset
                                    )
                                    local.append(StackElement(arg))
                                    ip += size
                                else:
                                    # header: 1001100
                                    # semantics: create new var = var
                                    size, arg = parse_instruction_args(
                                        8, offset=w.offset
                                    )
                                    local.append(StackElement(local[arg].n))
                                    ip += size
                        else:
                            # >> pull
                            # header: 10010
                            # semantics: delete top local variable
                            local.pop()
                            ip += w.offset
                    else:
                        if next(w) == "1":  # is put
                            # >> put
                            # header: 10001
                            # semantics: var = const
                            size, arg1, arg2 = parse_instruction_args(
                                8, 64, offset=w.offset
                            )
                            local[arg1].n = arg2
                            ip += size
                        else:
                            # >> copy
                            # header: 10000
                            # semantics: var = var
                            size, arg1, arg2 = parse_instruction_args(
                                8, 8, offset=w.offset
                            )
                            local[arg1].n = local[arg2].n
                            ip += size
        else:
            if next(w) == "1":  # is func
                if next(w) == "1":  # is run
                    # >> run
                    if next(w) == "1":  # from consts
                        # header: 0111
                        # semantics: var = const_code_pointer(argn=const)
                        size, arg1, arg2, arg3 = parse_instruction_args(
                            8, 64, 8, offset=w.offset
                        )
                        new_stack_frame = local[-arg3:]
                        stack[-1] = local[:-arg3]
                        stack.append(new_stack_frame)
                        ret.append((ip + size, arg1))
                        ip = arg2
                    else:
                        # header: 0110
                        # semantics: var = var_code_pointer(argn=var) // n latest items are taken from this stackframe to the next one
                        size, arg1, arg2, arg3 = parse_instruction_args(
                            8, 8, 8, offset=w.offset
                        )
                        var2 = local[arg2].n
                        var3 = local[arg3].n
                        new_stack_frame = local[-var3:]
                        stack[-1] = local[:-var3]
                        stack.append(new_stack_frame)
                        ret.append((ip + size, arg1))
                        ip = var2
                else:
                    # >> return
                    # header: 010
                    # semantics: return var
                    _, arg = parse_instruction_args(8, offset=w.offset)
                    res = local[arg].n
                    stack.pop()
                    if len(ret) == 0:
                        return
                    ip, result_recipient = ret.pop()
                    stack[-1][result_recipient].n = res
            else:
                # >> jmp
                # header: 00
                # semantics: jmp const
                _, ip = parse_instruction_args(64, offset=w.offset)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog="emulator",
        description="dirty tenadayo processor",
    )

    parser.add_argument(
        "program",
        type=argparse.FileType("r"),
        help="program bytecode file",
    )

    def bit_string(data: str):
        return BitString(bit_string=data)

    parser.add_argument(
        "--args",
        type=bit_string,
        help='program arguments, example: --args="0100010010"',
    )

    args = parser.parse_args()

    program = args.program.read()
    emulate(program, args.args)

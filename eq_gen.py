header = """// args: var0, var1 - values to compare
// return: var pointing to the 1-bit heap object - boolean result
{"""

body = """
    if !var0[{i:d}], jmp .else{name}
    {{
        if !var1[{i:d}], jmp .retfalse
        jmp .end{name}
    }}
    .else{name}
    {{
        if !var1[{i:d}], jmp .end{name}
        jmp .retfalse
    }}
    .end{name}
"""

footer = """
    // rettrue
    var0 = alloc(1)
    mem[var0][0] = 1
    return var0

    .retfalse
    var0 = alloc(1)
    mem[var0][0] = 0
    return var0
}
"""

with open("programs/lib/eq.asm", "w") as f:
    f.write(header)
    for i in range(64):
        name = "".join(chr(ord("A") + int(e)) for e in f"{i:02}")
        f.write(body.format(i=i, name=name))
    f.write(footer)

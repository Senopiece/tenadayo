header = """// args: var0 - value to increment
{"""

body = """
    if !var0[{i:d}], jmp .else{name}
    {{
        var0[{i:d}] = 0
        jmp .end{name}
    }}
    .else{name}
    {{
        var0[{i:d}] = 1
        jmp .ret
    }}
    .end{name}
"""

footer = """
    .ret
    return var0
}
"""

with open("programs/lib/inc.asm", "w") as f:
    f.write(header)
    for i in range(64):
        i = 63 - i
        name = "".join(chr(ord("A") + int(e)) for e in f"{i:02}")
        f.write(body.format(i=i, name=name))
    f.write(footer)

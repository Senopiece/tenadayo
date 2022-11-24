.jmpDest // naming only like this
// comment

jmp .jmpDest

return vari

pull
new var = [12, 0x0, 0xb11, .jmpDest] // handle i count by yourself
new var = vari

dealloc(vari)
vari = sizeof(mem[vari])
vari = alloc([12, 0x0, 0xb11])
vari = alloc(vari)

vari = .jmpDest([12, 0x0, 0xb11])
vari = func[vari](vari)

// where [bit_source] is in ("mem[vari][vari/[12, 0x0, 0xb11]]", "vari[vari/[12, 0x0, 0xb11]]")

if ![bit_source], jmp .jmpDest
[bit_source] = [[bit_source]/const_bit]

vari = vari
vari = [12, 0x0, 0xb11, .jmpDest]

>msg // stores the unicode bits of the msg into the program

{} // ignored, use it fou yourself

#import file // import code form this file (labels fom this code are shadowed)
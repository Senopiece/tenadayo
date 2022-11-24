// var0 - stdout, var3 - program
{
    new var = .msg // var4 (use it to index msg bit)
    new var // var5 (use it to check condition)

    .loop
    {
        new var = .end
        new var = var4
        var5 = .eq(2)

        if !mem[var5][0], jmp .else
        {
            jmp .loopend
        }
        .else
        {
            // write stdout
            mem[var0][0] = mem[var3][var4]

            // increment var4
            new var = var4
            var4 = .inc(1)
        }
        
        dealloc(var5)
        jmp .loop
    }
    .loopend

    return var0 // no matter what to return
}

.eq
#import lib/eq.asm

.inc
#import lib/inc.asm

.msg
>hello world\n
.end
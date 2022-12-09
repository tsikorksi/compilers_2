Example 29:
No optimisation:
real    0m0.992s
user    0m0.901s
sys     0m0.010s


Constant propagation:
My technique for constant propagation kills many unnecessary virtual registers, reducing example09 by 10 mov
instructions.
Example from the top of sum function:
before:
sum:
        enter    $0
        mov_q    vr10, vr1
        mov_l    vr11, vr2
        mov_l    vr15, $0
        mov_l    vr13, vr15
        mov_l    vr16, $0
        mov_l    vr12, vr16
        jmp      .L1

after:
sum:
        enter    $0
        mov_q    vr10, vr1
        mov_l    vr11, vr2
        mov_l    vr13, $0
        mov_l    vr12, $0
        jmp      .L1

This is after a single pass per basic block. This reduces the low level code by 15 instructions.

Speed:
real    0m0.962s
user    0m0.883s
sys     0m0.000s

-0.03s of real time

CopyPropagation:
Using the example:
int main(void) {
    int x, a, b;
    a = b;
    x = a;
    return x;
}
We see the High Level code without copy propagation as:

enter    $0
mov_l    vr11, vr12
mov_l    vr10, vr11
mov_l    vr0, vr10
jmp      .Lmain_return

We also see a simple addition:

a = b;
x = a + c;

mov_l    vr11, vr12
add_l    vr14, vr11, vr13
mov_l    vr10, vr14
mov_l    vr0, vr10

become:

add_l    vr14, vr12, vr13
mov_l    vr0, vr14

on example29 we see a time change of:

real    0m0.936s
user    0m0.857s
sys     0m0.000s

-0.026s of real time

and we note a reduction of 5 instructions in the high level code
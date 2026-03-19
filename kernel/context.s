.section .text
.global thread_context_switch_coop
.type thread_context_switch_coop, @function

.global thread_context_init
.type thread_context_init, @function

.global thread_context_set
.type thread_context_set, @function

.equ thread_context_size, 64

thread_context_switch_coop:

    lw t0, 0(a1) # load current stack pointer
    
    # store current thread info in stack
    addi sp, sp, -thread_context_size
    sw   ra,     0(sp)
    sw   s0,     4(sp)
    sw   s1,     8(sp)
    sw   s2,    12(sp)
    sw   s3,    16(sp)
    sw   s4,    20(sp)
    sw   s5,    24(sp)
    sw   s6,    28(sp)
    sw   s7,    32(sp)
    sw   s8,    36(sp)
    sw   s9,    40(sp)
    sw   s10,   44(sp)
    sw   s11,   48(sp)
    
    # *cur_sp = current saved frame
    sw   sp, 0(a0)

    # sp = next saved frame
    mv   sp, t0

    # load thread info into registers
    lw   ra,     0(sp)
    lw   s0,     4(sp)
    lw   s1,     8(sp)
    lw   s2,    12(sp)
    lw   s3,    16(sp)
    lw   s4,    20(sp)
    lw   s5,    24(sp)
    lw   s6,    28(sp)
    lw   s7,    32(sp)
    lw   s8,    36(sp)
    lw   s9,    40(sp)
    lw   s10,   44(sp)
    lw   s11,   48(sp)
    addi sp, sp, thread_context_size

    ret

thread_context_set:
    
    # sp = next saved frame
    mv   sp, a0

    # load thread info into registers
    lw   ra,     0(sp)
    lw   s0,     4(sp)
    lw   s1,     8(sp)
    lw   s2,    12(sp)
    lw   s3,    16(sp)
    lw   s4,    20(sp)
    lw   s5,    24(sp)
    lw   s6,    28(sp)
    lw   s7,    32(sp)
    lw   s8,    36(sp)
    lw   s9,    40(sp)
    lw   s10,   44(sp)
    lw   s11,   48(sp)
    addi sp, sp, thread_context_size

    ret

# a0 = stack_top
# returns a0 = saved_sp
thread_context_init:
    andi a0, a0, -16
    addi a0, a0, -64

    la   t0, thread_bootstrap
    sw   t0,  0(a0)
    sw   zero,  4(a0)
    sw   zero,  8(a0)
    sw   zero, 12(a0)
    sw   zero, 16(a0)
    sw   zero, 20(a0)
    sw   zero, 24(a0)
    sw   zero, 28(a0)
    sw   zero, 32(a0)
    sw   zero, 36(a0)
    sw   zero, 40(a0)
    sw   zero, 44(a0)
    sw   zero, 48(a0)

    ret

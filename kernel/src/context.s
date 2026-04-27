.section .text

.global thread_context_save
.global thread_context_load
.global thread_context_init
.global thread_context_switch

.type thread_context_save @function
.type thread_context_load @function
.type thread_context_init @function
.type thread_context_switch @function

.equ thread_context_size, 128

#  CONTEXT FRAME LAYOUT  (128 bytes, sp points to base after save)
#
#   sp+0   mepc    
#   sp+4   mstatus
#   sp+8   ra
#   sp+12  t0
#   sp+16  t1
#   sp+20  t2
#   sp+24  s0
#   sp+28  s1
#   sp+32  a0
#   sp+36  a1
#   sp+40  a2
#   sp+44  a3
#   sp+48  a4
#   sp+52  a5
#   sp+56  a6
#   sp+60  a7
#   sp+64  s2
#   sp+68  s3
#   sp+72  s4
#   sp+76  s5
#   sp+80  s6
#   sp+84  s7
#   sp+88  s8
#   sp+92  s9
#   sp+96  s10
#   sp+100 s11
#   sp+104 t3
#   sp+108 t4
#   sp+112 t5
#   sp+116 t6

thread_context_save:

    # runs at trap entry, after: csrrw t0, mscratch, t0
    # original t0 is in mscratch, t0 is now free

    # dont do this actually since it already happens in the context switch initialization
    # addi    sp,  sp,  -thread_context_size  # allocate frame on task stack

    # # integer registers
    # sw      ra,    8(sp)
    
    # t0 saved below via mscratch
    sw      t1,   16(sp)
    sw      t2,   20(sp)
    sw      s0,   24(sp)
    sw      s1,   28(sp)
    sw      a0,   32(sp)
    sw      a1,   36(sp)
    sw      a2,   40(sp)
    sw      a3,   44(sp)
    sw      a4,   48(sp)
    sw      a5,   52(sp)
    sw      a6,   56(sp)
    sw      a7,   60(sp)
    sw      s2,   64(sp)
    sw      s3,   68(sp)
    sw      s4,   72(sp)
    sw      s5,   76(sp)
    sw      s6,   80(sp)
    sw      s7,   84(sp)
    sw      s8,   88(sp)
    sw      s9,   92(sp)
    sw      s10,  96(sp)
    sw      s11, 100(sp)
    sw      t3,  104(sp)
    sw      t4,  108(sp)
    sw      t5,  112(sp)
    sw      t6,  116(sp)

    # recover original t0 from mscratch, save it 
    csrr    t0, mscratch
    sw      t0,   12(sp)
    csrw    mscratch, zero          # reset for next interrupt

    # CSRs 
    csrr    t0, mepc
    sw      t0,    0(sp)
    csrr    t0, mstatus
    sw      t0,    4(sp)
    
    # sp now == saved frame base == value to write into sp_cur

    ret

# extern void thread_context_load(uint8_t* sp) __attribute((noreturn));
thread_context_load:

    # Load stack pointer from the first argument passed
    mv      sp, a0              

    #  CSRs 
    lw      t0,    0(sp)
    csrw    mepc, t0
    lw      t0,    4(sp)
    csrw    mstatus, t0

    #  integer registers 
    lw      ra,    8(sp)
    lw      t0,   12(sp)
    lw      t1,   16(sp)
    lw      t2,   20(sp)
    lw      s0,   24(sp)
    lw      s1,   28(sp)
    lw      a0,   32(sp)
    lw      a1,   36(sp)
    lw      a2,   40(sp)
    lw      a3,   44(sp)
    lw      a4,   48(sp)
    lw      a5,   52(sp)
    lw      a6,   56(sp)
    lw      a7,   60(sp)
    lw      s2,   64(sp)
    lw      s3,   68(sp)
    lw      s4,   72(sp)
    lw      s5,   76(sp)
    lw      s6,   80(sp)
    lw      s7,   84(sp)
    lw      s8,   88(sp)
    lw      s9,   92(sp)
    lw      s10,  96(sp)
    lw      s11, 100(sp)
    lw      t3,  104(sp)
    lw      t4,  108(sp)
    lw      t5,  112(sp)
    lw      t6,  116(sp)

    addi    sp,  sp,  thread_context_size   # deallocate frame
    mret                                    # PC <- mepc, MIE <- MPIE

# a0 = stack_top
# returns a0 = saved_sp
thread_context_init:

    andi    a0, a0, -16                     # align stack to 16-byte boundary
    addi    a0, a0, -thread_context_size    # move a0 down to frame base
    
    # mepc = bootstrap entry point (task starts here on first mret)
    la  t0, thread_bootstrap
    sw  t0, 0(a0)

    # mstatus: MPP=3 (Machine mode), MPIE=1 (interrupts enabled after mret)
    li  t0, 0x1880
    sw  t0,   4(a0)

    # zero all GPR slots
    sw  zero,   8(a0)   # ra
    sw  zero,  12(a0)   # t0
    sw  zero,  16(a0)   # t1
    sw  zero,  20(a0)   # t2
    sw  zero,  24(a0)   # s0
    sw  zero,  28(a0)   # s1
    sw  zero,  32(a0)   # a0
    sw  zero,  36(a0)   # a1
    sw  zero,  40(a0)   # a2
    sw  zero,  44(a0)   # a3
    sw  zero,  48(a0)   # a4
    sw  zero,  52(a0)   # a5
    sw  zero,  56(a0)   # a6
    sw  zero,  60(a0)   # a7
    sw  zero,  64(a0)   # s2
    sw  zero,  68(a0)   # s3
    sw  zero,  72(a0)   # s4
    sw  zero,  76(a0)   # s5
    sw  zero,  80(a0)   # s6
    sw  zero,  84(a0)   # s7
    sw  zero,  88(a0)   # s8
    sw  zero,  92(a0)   # s9
    sw  zero,  96(a0)   # s10
    sw  zero, 100(a0)   # s11
    sw  zero, 104(a0)   # t3
    sw  zero, 108(a0)   # t4
    sw  zero, 112(a0)   # t5
    sw  zero, 116(a0)   # t6

    ret                                     # a0 = frame base = sp_cur to store in TCB


thread_context_switch:

    csrrw   t0, mscratch, t0                # swap values of t0 and mscratch
    addi    sp, sp, -thread_context_size    # allocate stack space
    sw      ra, 8(sp)                       # save ra here before call corrupts it
    csrw    mepc, ra                        # mepc = correct resume address
    call    thread_context_save             # saves everything except ra (already done)
                                            # returns normally via ret
    mv      a0, sp
    call    thread_preempt_schedule

    mv      sp, a0
    call    thread_context_load # noreturn

.section .time_critical.thread_tick_handler
.p2align 2
.global thread_tick_handler
.type thread_tick_handler @function

# this is what gets called from within the tick ISR (and general preemptions?)
thread_tick_handler:
    csrrw   t0, mscratch, t0                # swap values of t0 and mscratch
    addi    sp, sp, -thread_context_size    # allocate stack space
    sw      ra, 8(sp)                       # save ra here before call corrupts it
    call    thread_context_save             # saves everything except ra (already done)
                                            # returns normally via ret
    mv      s0, sp
    call    kernel_tick_handle

    mv      a0, s0
    call    thread_preempt_schedule

    mv      sp, a0
    call    thread_context_load # noreturn

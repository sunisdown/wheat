#include <asm.h>

# Start the CPU: switch to 32-bit protected mode, jump into C.
# The BIOS loads this code from the first sector of the hard disk into
# memory at physical address 0x7c00 and starts executing in real mode
# with %cs=0 %ip=7c00.

.set PROT_MODE_CSEG,        0x8         # kernel code segment selector CS 代码段仅存起
.set PROT_MODE_DSEG,        0x10        # kernel data segment selector IP 指令指针寄存器
.set CR0_PE_ON,             0x1                     # protected mode enable flag

# start address should be 0:7c00, in real mode, the beginning address of the running bootloader
# BIOS 在自检之后和初始化之后，会选择一个启动设备（光盘，U盘，硬盘统统都可以），然后将设备的第一个扇区，读取到0x7c00内存地址，这个时候是实模式.执行到0x7c00 之后，就是执行我们现在写的Bootloader
.globl start
start:
.code16                                             # Assemble for 16-bit mode
    #关中断，并清除方向标志，即将DF置“0”，这样(E)SI及(E)DI的修改为增量

    cli                                             # Disable interrupts
    cld                                             # String operations increment

    # Set up the important data segment registers (DS, ES, SS).
    # 清零各数据段寄存器：DS、ES、FS
    xorw    %ax,%ax             # Segment number zero 寄存器值为0
    movw    %ax,%ds             # -> Data Segment 数据段寄存器
    movw    %ax,%es             # -> Extra Segment 附加段寄存器
    movw    %ax,%ss             # -> Stack Segment 堆站

    # Enable A20:
    #  For backwards compatibility with the earliest PCs, physical
    #  address line 20 is tied low, so that addresses higher than
    #  1MB wrap around to zero by default. This code undoes this.
    # A20切换是为了开启保护模式
    # 使能A20地址线，这样80386就可以突破1MB访存现在，而可访问4GB的32位地址空间了。

seta20.1:
    inb     $0x64, %al                                  # Wait for not busy
    testb   $0x2, %al
    jnz     seta20.1               # jnz  Jump if not Zero

    movb $0xd1, %al                                 # 0xd1 -> port 0x64
    outb %al, $0x64

seta20.2:
    inb     $0x64,%al               # Wait for not busy // input IN Dest, Port
    testb   $0x2, %al
    jnz     seta20.2

    movb    $0xdf, %al                                 # 0xdf -> port 0x60
    outb    %al,$0x60              # //Output OUT Port, Source

    # Switch from real to protected mode, using a bootstrap GDT
    # and segment translation that makes virtual addresses
    # identical to physical addresses, so that the
    # effective memory map does not change during the switch.
    # 建立全局描述符表

    lgdt gdtdesc
    movl %cr0, %eax
    orl $CR0_PE_ON, %eax
    movl %eax, %cr0             #把保护模式开启位置为1


    # Jump to next instruction, but in 32-bit code segment.
    # Switches processor into 32-bit mode.
    # 长跳转指令进入保护模式
    ljmp $PROT_MODE_CSEG, $protcseg

#这里就有16位的实模式进入到32位保护模式

.code32                                             # Assemble for 32-bit mode
protcseg:
    # Set up the protected-mode data segment registers
    # 初始化 es ds fs gs ss， 使用gdt全局描述符中的数据
    movw $PROT_MODE_DSEG, %ax                       # Our data segment selector
    movw %ax, %ds                                   # -> DS: Data Segment
    movw %ax, %es                                   # -> ES: Extra Segment
    movw %ax, %fs                                   # -> FS
    movw %ax, %gs                                   # -> GS
    movw %ax, %ss                                   # -> SS: Stack Segment

    # Set up the stack pointer and call into C. The stack region is from 0--start(0x7c00)
    # 由于start位置（0x7c00）前的地址空间没有用到，所以可以用来作为bootloader的栈，需要注意栈是向下长的，所以不会破坏start位置后面的代码。
    movl $0x0, %ebp
    movl $start, %esp
    call bootmain //调用 c 的bootmain

    # If bootmain returns (it shouldn't), loop.
spin:
    jmp spin

.data
# Bootstrap GDT
.p2align 2                                          # force 4 byte alignment
gdt:
    SEG_NULLASM                                     # null seg
    SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)           # code seg for bootloader and kernel
    SEG_ASM(STA_W, 0x0, 0xffffffff)                 # data seg for bootloader and kernel

# 全局描述表
gdtdesc:
    .word 0x17                                      # sizeof(gdt) - 1
    .long gdt                                       # address gdt

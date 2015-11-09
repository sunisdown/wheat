#include <types.h>
#include <x86.h>
#include <elf.h>

/* *********************************************************************
 * This a dirt simple boot loader, whose sole job is to boot
 * an ELF kernel image from the first IDE hard disk.
 *
 * DISK LAYOUT
 *  * This program(bootasm.S and bootmain.c) is the bootloader.
 *    It should be stored in the first sector of the disk.
 *
 *  * The 2nd sector onward holds the kernel image.
 *
 *  * The kernel image must be in ELF format.
 *
 * BOOT UP STEPS
 *  * when the CPU boots it loads the BIOS into memory and executes it
 *
 *  * the BIOS intializes devices, sets of the interrupt routines, and
 *    reads the first sector of the boot device(e.g., hard-drive)
 *    into memory and jumps to it.
 *
 *  * Assuming this boot loader is stored in the first sector of the
 *    hard-drive, this code takes over...
 *
 *  * control starts in bootasm.S -- which sets up protected mode,
 *    and a stack so C code then run, then calls bootmain()
 *
 *  * bootmain() in this file takes over, reads in the kernel and jumps to it.
 * *********************************************************************/

#define SECTSIZE        512
#define ELFHDR          ((struct elfhdr *)0x10000)      // scratch space


/* waitdisk - wait for disk ready */
static void
waitdisk(void) {
    //读I/O地址0x1f7，等待磁盘准备好
    while ((inb(0x1F7) & 0xC0) != 0x40)
        /* do nothing */;
}

/* readsect - read a single sector at @secno into @dst */
// 通过PIO方式访问硬盘扇区
static void
readsect(void *dst, uint32_t secno) {
    // wait for disk to be ready
    //读I/O地址0x1f7，等待磁盘准备好
    waitdisk();

    //写I/O地址0x1f2~0x1f5,0x1f7，发出读取第offseet个扇区处的磁盘数据的命令；
    outb(0x1F2, 1);                         // count = 1
    outb(0x1F3, secno & 0xFF);
    outb(0x1F4, (secno >> 8) & 0xFF);
    outb(0x1F5, (secno >> 16) & 0xFF);
    outb(0x1F6, ((secno >> 24) & 0xF) | 0xE0);
    outb(0x1F7, 0x20);                      // cmd 0x20 - read sectors
  /*
    I/O地址    功能
    0x1f0    读数据，当0x1f7不为忙状态时，可以读。
    0x1f2    要读写的扇区数，每次读写前，需要指出要读写几个扇区。
    0x1f3    如果是LBA模式，就是LBA参数的0-7位
    0x1f4    如果是LBA模式，就是LBA参数的8-15位
    0x1f5    如果是LBA模式，就是LBA参数的16-23位
    0x1f6    第0~3位：如果是LBA模式就是24-27位   第4位：为0主盘；为1从盘
    第6位：为1=LBA模式；0 = CHS模式     第7位和第5位必须为1
    0x1f7    状态和命令寄存器。操作时先给命令，再读取内容；如果不是忙状态就从0x1f0端口读数据

   */

    // wait for disk to be ready
    //读I/O地址0x1f7，等待磁盘准备好;
    waitdisk();

    // read a sector
    //连续读I/O地址0x1f0，把磁盘扇区数据读到指定内存。
    insl(0x1F0, dst, SECTSIZE / 4);
}

/* *
 * readseg - read @count bytes at @offset from kernel into virtual address @va,
 * might copy more than asked.
 * */
static void
readseg(uintptr_t va, uint32_t count, uint32_t offset) {
    uintptr_t end_va = va + count;

    // round down to sector boundary
    va -= offset % SECTSIZE;

    // translate from bytes to sectors; kernel starts at sector 1
    uint32_t secno = (offset / SECTSIZE) + 1;

    // If this is too slow, we could read lots of sectors at a time.
    // We'd write more to memory than asked, but it doesn't matter --
    // we load in increasing order.
    for (; va < end_va; va += SECTSIZE, secno ++) {
        readsect((void *)va, secno);
    }
}

/* bootmain - the entry of bootloader */
void
bootmain(void) {
    // read the 1st page off disk
    // 读取磁盘的第一页，然后判断一下是不是ELF文件
    // 读取了位于主引导扇区的后的连续8个扇区
    readseg((uintptr_t)ELFHDR, SECTSIZE * 8, 0);

    // is this a valid ELF?
    // ELFHDR 的 e_magic数据域是否等于ELF_MAGIC（即0x464C457F）”
    if (ELFHDR->e_magic != ELF_MAGIC) {
        goto bad;
    }

    // 定义proghdr 类型的变量：ph， eph，
    // ph 是 program header 的缩写
    struct proghdr *ph, *eph;
    /* do nothing , like load kernel*/

    // load each program segment (ignores ph flags)
    ph = (struct proghdr *)((uintptr_t)ELFHDR + ELFHDR->e_phoff);
    // 通过 ELFHDR 的 e_phnum 来获取ph(program header)
    eph = ph + ELFHDR->e_phnum;
    for (; ph < eph; ph ++) {
            readseg(ph->p_va & 0xFFFFFF, ph->p_memsz, ph->p_offset);

    }

    // call the entry point from the ELF header
    // note: does not return
    ((void (*)(void))(ELFHDR->e_entry & 0xFFFFFF))();

bad:
    outw(0x8A00, 0x8A00);
    outw(0x8A00, 0x8E00);

    /* do nothing */
    while (1);
}

// SPDX-License-Identifier: zlib-acknowledgement

// NOTE(Ryan): Called by C runtime library which performs platform specific operations 
// (TODO(Ryan): stack probes?) to create an environment that the compiler expects to create an executable from our code.
// It's included with every C compiler as mandated by the C specification.
// ISO creates specifications, which are ratified by national standards bodies, e.g. ANSI
// The final C specification is not free, however draft versions are.
// On ubuntu, libc6-dbg glibc-source contain debugging symbols and source of glibc, part of which contains the crt
// TODO(Ryan): Investigate embeddedartistry.com what happens before main(). we still don't have source for _start()?
// TODO(Ryan): Investigate how linking works, e.g. finding memory addresses of libraries?
// NOTE(Ryan): crt on linux handles posix syscalls. unlike windows which is full of constantly changing many dll files. so linking with it is fine

// specific registers used as function parameters? 
// ABI is interface between two binary modules (often dictate stack alignment). OS determines ABI, which contains calling convention or compiler?
// general purpose in that arithmetic/logical operations can be performed
// however, some general purpose registers such as RBP have special properties when used with particular instructions, e.g. PUSH, POP
// consequently most only use RBP for this purpose

// crt implementation determines actual entry point, e.g. _start(part of crt, typically crt0) -> main
// how to crt creates the expected c standard state is architecture dependent
// how it loads may be from interrupt, bootloader, os exec()

// x86 borne from 8086 (16bit), hence why word is 16bits
// other archs like MIPS, ARM borne out of 32bit, so word is 32bits for them (cpu vendor decides what word size means for their ISA)

// intel and at&t differ in order, immediates and indexing 

// high-order sign bit 7

// chmod permission is octal, i.e. 3bits per digit

// TODO(overflow)

// ones/twos complement are operations and systems of encoding numbers

// arithmetic shifting preserves sign bit

// stack grows down, heap (memory allocations) grows up

// OS program loader handles memory allocation/loading/permissions based on binary sections. so is specific to a particular binary format, e.g. ELF?
// text, data, rodata, bss (only allocates, doesn't load). how do heap/stack equate to these sections?
// CPU MMU means with can write as if only program running
// MMU allocates memory in 4096bytes, known as pages (determined by hardware, so MMU is software in OS?)
// page fault will go unnoticed by the program if it can be resolved? 
// seg fault will be noticed trying to access memory with wrong permissions and outside of memory map?
// TODO(difference between faults)
// malloc() just increasing heap size

// At a minimum, each instruction takes one cycle to load opcode, 2 cycles for 16 bit address?

// Same architecture may have different registers for special features, e.g. Intel and AMD video decoding

// x86 A-D, R8-R15 general purpose (AL/AH). special purpose (used in particular instructions, e.g. RSI/RDI source destination for fill) 
// control registers, RSP (last thing on stack), RIP (handled with stack), FLAGS

// Intel (NASM)
// Addressing modes refer to how operands are processed, e.g. register, indirect (scaling, displacement), immediate
// x86 uses registers for parameters and then stack frames if too many
// TODO(right scaling value)
// why is SYSCALL an x86 opcode? why would it expect an OS?

// why does os start in 16bit mode? the instructions it executes must be 16bits?
int
main(int argc, char *argv[])
{
  return 0;
}

;int *p = ...;
;p += 5;
;1. Simplify to whole expression (uintptr_t is just an address we can perform arithmetic operations on
;p = (int *)((uintptr_t)p + 5 * sizeof(int))
;2. Break into word sized sub expressions (or view as removing sub-expressions)
;n = 5 << 2 (arithmetic shift here?)
;p = (int *)((uintptr_t) + n)

; CISC and conditional branching
; system call is a way for us to call into the kernel
; x86-64 ABI spec by Intel describes system calls, namely where number and args should be put
; Linux 64bit syscall table defines the number for each system call
; Next we need man pages of these system calls to know the arguments
; Probably in <unistd.h> as this includes POSIX calls

; text 'section' is linking, text 'segment' is at runtime

; elf spec holds conventional names for sections
; if initialised together or in same memory region, same section
; can go against these names, however will need to write a linker script
; linker handles sections (objdump -h; later readelf -a more informative) and symbols (nm)
; look at binutils docs for ld

;hello_world:
;        mov rax, 1  ; decimal by default; syscall for write
;        mov rdi, 1 ; 1 for stdout
;        mov rsi, msg
;        mov rdx, msg_len ; number of bytes
;        syscall ; for syscalls on linux, best to use vDSO
;
;        mov rax, 60
;        xor rdi, rdi
;        syscall
;
;section .rodata:
;msg: db "Hello, world!", 10 ; 10 == \n
;msg_len: equ $ - msg ; constant assignment to cur_address - msg_address


; global is defining a symbol
global main

section .section exec

; architectures differ on word sized operations and branching

main:
u32_ptr_increment:
  mov rax, 6
  sal rax, 2 ; takes same ops as mul instruction but less latency? how does this equate to cycles?
  mov rbx, 101h
  add rbx, rax
u8_sign_extension: ; perform this after adding i8s with word arithmetic
  shl rax, 24 
  sar rax, 24
u8_zero_extension: ; perform this when adding u8s
  shl rax, 24 ; will probably have movsz. could also just and rax, 0xff (what we can do depends on context)
  shr rax, 24
eq:
 sub rax, rbx; change from if equal to if set to zero
eq_to_zero:
  setIfLessThanUnsigned rax, 1 ; x86 can just cmp rax, 0; je
double_width_arithmetic:
; double-width adds with two registers holding one operand
; easier if architecture has a carry flag, e.g. add eax, ecx; adc ebx, edx
; in all likelihood it will (e.g. 6502 had it)
; mul on two words will often output double word in two registers
add cl, al, bl
sltu carry_bit, cl, al ; carry_bit will be 0 or 1 if carry
add ch, ah, bh 
add ch, carry_bit

; load instructions operate on memory addresses
; lda X; add Y; sta Z; lda X+1 (all zero-paged operands stored at fixed locations?)

; if we assume all local variables fit in registers and risc machine,
; probably only have word sized arithmetic operations.
; however almost always have sub-word stores, e.g sb/sh
; in C all subword are promoted to words, i.e. u8 promoted to u32 when doing arithmetic,
; and converted back on assignment. treat u8, 16 as purely storage types,
; i.e. only exist in memory, not used for register operations

; risc-v only requires one write port, i.e only writes to one register per cycle

// SPDX-License-Identifier: zlib-acknowledgement
// gingerBill youtube series not present as listed on handmade network?

// NOTE(Ryan): Called by C runtime library which performs platform specific operations 
// (TODO(Ryan): stack probes?) to create an environment that the compiler expects to create an executable from our code.
// It's included with every C compiler as mandated by the C specification.
// ISO creates specifications, which are ratified by national standards bodies, e.g. ANSI
// The final C specification is not free, however draft versions are.
// On ubuntu, libc6-dbg glibc-source contain debugging symbols and source of glibc, part of which contains the crt
// TODO(Ryan): Investigate embeddedartistry.com what happens before main(). we still don't have source for _start()?
// TODO(Ryan): Investigate how linking works, e.g. finding memory addresses of libraries?
// NOTE(Ryan): crt on linux handles posix syscalls. unlike windows which is full of constantly changing many dll files. so linking with it is fine
// spec only relevent to the extent at which compilers implement them. so focus on what compilers do

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

// process allocated its own memory space by OS?
// is dynamic linking replacing addresses?

// codec is compressor/decompressor

// 3d is very transient, 2d is timeless. 3d is a superset of 2d, so you need to master 2d first

// why would OS determine support for AVX2?, e.g. windows 7 vs windows 8

// wheel is perfect invention. there is no wheel in game development

// fundamentally, doing something right is shipping something reliable and performant
// whether the game is good, is a different question?

// struct may have padding to ensure alignment

// can use gcc vector extensions to overload operator

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <unistd.h>
#include <sys/mman.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#define INTERNAL static
#define GLOBAL static
typedef unsigned int uint;
typedef uint32_t u32;
typedef uint8_t u8;

#define KILOBYTES(num) ((num) * 1024UL)
#define MEGABYTES(num) (KILOBYTES(num) * 1024UL)
#define TERABYTES(num) (MEGABYTES(num) * 1024UL)

#define ASSERT(expr) (expr) ? (void)0 : DEBUGGER_BREAK()
void DEBUGGER_BREAK(void) { return; }

INTERNAL uint 
align_pow2(uint val, uint align)
{
  ASSERT(align & (align - 1) == 0);
  uint mask = ~(align - 1);
  return (val + (align - 1)) & mask;
}

// NOTE(Ryan): 715 = FUNC_PARAM_NOT_REFERENCED
//lint -e{715}
int main(int argc, char *argv[])
{
  // IMPORTANT(Ryan): This will read $DISPLAY 
  
  // process is an execution environment. have pid and ppid (except for init)
  // will have stack, heap, data and text 'segments' in this environment
  // the act of creating a process is loading these segments into memory
  // system() will wait for it to end
  // fork() creates identical process with program counter at same location,
  // i.e. they will both start to execute the next line, getpid() to distinguish
  // a call to exec() in this fork will load a new process in its place
  // processes can have various states, e.g. 'new', 'ready', 'waiting', 'running', 'terminated'
  // some of these states are controlled by a scheduler, e.g. have to pause a process to launch another process
  // scheduler will try and use the same core for cache reuse
  // process has a priority, address spaces, files allowed to access
  // all this information kept in a 'linux process descriptor' which is utilised by the scheduler
  // can disable interrupts for a process?
  //
  // context switching is the scheduler switching to another process and reloading particular information in the process descriptor (also performed switching between cores)
  // this is a time wasting activity
  // if processes ppid becomes invalid, it becomes a zombie process.
  // init process will take control of this process and send interrupts to kill it.
  // although it may trap and ignore these interrupts the severity of the interrupt will increase, i.e. init will always win
  //
  // What IPC will it use? (compare `ps -ef` and `ipcs -p`)
  // IPC and device drivers are integral to unix.
  // Can use unnamed pipes, named/fifo pipes, shared memory (most efficient as requires no syscalls)
  
  // server does all rendering, input etc. (would be in front of us)
  // the client does all the logic
  Display *display = XOpenDisplay(NULL);
  if (display == NULL) {
    // TODO(Ryan): Error logging
    DEBUGGER_BREAK();
  }

  // want to minimise indirection and excessive parameters in documentation 
  
  Window default_root_window = XDefaultRootWindow(display);
  int default_screen = XDefaultScreen(display);

  int screen_bit_depth = 24;
  XVisualInfo visual_info = {0};
  // TrueColor means 24bits
  if (XMatchVisualInfo(display, default_screen, screen_bit_depth, TrueColor, &visual_info) == 0) {
    // TODO(Ryan): Error logging
    DEBUGGER_BREAK();
  }

  int window_width = 1280;
  int window_height = 720;

  XSetWindowAttributes window_attr = {0};
  window_attr.background_pixel = 0;
  window_attr.bit_gravity = StaticGravity;
  window_attr.event_mask = StructureNotifyMask;
  unsigned long attribute_mask = CWBackPixel | CWEventMask | CWBitGravity;
  // TODO(Ryan): x = y = 0 does not equate to top corner. There is some padding.
  // Perhaps this is the window manager?
  Window window = XCreateWindow(display, default_root_window,
                                0, 0,
                                window_width, window_height, 0,
                                visual_info.depth, InputOutput,
                                visual_info.visual, attribute_mask, &window_attr);
  if (window == BadAlloc || window == BadColor || window == BadCursor || 
      window == BadMatch || window == BadPixmap || window == BadValue || 
      window == BadWindow) {
    // TODO(Ryan): Error logging
    DEBUGGER_BREAK();
  } 

  int store_name_res = XStoreName(display, window, "WindowName");
  if (store_name_res == BadAlloc || store_name_res == BadWindow) {
    // TODO(Ryan): Error logging
    DEBUGGER_BREAK();
  }

  /* UTF8
  // iccm window manager manual
  Atom wmName = XInternAtom(display, "NET_WM_NAME", False);
  XChangeProperty(display, window, wmName, utf8Str, 8, PropModeReplace, (unsigned char *)name, i);
  // extended iccm window manager manual
  Atom wm_Name = XInternAtom(display, "_NET_WM_NAME", FALSE);
  Atom utf8Str = XInternAtom(display, "UTF8_STRING", FALSE);
  
  // presumably the char* will be a unicode string
  // look at sdl2 for usage functions 
  char * j = name;
  int i;
  for (i = 0; i < 1000 && *j != 0; j++, i++) {}
  {XChangeProperty(xfi->display, window->handle, wm_Name, utf8Str, 8, PropModeReplace, (unsigned char *)name, i);
  */

  if (XMapWindow(display, window) == BadWindow) {
    // TODO(Ryan): Error logging
    DEBUGGER_BREAK();

  }

  XFlush(display);

  // look at window manager docs for more information
  // atom just key-value pair, i.e. atom-property
  Atom delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
  if (delete_window == BadAlloc || delete_window == BadValue) {
    // TODO(Ryan): Error logging
    DEBUGGER_BREAK();
  }
  Status wm_set_res = XSetWMProtocols(display, window, &delete_window, 1);
  if (wm_set_res == BadAlloc || wm_set_res == BadWindow) {
    // TODO(Ryan): Error logging
    DEBUGGER_BREAK();
  }
 
  int bytes_per_pixel = 4;
  uint pixel_buffer_width = window_width;
  uint pixel_buffer_height = window_height;
  uint pixel_buffer_size = bytes_per_pixel * pixel_buffer_width * pixel_buffer_height;
  // allocate/deallocate in waves. os handles closing things, we don't have to
  // mmap whether or not we are using the pages now, and whether can execute
  // munmap(), also have mprotect() which can protect against 'use after free' bugs
  char *pixel_buffer = mmap(NULL, 
                            pixel_buffer_size, 
                            PROT_READ | PROT_WRITE, 
                            MAP_ANON | MAP_PRIVATE, 
                            -1, 0);
  if (pixel_buffer == NULL) {
    // TODO(Ryan): Error logging
    DEBUGGER_BREAK();
  }
  // xorg server docs for ZPixmap
  XImage *pixmap = XCreateImage(display,
                                visual_info.visual,
                                visual_info.depth,
                                ZPixmap,
                                0,
                                pixel_buffer,
                                window_width, window_height,
                                bytes_per_pixel * 4,
                                0);
  if (pixmap == NULL) {
    // TODO(Ryan): Error logging
    DEBUGGER_BREAK();
  }
  // TODO(Ryan): Do we need to recalculate this under special circumstances, e.g. change in resolution, new monitor added?
  GC default_gc = DefaultGC(display, default_screen);

  //int page_size = getpagesize();
  //int starting_size = 1024;
  //int aligned_range = starting_size + page_size;

  // 32-64 bit pointer size and address space concerns really

  // globals are bad as we want to easily know who can change what
  bool want_to_run = true;
  while (want_to_run) {
    XEvent event = {0};
    // TODO(Ryan): step through events in debugger, watch and call stack
    // an event.type is part of group, i.e. event mask
    while (XCheckWindowEvent(display, window, StructureNotifyMask, &event)) {
      switch (event.type) {
        case DestroyNotify: {
          // necessary if can't register with window manager
          want_to_run = false;
        } break;
        case ConfigureNotify: {
          XConfigureEvent *ev = (XConfigureEvent *)&event;
          window_width = ev->width;
          window_height = ev->height;
        } break;
      }
    }
    // necessary as clientmessage is not maskable
    while (XCheckTypedWindowEvent(display, window, ClientMessage, &event)) {
      if (event.xclient.data.l[0] == delete_window) {
          XDestroyWindow(display, window);
          want_to_run = false;
      }
    }

//    u32 *pixel_row = (u32 *)pixel_buffer;
//    for (uint x = 0; x < pixel_buffer_width; ++x) {
//      for (uint y = 0; y < pixel_buffer_height; ++y) {}
//
//    }

    XPutImage(display, 
              window, 
              default_gc, 
              pixmap, 
              0, 0, 0, 0, 
              window_width, window_height);
  }
  
  // NOTE(Ryan): Do all software rendering as gpu is opaque and transient resource
  return 0;
}

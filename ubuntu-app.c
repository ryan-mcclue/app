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
int
main(int argc, char *argv[])
{
  return 0;
}

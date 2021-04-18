#! /usr/bin/env bash
# SPDX-License-Identifier: zlib-acknowledgement

# IMPORTANT(Ryan): Assuming $(cfdisk) and $(mkfs.ext4) (inode table size specification)
mkdir -p bin sbin lib lib64 etc var dev proc sys run tmp boot

# every device file has a major (driver) and minor number (num of type of device)
# determined by LANANA (Linux assigned names and numbers authority)
# reflected in /Documentation/admin-guide/devices.txt

# major 5 is alternate tty device?
# TODO(Ryan): Investigate tty
mknod -m 600 ./dev/console c 5 1
# r-w-x u-g-o
mknod -m 666 ./dev/null c 1 3

# boot dir is where our kernel will live

# file is simply inumber-name pair. stat syscall will query inode data structure table information
# - : regular file
# l : soft link/symbolic link
# d : directory
# b : block device (typically something that will store data)
# c : character device (typically something like mouse or keyboard)

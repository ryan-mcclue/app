#! /usr/bin/env bash
# SPDX-License-Identifier: zlib-acknowledgement

~/prog/apps/pc-lint/pclp64_linux \
        -i/home/ryan/prog/personal/app \
        -i/home/ryan/prog/apps/pc-lint/config \
        -format="%f:%l:%c: %m" -summary -max_threads=$(getconf _NPROCESSORS_ONLN) \
        /home/ryan/prog/apps/pc-lint/lnt/au-misra3.lnt \
        /home/ryan/prog/apps/pc-lint/config/co-gcc.lnt \
        -wlib\(4\) -wlib\(1\) \
        -e970 \
        ubuntu-app.c

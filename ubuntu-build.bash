#! /usr/bin/env bash
# SPDX-License-Identifier: zlib-acknowledgement

[ ! -d ubuntu-build ] && mkdir ubuntu-build

gcc -g ubuntu-app.c -o ubuntu-build/ubuntu-app

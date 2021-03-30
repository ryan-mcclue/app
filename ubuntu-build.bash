#! /usr/bin/env bash
# SPDX-License-Identifier: zlib-acknowledgement

# NOTE(Ryan): No complex build tools as you spend more time on build tool than actually building
[[ ! -d ubuntu-build ]] && mkdir ubuntu-build


gcc -g ubuntu-app.c -o ubuntu-build/ubuntu-app -lX11 -ludev -lXrandr

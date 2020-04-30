#!/usr/bin/env bash

set -e

./b2 --user-config=./user-config.jam \
  toolset=clang,clang-a,clang-b,clang-c,clang-ab,clang-ac,gcc,gcc-a,gcc-b,gcc-c,gcc-ab,gcc-ac \
  variant=release \
  cxxstd=2a,17,14,11 \
  -j`grep processor /proc/cpuinfo | wc -l` \
  -q \
  libs/beast//test libs/beast//example

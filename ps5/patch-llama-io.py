#!/usr/bin/env python3
"""Big stdio buffers for llama.cpp's model reads: the console's libc reads a FILE in buffer-sized pieces (a few KB by
default), which held trellis.cpp's weight loading to about 90 MB/s (see the setvbuf lines in scripts/build-mg.sh).
usage: patch-llama-io.py <llama.cpp dir>   (does nothing when already applied)"""
import os
import sys

p = os.path.join(sys.argv[1], "src", "llama-mmap.cpp")
s = open(p, encoding="utf-8").read()
MARK = "_IOFBF, 16 << 20"
if MARK in s:
    sys.exit(0)
old = """        fp = ggml_fopen(fname.c_str(), mode);
        if (fp == NULL) {
            throw std::runtime_error(format("failed to open %s: %s", fname.c_str(), strerror(errno)));
        }
"""
if s.count(old) != 1:
    sys.exit("patch-llama-io: anchor not found once in src/llama-mmap.cpp")
new = old + """        if (std::strchr(mode, 'r')) setvbuf(fp, nullptr, _IOFBF, 16 << 20);   // PAUL3D: fast reads on the console
"""
s = s.replace(old, new)
if "#include <cstring>" not in s:
    s = s.replace('#include "llama-mmap.h"\n', '#include "llama-mmap.h"\n#include <cstring>\n', 1)
open(p, "w", encoding="utf-8").write(s)
print("patch-llama-io: src/llama-mmap.cpp")

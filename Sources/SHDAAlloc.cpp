//
//  SHDAAlloc.cpp
//  SimpleHDA
//
//  Created by laobamac on 2026/8/20.
//  Copyright © 2026 laobamac. All rights reserved.
//
#include "SHDAAlloc.h"
#include <IOKit/IOLib.h>
extern "C" void *shda_alloc(size_t size) {
    if (!size || size > 128U * 1024U * 1024U) return nullptr;
    auto p = IOMalloc(size);
    if (p) bzero(p, size);
    return p;
}
extern "C" void shda_free(void *p, size_t size) {
    if (p) IOFree(p, size);
}

extern "C" void shda_weak_reference() { panic("SimpleHDA: unresolved weak symbol was called"); }

//
//  SHDAAlloc.h
//  SimpleHDA
//
//  Created by laobamac on 2026/8/31.
//  Copyright © 2026 laobamac. All rights reserved.
//
#ifndef SHDA_ALLOC_H
#define SHDA_ALLOC_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
void *shda_alloc(size_t size);
void shda_free(void *address, size_t size);
#ifdef __cplusplus
}
#endif
#endif

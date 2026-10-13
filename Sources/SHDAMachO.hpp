//
//  SHDAMachO.hpp
//  SimpleHDA
//
//  Created by laobamac on 2026/8/10.
//  Copyright © 2026 laobamac. All rights reserved.
//
#ifndef SHDA_MACHO_HPP
#define SHDA_MACHO_HPP
#include <stdint.h>
#include <stddef.h>
#include <mach-o/loader.h>

namespace shda {
struct Buffer {
    uint8_t *data {};
    size_t size {};
    void clear();
};
using Reader = bool (*)(uint64_t address, void *destination, size_t size, void *context);
bool snapshot(const mach_header_64 *header, Reader reader, void *context, Buffer &result);
bool interfaceFor(const Buffer &symbols, Buffer &result);
bool validateFile(const void *data, size_t size);
bool preparePrelink(void *image, size_t size, uint64_t slide);
const mach_header_64 *findEntry(const mach_header_64 *collection, const char *identifier);
}
#endif

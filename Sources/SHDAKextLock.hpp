//
//  SHDAKextLock.hpp
//  SimpleHDA
//
//  Created by laobamac on 2026/8/23.
//  Copyright © 2026 laobamac. All rights reserved.
//
#ifndef SHDA_KEXT_LOCK_HPP
#define SHDA_KEXT_LOCK_HPP
#include <stddef.h>
#include <stdint.h>
#if KERNEL
#include <libkern/libkern.h>
#else
#include <string.h>
#endif
namespace shda {
using InstructionLength = size_t (*)(const uint8_t *);
inline size_t kextLockReferences(const uint8_t *code, uintptr_t address, size_t limit,
    InstructionLength instructionLength, uintptr_t slots[8]) {
    size_t count = 0;
    for (size_t offset = 0; offset < limit;) {
        auto length = instructionLength(code + offset);
        if (!length || length > 15) return 0;
        if (length > limit - offset) break;
        if (length == 7 && (code[offset] & 0xf8) == 0x48 && code[offset + 1] == 0x8b &&
            (code[offset + 2] & 0xc7) == 0x05) {
            int32_t delta;
            memcpy(&delta, code + offset + 3, sizeof(delta));
            auto slot = address + offset + length + static_cast<intptr_t>(delta);
            bool present = false;
            for (size_t i = 0; i < count; ++i) present |= slots[i] == slot;
            if (!present && !(slot & (sizeof(void *) - 1))) {
                if (count == 8) return 0;
                slots[count++] = slot;
            }
        }
        if (code[offset] == 0xc3 || code[offset] == 0xc2) break;
        offset += length;
    }
    return count;
}
inline uintptr_t kextLockSlot(const uint8_t *lookup, uintptr_t lookupAddress,
    const uint8_t *cancel, uintptr_t cancelAddress, InstructionLength instructionLength, size_t limit = 128) {
    if (!lookup || !cancel || !instructionLength) return 0;
    uintptr_t first[8] {}, second[8] {}, found = 0;
    auto a = kextLockReferences(lookup, lookupAddress, limit, instructionLength, first);
    auto b = kextLockReferences(cancel, cancelAddress, limit, instructionLength, second);
    for (size_t i = 0; i < a; ++i) {
        for (size_t j = 0; j < b; ++j) {
            if (first[i] != second[j]) continue;
            if (found && found != first[i]) return 0;
            found = first[i];
        }
    }
    return found;
}
}
#endif

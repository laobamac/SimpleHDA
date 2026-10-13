//
//  SHDAPayload.hpp
//  SimpleHDA
//
//  Created by laobamac on 2026/8/14.
//  Copyright © 2026 laobamac. All rights reserved.
//
#ifndef SHDA_PAYLOAD_HPP
#define SHDA_PAYLOAD_HPP
#include <stdint.h>
#include <stddef.h>
namespace shda {
struct Module {
    const char *identifier;
    const char *path;
    const char *plist;
    const uint8_t *executable;
    size_t executableSize;
    const char * const *libraries;
    size_t libraryCount;
};
struct Resource { const char *name; const uint8_t *data; uint32_t size; };
struct Translation { const char *key, *value; };
struct Localization { const char *locale; const Translation *strings; size_t count; };
extern const Module modules[];
extern const size_t moduleCount;
extern const Resource resources[];
extern const size_t resourceCount;
extern const Localization localizations[];
extern const size_t localizationCount;
extern const char defaultLocale[];
}
#endif

//
//  SHDAResources.hpp
//  SimpleHDA
//
//  Created by laobamac on 2026/8/23.
//  Copyright © 2026 laobamac. All rights reserved.
//
#ifndef SHDA_RESOURCES_HPP
#define SHDA_RESOURCES_HPP
#include <libkern/OSKextLib.h>
#include <IOKit/IOLib.h>
namespace shda {
struct ResourceRuntime {
    IORecursiveLock *lock;
    void *(*retainOwner)(uintptr_t);
    bool (*canDeliver)(void *);
    void (*releaseOwner)(void *);
};
bool initializeResources(const ResourceRuntime &);
OSReturn requestResource(const char *, const char *, OSKextRequestResourceCallback, void *, OSKextRequestTag *);
OSReturn cancelResource(OSKextRequestTag, void **);
}
#endif

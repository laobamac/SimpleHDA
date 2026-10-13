//
//  SHDAResources.cpp
//  SimpleHDA
//
//  Created by laobamac on 2026/8/27.
//  Copyright © 2026 laobamac. All rights reserved.
//
#include "SHDAResources.hpp"
#include "SHDAPayload.hpp"
#include <IOKit/IOLib.h>
#include <kern/thread_call.h>

namespace {
struct Request {
    OSKextRequestTag tag;
    OSKextRequestResourceCallback callback;
    void *context;
    const shda::Resource *resource;
    void *owner;
};
shda::ResourceRuntime runtime {};
thread_call_t worker;
Request pending[64] {};
uint32_t serial;
void deliver(thread_call_param_t, thread_call_param_t) {
    IORecursiveLockLock(runtime.lock);
    for (;;) {
        Request next {};
        for (auto &r : pending) {
            if (r.callback) { next = r; r = {}; break; }
        }
        if (!next.callback) break;
        if (runtime.canDeliver(next.owner)) {
            next.callback(next.tag, next.resource ? kOSReturnSuccess : kOSKextReturnNotFound,
                next.resource ? next.resource->data : nullptr, next.resource ? next.resource->size : 0, next.context);
        }
        runtime.releaseOwner(next.owner);
    }
    IORecursiveLockUnlock(runtime.lock);
}
}
namespace shda {
bool initializeResources(const ResourceRuntime &configuration) {
    if (!configuration.lock || !configuration.retainOwner || !configuration.canDeliver || !configuration.releaseOwner) return false;
    if (worker) return runtime.lock == configuration.lock;
    runtime = configuration;
    worker = thread_call_allocate(deliver, nullptr);
    return worker != nullptr;
}
OSReturn requestResource(const char *id, const char *name, OSKextRequestResourceCallback cb, void *ctx, OSKextRequestTag *tag) {
    if (!id || strcmp(id, "com.apple.driver.AppleHDA")) return OSKextRequestResource(id, name, cb, ctx, tag);
    if (tag) *tag = kOSKextRequestTagInvalid;
    if (!name || !cb || !worker) return kOSKextReturnInvalidArgument;
    const Resource *res = nullptr;
    for (size_t i = 0; i < resourceCount; ++i) if (!strcmp(resources[i].name, name)) { res = resources + i; break; }
    IORecursiveLockLock(runtime.lock);
    for (auto &r : pending) {
        if (r.callback) continue;
        if (serial == 65535) { IORecursiveLockUnlock(runtime.lock); return kOSKextReturnNoResources; }
        auto owner = runtime.retainOwner(reinterpret_cast<uintptr_t>(cb));
        if (!owner) { IORecursiveLockUnlock(runtime.lock); return kOSKextReturnInvalidArgument; }
        auto t = 0x53480000U | ++serial;
        r = {t, cb, ctx, res, owner};
        if (tag) *tag = t;
        thread_call_enter(worker);
        IORecursiveLockUnlock(runtime.lock);
        return kOSReturnSuccess;
    }
    IORecursiveLockUnlock(runtime.lock);
    return kOSKextReturnNoResources;
}
OSReturn cancelResource(OSKextRequestTag tag, void **context) {
    if ((tag & 0xffff0000U) != 0x53480000U) return OSKextCancelRequest(tag, context);
    if (context) *context = nullptr;
    if (!worker) return kOSKextReturnNotFound;
    IORecursiveLockLock(runtime.lock);
    for (auto &r : pending) {
        if (!r.callback || r.tag != tag) continue;
        if (context) *context = r.context;
        runtime.releaseOwner(r.owner);
        r = {};
        IORecursiveLockUnlock(runtime.lock);
        return kOSReturnSuccess;
    }
    IORecursiveLockUnlock(runtime.lock);
    return kOSKextReturnNotFound;
}
}

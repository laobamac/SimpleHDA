//
//  SHDAHAL.cpp
//  SimpleHDA
//
//  Created by laobamac on 2026/8/10.
//  Copyright © 2026 laobamac. All rights reserved.
//
#include "SHDAHAL.hpp"
#include "SHDALocalization.hpp"
#include <Headers/kern_util.hpp>
#include <sys/vnode.h>

namespace {
constexpr char bundle[] = "/Library/Audio/Plug-Ins/SimpleHDA/AppleHDAHALPlugIn.bundle";
constexpr char redirect[] = "../../../Library/Audio/Plug-Ins/SimpleHDA/AppleHDAHALPlugIn.bundle";
bool enabled;
bool fileAvailable(const char *path, vfs_context_t context) {
    vnode_t node = NULLVP;
    if (vnode_lookup(path, 0, &node, context)) return false;
    vnode_attr attributes;
    VATTR_INIT(&attributes);
    VATTR_WANTED(&attributes, va_data_size);
    bool available = vnode_vtype(node) == VREG && !vnode_getattr(node, &attributes, context) &&
        VATTR_IS_SUPPORTED(&attributes, va_data_size) && attributes.va_data_size > 0;
    vnode_put(node);
    return available;
}
bool installed() {
    auto context = vfs_context_create(nullptr);
    if (!context) return false;
    char path[256];
    snprintf(path, sizeof(path), "%s/Contents/Info.plist", bundle);
    bool available = fileAvailable(path, context);
    if (available) {
        snprintf(path, sizeof(path), "%s/Contents/MacOS/AppleHDAHALPlugIn", bundle);
        available = fileAvailable(path, context);
    }
    vfs_context_rele(context);
    return available;
}
}
namespace shda {
bool configureHAL() {
    enabled = !checkKernelArgument("-shdahaloff");
    auto root = IORegistryEntry::getRegistryRoot();
    if (root) root->setProperty("SimpleHDAHALMode", enabled ? "auto" : "disabled");
    return enabled;
}
bool setAudioProperty(IORegistryEntry *entry, const char *key, const char *value) {
    if (key && value && !strcmp(key, "IOAudioEngineCoreAudioPlugIn") &&
        !strcmp(value, "AppleHDA.kext/Contents/PlugIns/AppleHDAHALPlugIn.bundle")) {
        bool available = enabled && installed();
        auto root = IORegistryEntry::getRegistryRoot();
        if (root) {
            root->setProperty("SimpleHDAHALMode", available ? "data-redirect" : enabled ? "not-installed" : "disabled");
            if (available) root->setProperty("SimpleHDAHALPath", redirect);
            else root->removeProperty("SimpleHDAHALPath");
        }
        entry->IORegistryEntry::setProperty("SimpleHDAHALAvailable", available);
        if (!available) {
            entry->IORegistryEntry::removeProperty(key);
            return true;
        }
        return entry->IORegistryEntry::setProperty(key, redirect);
    }
    if (key && (!strcmp(key, "IOAudioDeviceName") || !strcmp(key, "IOAudioDeviceShortName") ||
        !strcmp(key, "IOAudioDeviceManufacturerName") || !strcmp(key, "IOAudioEngineDescription") ||
        !strcmp(key, "IOAudioControlChannelName"))) value = localized(value);
    return entry->IORegistryEntry::setProperty(key, value);
}
}

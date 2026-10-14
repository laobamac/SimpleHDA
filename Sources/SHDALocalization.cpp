//
//  SHDALocalization.cpp
//  SimpleHDA
//
//  Created by laobamac on 2026/8/28.
//  Copyright © 2026 laobamac. All rights reserved.
//
#include "SHDALocalization.hpp"
#include "SHDAPayload.hpp"
#if KERNEL
#include <Headers/kern_util.hpp>
#include <Headers/kern_patcher.hpp>
#include <libkern/c++/OSString.h>
#include <libkern/c++/OSData.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IODeviceTreeSupport.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-W#warnings"
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#include <IOKit/audio/IOAudioSelectorControl.h>
#pragma clang diagnostic pop
#else
#include <string.h>
#endif

namespace {
const shda::Localization *selected;
bool equivalent(const char *a, const char *b) {
    while (*a && *b) {
        auto x = *a++, y = *b++;
        if (x == '-') x = '_';
        if (y == '-') y = '_';
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y) return false;
    }
    return *a == *b;
}
const shda::Localization *findLocale(const char *name) {
    if (!name) return nullptr;
    for (size_t i = 0; i < shda::localizationCount; ++i)
        if (equivalent(name, shda::localizations[i].locale)) return shda::localizations + i;
    return nullptr;
}
}
namespace shda {
bool selectLocale(const char *name) {
#if KERNEL
    char nvramLang[16] {};
    if (!name) {
        auto options = IORegistryEntry::fromPath("/options", gIODTRegistryPlane);
        if (options) {
            auto prop = options->getProperty("prev-lang:kbd");
            if (prop) {
                const char *strBytes = nullptr;
                size_t strLen = 0;
                auto dataProp = OSDynamicCast(OSData, prop);
                if (dataProp) {
                    strBytes = static_cast<const char *>(dataProp->getBytesNoCopy());
                    strLen = dataProp->getLength();
                } else {
                    auto strProp = OSDynamicCast(OSString, prop);
                    if (strProp) {
                        strBytes = strProp->getCStringNoCopy();
                        strLen = strBytes ? strlen(strBytes) : 0;
                    }
                }
                if (strBytes && strLen > 0) {
                    size_t i = 0;
                    while (i < strLen && i < sizeof(nvramLang) - 1 &&
                           strBytes[i] != 0 && strBytes[i] != ':' &&
                           strBytes[i] != ' ' && strBytes[i] != '\n' && strBytes[i] != '\r') {
                        nvramLang[i] = strBytes[i];
                        ++i;
                    }
                    nvramLang[i] = '\0';
                    if (i > 0) name = nvramLang;
                }
            }
            options->release();
        }
    }
#endif
    auto found = findLocale(name);
    if (!found && name) {
        if (equivalent(name, "zh-Hans") || equivalent(name, "zh-Hans-CN") || equivalent(name, "zh-CN")) found = findLocale("zh_CN");
        if (equivalent(name, "zh-Hant") || equivalent(name, "zh-Hant-TW") || equivalent(name, "zh-TW")) found = findLocale("zh_TW");
        if (equivalent(name, "zh-Hant-HK") || equivalent(name, "zh-HK")) found = findLocale("zh_HK");
    }
    if (!found && name) {
        char language[16] {};
        size_t size = 0;
        while (name[size] && name[size] != '-' && name[size] != '_' && size < sizeof(language) - 1) {
            language[size] = name[size];
            ++size;
        }
        found = findLocale(language);
    }
    if (found) selected = found;
    else if (!selected) {
        selected = findLocale(defaultLocale);
        if (!selected) selected = findLocale("en");
    }
    return found != nullptr;
}
const char *selectedLocale() {
    if (!selected) selectLocale(nullptr);
    return selected ? selected->locale : "en";
}
const char *localized(const char *key) {
    if (!key) return nullptr;
    if (!selected) selectLocale(nullptr);
    if (selected)
        for (size_t i = 0; i < selected->count; ++i)
            if (!strcmp(key, selected->strings[i].key)) return selected->strings[i].value;
    auto english = findLocale("en");
    if (english && english != selected)
        for (size_t i = 0; i < english->count; ++i)
            if (!strcmp(key, english->strings[i].key)) return english->strings[i].value;
    return key;
}
}

#if KERNEL
class IOAudioDevice;
class IOAudioEngine;
class IOAudioControl;
namespace {
using DeviceSetter = void (*)(IOAudioDevice *, const char *);
using EngineSetter = void (*)(IOAudioEngine *, const char *);
using ControlSetter = void (*)(IOAudioControl *, const char *);
using SelectionText = IOReturn (*)(IOAudioSelectorControl *, SInt32, const char *);
using SelectionString = IOReturn (*)(IOAudioSelectorControl *, SInt32, OSString *);
using SelectionTagged = IOReturn (*)(IOAudioSelectorControl *, SInt32, OSString *, const char *, OSObject *);
using SelectorFactory = IOAudioSelectorControl *(*)(SInt32, UInt32, const char *, UInt32);
using SelectorCreate = IOAudioSelectorControl *(*)(SInt32, UInt32, const char *, UInt32, UInt32, UInt32);
using SelectorInit = bool (*)(IOAudioSelectorControl *, SInt32, UInt32, const char *, UInt32, UInt32, UInt32, OSDictionary *);
DeviceSetter deviceName, shortName, manufacturer, modelName;
EngineSetter description;
ControlSetter channelName;
SelectionText selectionText;
SelectionString selectionString;
SelectionTagged selectionTagged;
SelectionText replaceText;
SelectionString replaceString;
SelectorFactory inputFactory, outputFactory;
SelectorCreate selectorCreate;
SelectorInit selectorInit;
SelectionTagged originalTagged;
SelectionString originalAdd, originalReplace;
const char selectorMarker[] = "SimpleHDALocalizeSelections";
const char *deviceKey(const char *text, const char *truncated, const char *full) {
    return shda::localized(text && !strcmp(text, truncated) ? full : text);
}
void setDeviceName(IOAudioDevice *device, const char *text) { deviceName(device, deviceKey(text, "DeviceN", "DeviceName")); }
void setShortName(IOAudioDevice *device, const char *text) { shortName(device, deviceKey(text, "DeviceS", "DeviceShortName")); }
void setManufacturer(IOAudioDevice *device, const char *text) { manufacturer(device, shda::localized(text)); }
void setModelName(IOAudioDevice *device, const char *text) { modelName(device, shda::localized(text)); }
void setDescription(IOAudioEngine *engine, const char *text) { description(engine, shda::localized(text)); }
void setChannelName(IOAudioControl *control, const char *text) { channelName(control, shda::localized(text)); }
IOReturn addSelectionText(IOAudioSelectorControl *control, SInt32 value, const char *text) {
    return selectionText(control, value, shda::localized(text));
}
IOReturn addSelectionString(IOAudioSelectorControl *control, SInt32 value, OSString *text) {
    auto original = text ? text->getCStringNoCopy() : nullptr;
    auto translated = shda::localized(original);
    if (translated == original) return selectionString(control, value, text);
    auto copy = OSString::withCString(translated);
    if (!copy) return selectionString(control, value, text);
    auto result = selectionString(control, value, copy);
    copy->release();
    return result;
}
IOReturn addSelectionTagged(IOAudioSelectorControl *control, SInt32 value, OSString *text, const char *name, OSObject *tag) {
    auto original = text ? text->getCStringNoCopy() : nullptr;
    auto translated = shda::localized(original);
    if (translated == original) return selectionTagged(control, value, text, name, tag);
    auto copy = OSString::withCString(translated);
    if (!copy) return selectionTagged(control, value, text, name, tag);
    auto result = selectionTagged(control, value, copy, name, tag);
    copy->release();
    return result;
}
IOReturn replaceSelectionText(IOAudioSelectorControl *control, SInt32 value, const char *text) {
    return replaceText(control, value, shda::localized(text));
}
IOReturn replaceSelectionString(IOAudioSelectorControl *control, SInt32 value, OSString *text) {
    auto original = text ? text->getCStringNoCopy() : nullptr;
    auto translated = shda::localized(original);
    if (translated == original) return replaceString(control, value, text);
    auto copy = OSString::withCString(translated);
    if (!copy) return replaceString(control, value, text);
    auto result = replaceString(control, value, copy);
    copy->release();
    return result;
}
IOAudioSelectorControl *markSelector(IOAudioSelectorControl *control) {
    if (control) control->IORegistryEntry::setProperty(selectorMarker, true);
    return control;
}
IOAudioSelectorControl *createInput(SInt32 value, UInt32 channel, const char *name, UInt32 id) {
    return markSelector(inputFactory(value, channel, shda::localized(name), id));
}
IOAudioSelectorControl *createOutput(SInt32 value, UInt32 channel, const char *name, UInt32 id) {
    return markSelector(outputFactory(value, channel, shda::localized(name), id));
}
IOAudioSelectorControl *createSelector(SInt32 value, UInt32 channel, const char *name, UInt32 id, UInt32 subtype, UInt32 usage) {
    return markSelector(selectorCreate(value, channel, shda::localized(name), id, subtype, usage));
}
bool initSelector(IOAudioSelectorControl *control, SInt32 value, UInt32 channel, const char *name, UInt32 id, UInt32 subtype, UInt32 usage, OSDictionary *properties) {
    auto result = selectorInit(control, value, channel, shda::localized(name), id, subtype, usage, properties);
    if (result) markSelector(control);
    return result;
}
IOReturn nativeAddTagged(IOAudioSelectorControl *control, SInt32 value, OSString *text, const char *name, OSObject *tag) {
    if (!control->IORegistryEntry::getProperty(selectorMarker)) return originalTagged(control, value, text, name, tag);
    auto original = text ? text->getCStringNoCopy() : nullptr;
    auto translated = shda::localized(original);
    if (translated == original) return originalTagged(control, value, text, name, tag);
    auto copy = OSString::withCString(translated);
    if (!copy) return originalTagged(control, value, text, name, tag);
    auto result = originalTagged(control, value, copy, name, tag);
    copy->release();
    return result;
}
IOReturn nativeAddString(IOAudioSelectorControl *control, SInt32 value, OSString *text) {
    if (!control->IORegistryEntry::getProperty(selectorMarker)) return originalAdd(control, value, text);
    auto original = text ? text->getCStringNoCopy() : nullptr;
    auto translated = shda::localized(original);
    if (translated == original) return originalAdd(control, value, text);
    auto copy = OSString::withCString(translated);
    if (!copy) return originalAdd(control, value, text);
    auto result = originalAdd(control, value, copy);
    copy->release();
    return result;
}
IOReturn nativeReplace(IOAudioSelectorControl *control, SInt32 value, OSString *text) {
    if (!control->IORegistryEntry::getProperty(selectorMarker)) return originalReplace(control, value, text);
    auto original = text ? text->getCStringNoCopy() : nullptr;
    auto translated = shda::localized(original);
    if (translated == original) return originalReplace(control, value, text);
    auto copy = OSString::withCString(translated);
    if (!copy) return originalReplace(control, value, text);
    auto result = originalReplace(control, value, copy);
    copy->release();
    return result;
}
template <typename T> bool bind(const char *name, const char *expected, uint64_t &address, T &native, T wrapper) {
    if (strcmp(name, expected)) return false;
    native = reinterpret_cast<T>(address);
    address = reinterpret_cast<uintptr_t>(wrapper);
    return true;
}
}
namespace shda {
void configureLocalization() {
    char locale[64] {};
    selectLocale(nullptr);
    if (lilu_get_boot_args("shda-lang", locale, sizeof(locale))) {
        locale[sizeof(locale) - 1] = 0;
        if (!selectLocale(locale)) IOLog("SimpleHDA: unsupported locale %s; using %s\n", locale, selectedLocale());
    }
    auto root = IORegistryEntry::getRegistryRoot();
    if (root) root->setProperty("SimpleHDALocale", selectedLocale());
}
bool bindLocalizationSymbol(const char *name, uint64_t &address) {
    return bind(name, localizationImports[0], address, deviceName, setDeviceName) ||
        bind(name, localizationImports[1], address, shortName, setShortName) ||
        bind(name, localizationImports[2], address, manufacturer, setManufacturer) ||
        bind(name, localizationImports[3], address, modelName, setModelName) ||
        bind(name, localizationImports[4], address, description, setDescription) ||
        bind(name, localizationImports[5], address, channelName, setChannelName) ||
        bind(name, localizationImports[6], address, selectionText, addSelectionText) ||
        bind(name, localizationImports[7], address, selectionString, addSelectionString) ||
        bind(name, localizationImports[8], address, selectionTagged, addSelectionTagged) ||
        bind(name, localizationImports[9], address, replaceText, replaceSelectionText) ||
        bind(name, localizationImports[10], address, replaceString, replaceSelectionString) ||
        bind(name, localizationImports[11], address, inputFactory, createInput) ||
        bind(name, localizationImports[12], address, outputFactory, createOutput) ||
        bind(name, localizationImports[13], address, selectorCreate, createSelector) ||
        bind(name, localizationImports[14], address, selectorInit, initSelector);
}
bool routeSelectorLocalization(KernelPatcher &patcher) {
    if (!selectionString || !selectionTagged || !replaceString) return false;
    KernelPatcher::RouteRequest requests[] {
        {nullptr, nativeAddString, originalAdd},
        {nullptr, nativeAddTagged, originalTagged},
        {nullptr, nativeReplace, originalReplace}
    };
    requests[0].from = reinterpret_cast<uintptr_t>(selectionString);
    requests[1].from = reinterpret_cast<uintptr_t>(selectionTagged);
    requests[2].from = reinterpret_cast<uintptr_t>(replaceString);
    if (!patcher.routeMultiple(KernelPatcher::KernelID, requests)) {
        patcher.clearError();
        return false;
    }
    auto root = IORegistryEntry::getRegistryRoot();
    if (root) root->setProperty("SimpleHDASelectorLocalization", "native-dispatch-scoped");
    return true;
}
}
#endif

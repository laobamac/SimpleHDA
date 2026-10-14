//
//  kern_start.cpp
//  SimpleHDA
//
//  Created by laobamac on 2026/8/10.
//  Copyright © 2026 laobamac. All rights reserved.
//
#include <Headers/kern_api.hpp>
#include <Headers/kern_patcher.hpp>
#include <Headers/plugin_start.hpp>
#include <IOKit/IORegistryEntry.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSString.h>
#include <libkern/c++/OSUnserialize.h>
#include <mach-o/nlist.h>
#include <mach/kmod.h>
#include <kern/thread.h>
#include "SHDAMachO.hpp"
#include "SHDAPayload.hpp"
#include "SHDAResources.hpp"
#include "SHDAHAL.hpp"
#include "SHDAStart.hpp"
#include "SHDAKextLock.hpp"
#include "SHDALocalization.hpp"
extern bool ADDPR(startSuccess);
extern "C" {
#include <libkern/kxld.h>
}

namespace {
KernelPatcher *runtimePatcher;
struct __attribute__((trivial_abi)) OwnedObject {
    OSObject *value;
    ~OwnedObject() { if (value) value->release(); }
    OwnedObject(const OwnedObject &) = delete;
    OwnedObject(OwnedObject &&other) : value(other.value) { other.value = nullptr; }
};
static_assert(sizeof(OwnedObject) == sizeof(void *), "OSSharedPtr ABI storage");
using Register = OwnedObject (*)(OSDictionary *, bool, int);
using Load = OSReturn (*)(const char *, bool, bool, uint8_t, uint8_t, void *);
using Loaded = bool (*)(const char *);
using Lookup = OwnedObject (*)(const char *);
using LookupAddress = OwnedObject (*)(uintptr_t);
using Flag = bool (*)(OSObject *);
using KCHeader = void *(*)(int);
using KCSlide = uintptr_t (*)(int);
using Allocate = kern_return_t (*)(vm_offset_t *, vm_size_t, boolean_t);
using Free = void (*)(vm_offset_t, vm_size_t);
Register registerImage;
Load loadImage;
Loaded isLoaded;
Lookup lookupImage;
LookupAddress lookupAddress;
Flag imageLoaded, imageStarted;
IORecursiveLock *kextLock;
OSObject *resident[8] {};
KCHeader kcHeader;
KCSlide kcSlide;
Allocate allocateImage;
Free freeImage;
OSReturn (*originalFileset)(OSDictionary *);
void (*originalCatalog)(bool);
thread_t filesetThread;
bool attempted;
char lastLinkError[1024];

const char *nativeIDs[] {
    "com.apple.kernel", "com.apple.iokit.IOPCIFamily", "com.apple.driver.AppleEFINVRAM",
    "com.apple.iokit.IOAudioFamily", "com.apple.iokit.IOGraphicsFamily",
    "com.apple.iokit.IONDRVSupport", "com.apple.kext.OSvKernDSPLib", "com.apple.vecLib.kext"
};
struct Dependency {
    const char *id {};
    shda::Buffer buffer;
};
Dependency dependencies[16];
size_t dependencyCount;
shda::Buffer kernelInterface;
void state(const char *value, const char *detail = "") {
    shda::publishState(value, detail);
}
void releaseDependencies() {
    for (size_t i = 0; i < dependencyCount; ++i) dependencies[i].buffer.clear();
    dependencyCount = 0;
    kernelInterface.clear();
}
bool liveRead(uint64_t address, void *destination, size_t length, void *) {
    if (address < 0xffffff0000000000ULL || length > 128U * 1024U * 1024U || address > UINT64_MAX - length) return false;
    memcpy(destination, reinterpret_cast<const void *>(address), length);
    return true;
}
const mach_header_64 *findNative(const char *id) {
    for (int kind = 1; kind <= 3; ++kind) {
        auto collection = static_cast<const mach_header_64 *>(kcHeader(kind));
        if (!collection) continue;
        auto image = shda::findEntry(collection, id);
        if (image) return image;
    }
    return nullptr;
}
bool addDependency(const char *id, const mach_header_64 *header) {
    if (dependencyCount == 16 || !header) return false;
    auto &d = dependencies[dependencyCount];
    if (!shda::snapshot(header, liveRead, nullptr, d.buffer)) {
        IOLog("SimpleHDA: snapshot failed: %s\n", id);
        return false;
    }
    d.id = id;
    ++dependencyCount;
    IOLog("SimpleHDA: snapshot %s bytes=%lu\n", id, d.buffer.size);
    return true;
}
bool redirectResources(shda::Buffer &buffer) {
    auto h = reinterpret_cast<mach_header_64 *>(buffer.data);
    auto p = reinterpret_cast<uint8_t *>(h + 1);
    unsigned found = 0;
    for (uint32_t i = 0; i < h->ncmds; ++i) {
        auto c = reinterpret_cast<load_command *>(p);
        if (c->cmd == LC_SYMTAB) {
            auto t = reinterpret_cast<symtab_command *>(c);
            auto n = reinterpret_cast<nlist_64 *>(buffer.data + t->symoff);
            auto strings = reinterpret_cast<const char *>(buffer.data + t->stroff);
            for (uint32_t j = 0; j < t->nsyms; ++j) {
                auto name = strings + n[j].n_un.n_strx;
                if (!strcmp(name, "_OSKextRequestResource")) { n[j].n_value = reinterpret_cast<uintptr_t>(shda::requestResource); ++found; }
                if (!strcmp(name, "__ZN15IORegistryEntry11setPropertyEPKcS1_")) n[j].n_value = reinterpret_cast<uintptr_t>(shda::setAudioProperty);
                if (!strcmp(name, "_OSKextCancelRequest")) { n[j].n_value = reinterpret_cast<uintptr_t>(shda::cancelResource); ++found; }
            }
        }
        p += c->cmdsize;
    }
    return found == 2;
}
struct Allocation { vm_offset_t address {}; vm_size_t size {}; };
kxld_addr_t allocate(size_t size, KXLDAllocateFlags *flags, void *context) {
    auto a = static_cast<Allocation *>(context);
    if (!size || size > 32U * 1024U * 1024U || a->address) return 0;
    a->size = (size + 4095) & ~vm_size_t(4095);
    if (allocateImage(&a->address, a->size, false) != KERN_SUCCESS) { a->address = 0; return 0; }
    *flags = kKxldAllocateWritable;
    return a->address;
}
void logLink(KXLDLogSubsystem, KXLDLogLevel level, const char *format, va_list args, void *) {
    if (level > kKxldLogBasic && !ADDPR(debugEnabled)) return;
    char text[1024];
    vsnprintf(text, sizeof(text), format, args);
    if (level == kKxldLogErr) {
        strlcpy(lastLinkError, text, sizeof(lastLinkError));
        auto root = IORegistryEntry::getRegistryRoot();
        if (root) root->setProperty("SimpleHDALinkError", text);
    }
    IOLog("SimpleHDA: %s\n", text);
}
bool setNumber(OSDictionary *dict, const char *key, uint64_t value) {
    auto n = OSNumber::withNumber(value, 64);
    if (!n) return false;
    auto result = dict->setObject(key, n);
    n->release();
    return result;
}
void *retainCallbackOwner(uintptr_t callback) {
    auto owner = lookupAddress(callback);
    auto value = owner.value;
    owner.value = nullptr;
    return value;
}
bool canDeliver(void *owner) {
    auto object = static_cast<OSObject *>(owner);
    return imageLoaded(object) && imageStarted(object);
}
void releaseCallbackOwner(void *owner) { static_cast<OSObject *>(owner)->release(); }
size_t instructionLength(const uint8_t *code) { return Disassembler::quickInstructionSize(reinterpret_cast<uintptr_t>(code), 1); }
bool kernelDataAddress(uintptr_t address, size_t length) {
    auto header = findNative("com.apple.kernel");
    if (!header) return false;
    auto command = reinterpret_cast<const uint8_t *>(header + 1);
    for (uint32_t i = 0; i < header->ncmds; ++i) {
        auto c = reinterpret_cast<const load_command *>(command);
        if (c->cmd == LC_SEGMENT_64) {
            auto segment = reinterpret_cast<const segment_command_64 *>(c);
            if ((segment->initprot & VM_PROT_WRITE) && address >= segment->vmaddr &&
                address - segment->vmaddr <= segment->vmsize && length <= segment->vmsize - (address - segment->vmaddr)) return true;
        }
        command += c->cmdsize;
    }
    return false;
}
bool run() {
    for (size_t i = 0; i < shda::moduleCount; ++i) {
        auto previous = lookupImage(shda::modules[i].identifier);
        if (previous.value || isLoaded(shda::modules[i].identifier) || findNative(shda::modules[i].identifier)) {
            state("existing-stack", shda::modules[i].identifier);
            return false;
        }
    }
    if (shda::moduleCount > 8) { state("payload-limit-exceeded"); return false; }
    shda::configureLocalization();
    if (!shda::initializeResources({kextLock, retainCallbackOwner, canDeliver, releaseCallbackOwner})) { state("resource-init-failed"); return false; }
    for (size_t i = 0; i < sizeof(nativeIDs) / sizeof(nativeIDs[0]); ++i) {
        if (i) {
            state("native-loading", nativeIDs[i]);
            auto result = loadImage(nativeIDs[i], false, true, 0, 2, nullptr);
            if (result != kOSReturnSuccess) {
                IOLog("SimpleHDA: native load %s error=0x%x\n", nativeIDs[i], result);
                state("native-load-failed", nativeIDs[i]);
                return false;
            }
        }
        state("native-snapshot", nativeIDs[i]);
        if (!addDependency(nativeIDs[i], findNative(nativeIDs[i]))) { state("native-snapshot-failed", nativeIDs[i]); return false; }
    }
    {
        auto &buffer = dependencies[3].buffer;
        auto header = reinterpret_cast<mach_header_64 *>(buffer.data);
        auto command = reinterpret_cast<uint8_t *>(header + 1);
        unsigned bindings = 0;
        for (uint32_t i = 0; i < header->ncmds; ++i) {
            auto c = reinterpret_cast<load_command *>(command);
            if (c->cmd == LC_SYMTAB) {
                auto t = reinterpret_cast<symtab_command *>(c);
                auto symbols = reinterpret_cast<nlist_64 *>(buffer.data + t->symoff);
                auto names = reinterpret_cast<const char *>(buffer.data + t->stroff);
                for (uint32_t j = 0; j < t->nsyms; ++j)
                    bindings += shda::bindLocalizationSymbol(names + symbols[j].n_un.n_strx, symbols[j].n_value);
            }
            command += c->cmdsize;
        }
        if (bindings != shda::localizationImportCount) { state("localization-symbols-missing"); return false; }
        if (!runtimePatcher || !shda::routeSelectorLocalization(*runtimePatcher)) {
            state("selector-localization-hook-failed"); return false;
        }
    }
    if (!shda::interfaceFor(dependencies[0].buffer, kernelInterface)) { state("kernel-interface-failed"); return false; }
    if (!redirectResources(dependencies[0].buffer)) { state("resource-symbols-missing"); return false; }
    KXLDContext *linker = nullptr;
    if (kxld_create_context(&linker, allocate, logLink, kKxldFlagDefault, CPU_TYPE_X86_64,
        CPU_SUBTYPE_X86_64_ALL, 4096) != KERN_SUCCESS) { state("linker-init-failed"); return false; }
    bool success = true;
    for (size_t m = 0; m < shda::moduleCount; ++m) {
        auto &module = shda::modules[m];
        state("linking", module.identifier);
        shda::Buffer file {static_cast<uint8_t *>(IOMalloc(module.executableSize)), module.executableSize};
        if (!file.data) { state("out-of-memory"); success = false; break; }
        memcpy(file.data, module.executable, file.size);
        KXLDDependency deps[16] {};
        for (size_t i = 0; i < dependencyCount; ++i) {
            deps[i].kext = dependencies[i].buffer.data;
            deps[i].kext_size = dependencies[i].buffer.size;
            deps[i].kext_name = const_cast<char *>(dependencies[i].id);
            deps[i].is_direct_dependency = i == 0;
            for (size_t j = 0; j < module.libraryCount; ++j)
                if (!strcmp(module.libraries[j], dependencies[i].id)) deps[i].is_direct_dependency = true;
        }
        deps[0].interface = kernelInterface.data;
        deps[0].interface_size = kernelInterface.size;
        deps[0].interface_name = const_cast<char *>("SimpleHDA.kernel-interface");
        Allocation allocation;
        lastLinkError[0] = 0;
        u_char *linked = nullptr;
        kxld_addr_t kmod = 0;
        auto result = shda::validateFile(file.data, file.size) ?
            kxld_link_file(linker, file.data, file.size, module.identifier, &allocation,
                deps, static_cast<unsigned>(dependencyCount), &linked, &kmod) : KERN_INVALID_ARGUMENT;
        file.clear();
        if (result != KERN_SUCCESS || !allocation.address || reinterpret_cast<uintptr_t>(linked) != allocation.address ||
            kmod < allocation.address || kmod > allocation.address + allocation.size - sizeof(kmod_info_t)) {
            if (allocation.address) freeImage(allocation.address, allocation.size);
            IOLog("SimpleHDA: link error %s result=0x%x\n", module.identifier, result);
            char detail[512];
            snprintf(detail, sizeof(detail), "%s result=0x%x %s", module.identifier, result, lastLinkError);
            state("link-failed", detail); success = false; break;
        }
        auto image = reinterpret_cast<mach_header_64 *>(allocation.address);
        if (!addDependency(module.identifier, image)) {
            freeImage(allocation.address, allocation.size);
            state("linked-snapshot-failed", module.identifier); success = false; break;
        }
        auto plist = OSDynamicCast(OSDictionary, OSUnserializeXML(module.plist));
        auto slide = kcSlide(0);
        auto info = reinterpret_cast<kmod_info_t *>(kmod);
        if (!plist || !shda::preparePrelink(linked, allocation.size, slide) ||
            !setNumber(plist, "_PrelinkExecutableLoadAddr", allocation.address - slide) ||
            !setNumber(plist, "_PrelinkExecutableSize", allocation.size) ||
            !setNumber(plist, "_PrelinkKmodInfo", kmod - slide)) {
            if (plist) plist->release();
            freeImage(allocation.address, allocation.size);
            state("prelink-preparation-failed", module.identifier); success = false; break;
        }
        info->address -= slide;
        state("registering", module.identifier);
        auto registered = registerImage(plist, false, 0);
        plist->release();
        if (!registered.value) {
            state("registration-failed", module.identifier); success = false; break;
        }
        resident[m] = registered.value;
        registered.value = nullptr;
        IOLog("SimpleHDA: registered and retained %s; IOKit will load it on matching\n", module.identifier);
    }
    kxld_destroy_context(linker);
    if (success) state("stack-registered");
    return success;
}
void catalog(bool matching) {
    if (!attempted && __atomic_load_n(&filesetThread, __ATOMIC_ACQUIRE) == current_thread() && kcHeader(2)) {
        attempted = true;
        state("system-kc-ready");
        run();
        releaseDependencies();
    }
    originalCatalog(matching);
}
OSReturn fileset(OSDictionary *request) {
    auto owner = current_thread();
    thread_t empty = nullptr;
    if (!__atomic_compare_exchange_n(&filesetThread, &empty, owner, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
        return originalFileset(request);
    auto result = originalFileset(request);
    __atomic_store_n(&filesetThread, nullptr, __ATOMIC_RELEASE);
    if (!attempted) state("system-kc-hook-not-reached");
    return result;
}
void patcherReady(void *, KernelPatcher &patcher) {
    runtimePatcher = &patcher;
    state("patcher-ready");
    int keepsyms = 0;
    if (!lilu_get_boot_args("keepsyms", &keepsyms, sizeof(keepsyms)) || keepsyms != 1) { state("requires-keepsyms-1"); return; }
    shda::configureHAL();
    const char *symbols[] {
        "__ZN6OSKext21withPrelinkedInfoDictEP12OSDictionaryb7kc_kind",
        "__ZN6OSKext22loadKextWithIdentifierEPKcbbhhP7OSArray",
        "__ZN6OSKext26isKextWithIdentifierLoadedEPKc",
        "_PE_get_kc_header", "_PE_get_kc_slide", "_kext_alloc", "_kext_free",
        "__ZN6OSKext24lookupKextWithIdentifierEPKc",
        "__ZN6OSKext13cancelRequestEjPPv",
        "__ZN6OSKext21lookupKextWithAddressEm",
        "__ZN6OSKext8isLoadedEv",
        "__ZN6OSKext9isStartedEv"
    };
    mach_vm_address_t addresses[12] {};
    for (size_t i = 0; i < 12; ++i) {
        addresses[i] = patcher.solveSymbol(KernelPatcher::KernelID, symbols[i]);
        if (!addresses[i]) { patcher.clearError(); state("kernel-symbol-missing", symbols[i]); return; }
    }
    registerImage = reinterpret_cast<Register>(addresses[0]);
    loadImage = reinterpret_cast<Load>(addresses[1]);
    isLoaded = reinterpret_cast<Loaded>(addresses[2]);
    kcHeader = reinterpret_cast<KCHeader>(addresses[3]);
    kcSlide = reinterpret_cast<KCSlide>(addresses[4]);
    allocateImage = reinterpret_cast<Allocate>(addresses[5]);
    freeImage = reinterpret_cast<Free>(addresses[6]);
    lookupImage = reinterpret_cast<Lookup>(addresses[7]);
    lookupAddress = reinterpret_cast<LookupAddress>(addresses[9]);
    imageLoaded = reinterpret_cast<Flag>(addresses[10]);
    imageStarted = reinterpret_cast<Flag>(addresses[11]);
    auto slot = shda::kextLockSlot(reinterpret_cast<const uint8_t *>(addresses[7]), addresses[7],
        reinterpret_cast<const uint8_t *>(addresses[8]), addresses[8], instructionLength);
    if (!slot || !kernelDataAddress(slot, sizeof(kextLock))) { releaseDependencies(); state("native-kext-lock-unverified"); return; }
    memcpy(&kextLock, reinterpret_cast<const void *>(slot), sizeof(kextLock));
    if (reinterpret_cast<uintptr_t>(kextLock) < 0xffffff0000000000ULL) { releaseDependencies(); state("native-kext-lock-missing"); return; }
    KernelPatcher::RouteRequest requests[] {
        {"__ZN6OSKext33sendAllKextPersonalitiesToCatalogEb", catalog, originalCatalog},
        {"__ZN6OSKext16loadFileSetKextsEP12OSDictionary", fileset, originalFileset}
    };
    if (!patcher.routeMultiple(KernelPatcher::KernelID, requests)) {
        releaseDependencies();
        patcher.clearError(); state("kernel-hook-failed"); return;
    }
    state("waiting-system-kc");
}
void start() {
    auto error = lilu.onPatcherLoad(patcherReady);
    if (error == LiluAPI::Error::NoError) state("waiting-lilu-patcher");
    else {
        ADDPR(startSuccess) = false;
        state("lilu-callback-registration-failed");
    }
}
const char *disable[] {"-shdaoff"};
const char *debug[] {"-shdadbg"};
}
void shda::publishState(const char *value, const char *detail) {
    IOLog("SimpleHDA: state=%s detail=%s\n", value, detail);
    auto root = IORegistryEntry::getRegistryRoot();
    if (root) {
        root->setProperty("SimpleHDAVersion", xStringify(MODULE_VERSION));
        root->setProperty("SimpleHDAState", value);
        root->setProperty("SimpleHDADetail", detail);
    }
}
PluginConfiguration ADDPR(config) {
    xStringify(PRODUCT_NAME), parseModuleVersion(xStringify(MODULE_VERSION)), LiluAPI::AllowNormal,
    disable, 1, debug, 1, nullptr, 0,
    KernelVersion::Tahoe, KernelVersion::Tahoe, start
};

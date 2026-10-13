//
//  SHDAMachO.cpp
//  SimpleHDA
//
//  Created by laobamac on 2026/8/25.
//  Copyright © 2026 laobamac. All rights reserved.
//
#include "SHDAMachO.hpp"
#include "SHDAAlloc.h"
#include <mach-o/nlist.h>
#if KERNEL
#include <libkern/libkern.h>
#else
#include <string.h>
#endif

namespace {
constexpr size_t limit = 128U * 1024U * 1024U;
bool range(uint64_t off, uint64_t length, uint64_t size) {
    return off <= size && length <= size - off;
}
bool headerOK(const mach_header_64 *h) {
    if (!h || h->magic != MH_MAGIC_64 || h->cputype != CPU_TYPE_X86_64 ||
        h->sizeofcmds > 65536 || h->ncmds > 1024) return false;
    auto p = reinterpret_cast<const uint8_t *>(h + 1);
    size_t left = h->sizeofcmds;
    for (uint32_t i = 0; i < h->ncmds; ++i) {
        if (left < sizeof(load_command)) return false;
        auto c = reinterpret_cast<const load_command *>(p);
        if (c->cmdsize < sizeof(*c) || c->cmdsize > left || (c->cmdsize & 7)) return false;
        if (c->cmd == LC_SEGMENT_64) {
            auto s = reinterpret_cast<const segment_command_64 *>(c);
            if (c->cmdsize < sizeof(*s) || s->nsects > 255 ||
                sizeof(*s) + s->nsects * sizeof(section_64) > c->cmdsize ||
                s->vmaddr > UINT64_MAX - s->vmsize) return false;
        }
        p += c->cmdsize;
        left -= c->cmdsize;
    }
    return left == 0;
}
}
namespace shda {
void Buffer::clear() {
    shda_free(data, size);
    data = nullptr;
    size = 0;
}
const mach_header_64 *findEntry(const mach_header_64 *h, const char *id) {
    if (!headerOK(h) || h->filetype != MH_FILESET) return nullptr;
    auto p = reinterpret_cast<const uint8_t *>(h + 1);
    for (uint32_t i = 0; i < h->ncmds; ++i) {
        auto c = reinterpret_cast<const load_command *>(p);
        if (c->cmd == LC_FILESET_ENTRY && c->cmdsize >= sizeof(fileset_entry_command)) {
            auto e = reinterpret_cast<const fileset_entry_command *>(c);
            auto off = e->entry_id.offset;
            if (off >= sizeof(*e) && off < c->cmdsize &&
                memchr(p + off, 0, c->cmdsize - off) && !strcmp(reinterpret_cast<const char *>(p + off), id))
                return reinterpret_cast<const mach_header_64 *>(e->vmaddr);
        }
        p += c->cmdsize;
    }
    return nullptr;
}
bool validateFile(const void *data, size_t size) {
    if (size < sizeof(mach_header_64) || size > limit) return false;
    auto h = static_cast<const mach_header_64 *>(data);
    if (!range(sizeof(*h), h->sizeofcmds, size) || !headerOK(h) || h->filetype != MH_KEXT_BUNDLE) return false;
    auto p = reinterpret_cast<const uint8_t *>(h + 1);
    bool syms = false;
    for (uint32_t i = 0; i < h->ncmds; ++i) {
        auto c = reinterpret_cast<const load_command *>(p);
        if (c->cmd == LC_SEGMENT_64) {
            auto s = reinterpret_cast<const segment_command_64 *>(c);
            if (!range(s->fileoff, s->filesize, size)) return false;
            auto sec = reinterpret_cast<const section_64 *>(s + 1);
            for (uint32_t j = 0; j < s->nsects; ++j) {
                if (!range(sec[j].addr - s->vmaddr, sec[j].size, s->vmsize) ||
                    !range(sec[j].reloff, static_cast<uint64_t>(sec[j].nreloc) * 8, size)) return false;
                auto type = sec[j].flags & SECTION_TYPE;
                if (type != S_ZEROFILL && type != S_GB_ZEROFILL &&
                    !range(sec[j].offset, sec[j].size, size)) return false;
            }
        } else if (c->cmd == LC_SYMTAB) {
            if (c->cmdsize != sizeof(symtab_command)) return false;
            auto s = reinterpret_cast<const symtab_command *>(c);
            if (!range(s->symoff, static_cast<uint64_t>(s->nsyms) * sizeof(nlist_64), size) ||
                !range(s->stroff, s->strsize, size)) return false;
            auto n = reinterpret_cast<const nlist_64 *>(static_cast<const uint8_t *>(data) + s->symoff);
            auto strings = static_cast<const char *>(data) + s->stroff;
            for (uint32_t j = 0; j < s->nsyms; ++j)
                if (n[j].n_un.n_strx >= s->strsize ||
                    !memchr(strings + n[j].n_un.n_strx, 0, s->strsize - n[j].n_un.n_strx)) return false;
            syms = true;
        } else if (c->cmd == LC_DYSYMTAB) {
            if (c->cmdsize != sizeof(dysymtab_command)) return false;
            auto d = reinterpret_cast<const dysymtab_command *>(c);
            if (!range(d->extreloff, static_cast<uint64_t>(d->nextrel) * 8, size) ||
                !range(d->locreloff, static_cast<uint64_t>(d->nlocrel) * 8, size) ||
                !range(d->indirectsymoff, static_cast<uint64_t>(d->nindirectsyms) * 4, size)) return false;
        }
        p += c->cmdsize;
    }
    return syms;
}
bool snapshot(const mach_header_64 *h, Reader read, void *ctx, Buffer &out) {
    if (!headerOK(h) || !read || out.data) return false;
    const section_64 *sections[255] {};
    uint8_t remap[256] {};
    const segment_command_64 *linkedit = nullptr;
    const symtab_command *table = nullptr;
    size_t count = 0;
    auto p = reinterpret_cast<const uint8_t *>(h + 1);
    for (uint32_t i = 0; i < h->ncmds; ++i) {
        auto c = reinterpret_cast<const load_command *>(p);
        if (c->cmd == LC_SEGMENT_64) {
            auto s = reinterpret_cast<const segment_command_64 *>(c);
            if (!strcmp(s->segname, "__LINKEDIT")) linkedit = s;
            auto sec = reinterpret_cast<const section_64 *>(s + 1);
            for (uint32_t j = 0; j < s->nsects; ++j) {
                if (count == 255 || !range(sec[j].addr - s->vmaddr, sec[j].size, s->vmsize)) return false;
                sections[count++] = sec + j;
            }
        } else if (c->cmd == LC_SYMTAB && c->cmdsize == sizeof(symtab_command))
            table = reinterpret_cast<const symtab_command *>(c);
        p += c->cmdsize;
    }
    if (!linkedit || !table || table->nsyms > 1000000 || table->strsize > 32 * 1024 * 1024 ||
        !range(table->symoff - linkedit->fileoff, static_cast<uint64_t>(table->nsyms) * 16, linkedit->filesize) ||
        !range(table->stroff - linkedit->fileoff, table->strsize, linkedit->filesize)) return false;
    Buffer symbols {static_cast<uint8_t *>(shda_alloc(table->nsyms * sizeof(nlist_64))), table->nsyms * sizeof(nlist_64)};
    Buffer strings {static_cast<uint8_t *>(shda_alloc(table->strsize)), table->strsize};
    bool ok = false;
    do {
        if (!symbols.data || !strings.data ||
            !read(linkedit->vmaddr + table->symoff - linkedit->fileoff, symbols.data, symbols.size, ctx) ||
            !read(linkedit->vmaddr + table->stroff - linkedit->fileoff, strings.data, strings.size, ctx)) break;
        auto n = reinterpret_cast<nlist_64 *>(symbols.data);
        uint32_t kept = 0, selected = 0;
        size_t stringBytes = 1;
        size_t bytes = 0;
        for (uint32_t i = 0; i < table->nsyms; ++i) {
            auto type = n[i].n_type & N_TYPE;
            if ((n[i].n_type & N_STAB) || (type != N_SECT && type != N_ABS)) continue;
            auto off = n[i].n_un.n_strx;
            if (off >= strings.size || !memchr(strings.data + off, 0, strings.size - off)) goto done;
            auto name = reinterpret_cast<const char *>(strings.data + off);
            if (!strncmp(name, "__ZTV", 5)) {
                auto sec = n[i].n_sect;
                if (!sec || sec > count || type != N_SECT ||
                    !range(n[i].n_value - sections[sec - 1]->addr, 24, sections[sec - 1]->size)) goto done;
                if (!remap[sec]) {
                    if (sections[sec - 1]->size > limit - bytes - 16) goto done;
                    remap[sec] = ++selected;
                    bytes += (sections[sec - 1]->size + 23) & ~size_t(7);
                }
            }
            stringBytes += strlen(name) + 1;
            n[kept++] = n[i];
        }
        size_t headerSize = sizeof(*h) + selected * (sizeof(segment_command_64) + sizeof(section_64)) +
            sizeof(segment_command_64) + sizeof(symtab_command);
        size_t total = headerSize + bytes + kept * sizeof(nlist_64) + stringBytes;
        if (total > limit) break;
        out = {static_cast<uint8_t *>(shda_alloc(total)), total};
        if (!out.data) break;
        auto dst = reinterpret_cast<mach_header_64 *>(out.data);
        *dst = {MH_MAGIC_64, CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_ALL, MH_KEXT_BUNDLE,
            selected + 2, static_cast<uint32_t>(headerSize - sizeof(*h)), MH_NOUNDEFS, 0};
        auto command = out.data + sizeof(*h);
        size_t dataOffset = headerSize;
        uint8_t actualMap[256] {};
        uint32_t idx = 0;
        for (size_t i = 0; i < count; ++i) {
            if (!remap[i + 1]) continue;
            auto sec = sections[i];
            auto seg = reinterpret_cast<segment_command_64 *>(command);
            *seg = {};
            seg->cmd = LC_SEGMENT_64;
            seg->cmdsize = sizeof(*seg) + sizeof(section_64);
            memcpy(seg->segname, sec->segname, 16);
            seg->vmaddr = sec->addr;
            seg->vmsize = sec->size + 16;
            seg->fileoff = dataOffset;
            seg->filesize = sec->size + 16;
            seg->maxprot = seg->initprot = VM_PROT_READ;
            seg->nsects = 1;
            auto s = reinterpret_cast<section_64 *>(seg + 1);
            *s = *sec;
            s->offset = static_cast<uint32_t>(dataOffset);
            s->reloff = s->nreloc = s->reserved1 = s->reserved2 = s->reserved3 = 0;
            s->flags = S_REGULAR;
            s->size += 16;
            if (!read(sec->addr, out.data + dataOffset, sec->size, ctx)) goto done;
            actualMap[i + 1] = ++idx;
            dataOffset += (sec->size + 23) & ~size_t(7);
            command += seg->cmdsize;
        }
        auto seg = reinterpret_cast<segment_command_64 *>(command);
        *seg = {};
        seg->cmd = LC_SEGMENT_64;
        seg->cmdsize = sizeof(*seg);
        memcpy(seg->segname, "__LINKEDIT", 11);
        seg->vmaddr = linkedit->vmaddr;
        seg->vmsize = seg->filesize = kept * sizeof(nlist_64) + stringBytes;
        seg->fileoff = dataOffset;
        seg->maxprot = seg->initprot = VM_PROT_READ;
        auto t = reinterpret_cast<symtab_command *>(seg + 1);
        *t = {LC_SYMTAB, sizeof(*t), static_cast<uint32_t>(dataOffset), kept,
            static_cast<uint32_t>(dataOffset + kept * sizeof(nlist_64)), static_cast<uint32_t>(stringBytes)};
        size_t stringOffset = 1;
        for (uint32_t i = 0; i < kept; ++i) {
            auto name = reinterpret_cast<const char *>(strings.data + n[i].n_un.n_strx);
            if (!strncmp(name, "__ZTV", 5)) n[i].n_sect = actualMap[n[i].n_sect];
            else { n[i].n_type = (n[i].n_type & (N_EXT | N_PEXT)) | N_ABS; n[i].n_sect = NO_SECT; }
            auto length = strlen(name) + 1;
            memcpy(out.data + t->stroff + stringOffset, name, length);
            n[i].n_un.n_strx = static_cast<uint32_t>(stringOffset);
            stringOffset += length;
        }
        memcpy(out.data + t->symoff, n, kept * sizeof(nlist_64));
        ok = validateFile(out.data, out.size);
    } while (false);
done:
    symbols.clear();
    strings.clear();
    if (!ok) out.clear();
    return ok;
}
bool interfaceFor(const Buffer &symbols, Buffer &out) {
    if (out.data || !validateFile(symbols.data, symbols.size)) return false;
    auto h = reinterpret_cast<const mach_header_64 *>(symbols.data);
    auto p = reinterpret_cast<const uint8_t *>(h + 1);
    const symtab_command *table = nullptr;
    for (uint32_t i = 0; i < h->ncmds; ++i) {
        auto c = reinterpret_cast<const load_command *>(p);
        if (c->cmd == LC_SYMTAB) table = reinterpret_cast<const symtab_command *>(c);
        p += c->cmdsize;
    }
    if (!table) return false;
    const size_t hs = sizeof(*h) + sizeof(symtab_command);
    out = {static_cast<uint8_t *>(shda_alloc(hs + table->nsyms * sizeof(nlist_64) + table->strsize)),
        hs + table->nsyms * sizeof(nlist_64) + table->strsize};
    if (!out.data) return false;
    auto dst = reinterpret_cast<mach_header_64 *>(out.data);
    *dst = {MH_MAGIC_64, CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_ALL, MH_KEXT_BUNDLE,
        1, sizeof(symtab_command), MH_NOUNDEFS, 0};
    auto t = reinterpret_cast<symtab_command *>(dst + 1);
    *t = {LC_SYMTAB, sizeof(*t), static_cast<uint32_t>(hs), table->nsyms,
        static_cast<uint32_t>(hs + table->nsyms * sizeof(nlist_64)), table->strsize};
    memcpy(out.data + t->symoff, symbols.data + table->symoff, table->nsyms * sizeof(nlist_64));
    memcpy(out.data + t->stroff, symbols.data + table->stroff, table->strsize);
    auto n = reinterpret_cast<nlist_64 *>(out.data + t->symoff);
    for (uint32_t i = 0; i < t->nsyms; ++i) {
        n[i].n_type = N_UNDF | (n[i].n_type & (N_EXT | N_PEXT));
        n[i].n_sect = 0;
        n[i].n_value = 0;
    }
    return validateFile(out.data, out.size);
}
bool preparePrelink(void *data, size_t size, uint64_t slide) {
    if (!validateFile(data, size)) return false;
    auto h = static_cast<mach_header_64 *>(data);
    h->flags &= ~MH_DYLIB_IN_CACHE;
    auto p = reinterpret_cast<uint8_t *>(h + 1);
    for (uint32_t i = 0; i < h->ncmds; ++i) {
        auto c = reinterpret_cast<load_command *>(p);
        if (c->cmd == LC_SEGMENT_64) {
            auto seg = reinterpret_cast<segment_command_64 *>(c);
            if (seg->vmaddr < slide) return false;
            seg->vmaddr -= slide;
            auto s = reinterpret_cast<section_64 *>(seg + 1);
            for (uint32_t j = 0; j < seg->nsects; ++j) s[j].addr -= slide;
        } else if (c->cmd == LC_SYMTAB) {
            auto t = reinterpret_cast<symtab_command *>(c);
            auto n = reinterpret_cast<nlist_64 *>(static_cast<uint8_t *>(data) + t->symoff);
            for (uint32_t j = 0; j < t->nsyms; ++j)
                if (!(n[j].n_type & N_STAB)) n[j].n_value -= slide;
        } else if (c->cmd == LC_DYSYMTAB) {
            auto d = reinterpret_cast<dysymtab_command *>(c);
            if (d->nextrel || d->nlocrel) return false;
        }
        p += c->cmdsize;
    }
    return true;
}
}

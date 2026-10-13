//
//  SHDALocalization.hpp
//  SimpleHDA
//
//  Created by laobamac on 2026/8/28.
//  Copyright © 2026 laobamac. All rights reserved.
//
#ifndef SHDA_LOCALIZATION_HPP
#define SHDA_LOCALIZATION_HPP
#include <stdint.h>
class KernelPatcher;
namespace shda {
inline constexpr const char *localizationImports[] {
    "__ZN13IOAudioDevice13setDeviceNameEPKc",
    "__ZN13IOAudioDevice18setDeviceShortNameEPKc",
    "__ZN13IOAudioDevice19setManufacturerNameEPKc",
    "__ZN13IOAudioDevice18setDeviceModelNameEPKc",
    "__ZN13IOAudioEngine14setDescriptionEPKc",
    "__ZN14IOAudioControl14setChannelNameEPKc",
    "__ZN22IOAudioSelectorControl21addAvailableSelectionEiPKc",
    "__ZN22IOAudioSelectorControl21addAvailableSelectionEiP8OSString",
    "__ZN22IOAudioSelectorControl21addAvailableSelectionEiP8OSStringPKcP8OSObject",
    "__ZN22IOAudioSelectorControl25replaceAvailableSelectionEiPKc",
    "__ZN22IOAudioSelectorControl25replaceAvailableSelectionEiP8OSString",
    "__ZN22IOAudioSelectorControl19createInputSelectorEijPKcj",
    "__ZN22IOAudioSelectorControl20createOutputSelectorEijPKcj",
    "__ZN22IOAudioSelectorControl6createEijPKcjjj",
    "__ZN22IOAudioSelectorControl4initEijPKcjjjP12OSDictionary"
};
inline constexpr unsigned localizationImportCount = sizeof(localizationImports) / sizeof(localizationImports[0]);
bool selectLocale(const char *);
const char *selectedLocale();
const char *localized(const char *);
void configureLocalization();
bool bindLocalizationSymbol(const char *, uint64_t &);
bool routeSelectorLocalization(KernelPatcher &);
}
#endif

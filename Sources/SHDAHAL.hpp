//
//  SHDAHAL.hpp
//  SimpleHDA
//
//  Created by laobamac on 2026/8/23.
//  Copyright © 2026 laobamac. All rights reserved.
//
#ifndef SHDA_HAL_HPP
#define SHDA_HAL_HPP
#include <IOKit/IORegistryEntry.h>
namespace shda {
bool configureHAL();
bool setAudioProperty(IORegistryEntry *entry, const char *key, const char *value);
}
#endif

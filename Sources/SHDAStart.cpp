//
//  SHDAStart.cpp
//  SimpleHDA
//
//  Created by laobamac on 2026/8/16.
//  Copyright © 2026 laobamac. All rights reserved.
//
#include <Headers/kern_api.hpp>
#include <Headers/plugin_start.hpp>
#include "SHDAStart.hpp"

extern PluginConfiguration ADDPR(config);
extern bool ADDPR(startSuccess);

EXPORT extern "C" kern_return_t ADDPR(kern_start)(kmod_info_t *, void *) {
    ADDPR(startSuccess) = false;
    shda::publishState("bootstrap", xStringify(MODULE_VERSION));
    lilu_get_boot_args("liludelay", &ADDPR(debugPrintDelay), sizeof(ADDPR(debugPrintDelay)));
    auto error = lilu.requestAccess();
    if (error != LiluAPI::Error::NoError) {
        char detail[96];
        snprintf(detail, sizeof(detail), "Lilu requestAccess returned %d", static_cast<int>(error));
        shda::publishState(error == LiluAPI::Error::TooLate ? "lilu-api-too-late" : "lilu-api-unavailable", detail);
        return KERN_SUCCESS;
    }
    error = lilu.shouldLoad(ADDPR(config).product, ADDPR(config).version, ADDPR(config).runmode,
        ADDPR(config).disableArg, ADDPR(config).disableArgNum, ADDPR(config).debugArg,
        ADDPR(config).debugArgNum, ADDPR(config).betaArg, ADDPR(config).betaArgNum,
        ADDPR(config).minKernel, ADDPR(config).maxKernel, ADDPR(debugEnabled));
    if (error == LiluAPI::Error::NoError) {
        ADDPR(startSuccess) = true;
        ADDPR(config).pluginStart();
    } else {
        char detail[96];
        snprintf(detail, sizeof(detail), "Lilu shouldLoad returned %d", static_cast<int>(error));
        shda::publishState("plugin-disabled", detail);
    }
    lilu.releaseAccess();
    return KERN_SUCCESS;
}

EXPORT extern "C" kern_return_t ADDPR(kern_stop)(kmod_info_t *, void *) {
    return ADDPR(startSuccess) ? KERN_FAILURE : KERN_SUCCESS;
}

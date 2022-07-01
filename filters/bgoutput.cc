/*
 * Copyright (c) 2022 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

#include "filter.hh"

extern "C" {

static PluginAPI *theAPI = nullptr;

__declspec(dllexport) const char *FILTERAPI init(PluginAPI *api, int *type) {
    theAPI = api;
    *type = 1;
    return "Blue Giant & Birth only";
}

__declspec(dllexport) void FILTERAPI output(const Galaxy *galaxy) {
    for (const auto *star: galaxy->stars) {
        if (star->index == 0 || (star->type == EStarType::GiantStar && star->spectr >= ESpectrType::B)) {
            theAPI->output(star);
        }
    }
}

}

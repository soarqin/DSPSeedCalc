/*
 * Copyright (c) 2024 Soar Qin<soarchin@gmail.com>
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 */

namespace dspugen {

struct Settings {
    bool hasPlanets = false;
    bool birthOnly = false;
    bool genName = false;
    bool noPosition = false;
};

extern Settings settings;

}

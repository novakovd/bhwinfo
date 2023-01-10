/**
 * Copyright 2023 novakovd (danilnovakov@gmail.com)
 * Copyright 2021 Aristocratos (jakob@qvantnet.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <filesystem>
#include <unistd.h>
#include <deque>
#include "../inc/robin_hood.h"

using std::string;
using std::array;
using std::deque;
using std::vector;

namespace fs = std::filesystem;
namespace rh = robin_hood;

namespace shared {
    fs::path proc_path, passwd_path;
    long page_size, clk_tck, core_count;

    struct cpu_info {
        rh::unordered_flat_map<string, deque<long long>> cpu_percent = {
                {"total", {}},
                {"user", {}},
                {"nice", {}},
                {"system", {}},
                {"idle", {}},
                {"iowait", {}},
                {"irq", {}},
                {"softirq", {}},
                {"steal", {}},
                {"guest", {}},
                {"guest_nice", {}}
        };
        vector<deque<long long>> core_percent;
        vector<deque<long long>> temp;
        long long temp_max = 0;

        /**
         * last one, five, and 10 minute periods
         */
        array<float, 3> load_avg;
    };

    inline void init() {
        // Shared global variables init
        proc_path =
                (fs::is_directory(fs::path("/proc")) and access("/proc", R_OK) != -1) ? "/proc" : "";

        if (proc_path.empty())
            throw std::runtime_error("Proc filesystem not found or no permission to read from it!");

        passwd_path =
                (fs::is_regular_file(fs::path("/etc/passwd")) and access("/etc/passwd", R_OK) != -1) ? "/etc/passwd" : "";

        core_count = sysconf(_SC_NPROCESSORS_ONLN);

        if (core_count < 1) {
            core_count = sysconf(_SC_NPROCESSORS_CONF);
            if (core_count < 1) {
                core_count = 1;
            }
        }

        page_size = sysconf(_SC_PAGE_SIZE);

        if (page_size <= 0) {
            page_size = 4096;
        }

        clk_tck = sysconf(_SC_CLK_TCK);

        if (clk_tck <= 0) {
            clk_tck = 100;
        }
    }
}
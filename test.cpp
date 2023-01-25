/**
 * Copyright 2023 novakovd (danilnovakov@gmail.com)
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

#include <ncurses.h>
#include <iostream>
#include "thread"
#include <chrono>
#include "src/collect.hpp"

using std::cout;
using std::endl;
using std::thread;

void set_interval(auto function, int interval) {
    thread t([&]() {
        for(;;) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            function();
        }
    });

    t.join();
}

void draw_border() {
    printw("\n------------------------------------- \n");
}

int main() {
    auto cpu_data_collector = cpu::DataCollector{};

    initscr();

    set_interval([&]() {
        cpu::Data cpu_data = cpu_data_collector.collect();

        auto freq = cpu_data.get_cpu_frequency();
        auto load_avg = cpu_data.get_average_load();
        auto core_load = cpu_data.get_core_load();

        printw("%s %f %s", "CPU frequency:", freq.value, freq.units.c_str());

        draw_border();

        printw("%s %s", "CPU name:", cpu_data.get_cpu_mame().c_str());

        draw_border();

        printw("%s %f %f %f", "CPU average load:", load_avg.one_min, load_avg.five_min, load_avg.fifteen_min);

        draw_border();

        printw("%s %ld%s","CPU temp:", cpu_data.get_cpu_temp(), "°C");

        draw_border();

        printw("%s %d", "CPU core count:", cpu_data.get_core_count());

        draw_border();

        printw("%s %lld%s", "CPU load:", cpu_data.get_cpu_load_percent(), "%");

        draw_border();

        printw("CPU cores load: \n");

        int n = 0;

        while (n < cpu_data.get_core_count()) {
            printw("%s%d% lld%s", "C", n, core_load.at(n).back(), "\n");

            n++;
        }

        refresh();
        clear();
    }, 1000);

    endwin();

    return 0;
}
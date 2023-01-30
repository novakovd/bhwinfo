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

void inline printf_border() {
   printf("\n------------------------------------- \n");
}

template<typename... Args>
void printf_with_border(const char* fmt , Args... args) {
   printf(fmt, args...);

   printf_border();
}

int main() {
    auto cpu_data_collector = cpu::DataCollector{};

    set_interval([&]() {
        system("clear");

        cpu::Data cpu_data = cpu_data_collector.collect();

        auto freq = cpu_data.get_cpu_frequency();
        auto load_avg = cpu_data.get_average_load();
        auto core_load = cpu_data.get_core_load();

       printf_with_border("%s %f %s", "CPU frequency:", freq.value, freq.units.c_str());

       printf_with_border("%s %s", "CPU name:", cpu_data.get_cpu_mame().c_str());

       printf_with_border("%s %f %f %f", "CPU average load:", load_avg.one_min, load_avg.five_min, load_avg.fifteen_min);

       printf_with_border("%s %ld%s","CPU temp:", cpu_data.get_cpu_temp(), "°C");

       printf_with_border("%s %d", "CPU core count:", cpu_data.get_core_count());

       printf_with_border("%s %lld%s", "CPU load:", cpu_data.get_cpu_load_percent(), "%");

       printf("CPU cores load: \n");

        for (int i = 0; i < cpu_data.get_core_count(); i++) {
            if(i > 0)printf(" ");

           printf("%s%d=%lld", "C", i, core_load.at(i));

            if(i != cpu_data.get_core_count() - 1)printf(" |");
        }

       printf_border();
    }, 1000);

    return 0;
}
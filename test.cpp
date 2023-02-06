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
#include "src/libhwinfo.hpp"

using std::cout;
using std::endl;
using std::thread;

void set_interval(auto function, int interval) {
    thread t([&]() {
        for (;;) {
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
void printf_with_new_line(const char *fmt, Args... args) {
    printf(fmt, args...);

    printf("\n");
}

template<typename... Args>
void printf_with_border(const char *fmt, Args... args) {
    printf(fmt, args...);

    printf_border();
}

int main() {
    auto cpu_data_collector = cpu::DataCollector{};
    auto mem_data_collector = mem::DataCollector{};

    set_interval([&]() {
        system("clear");

        printf("**************************** CPU INFO ****************************\n");

        cpu::Data cpu_data = cpu_data_collector.collect();

        auto freq = cpu_data.get_cpu_frequency();
        auto load_avg = cpu_data.get_average_load();
        auto core_load = cpu_data.get_core_load();
        auto cpu_usage = cpu_data.get_cpu_usage();

        printf_with_border("%s %f %s", "CPU frequency:", freq.value, freq.units.c_str());

        printf_with_border("%s %s", "CPU name:", cpu_data.get_cpu_mame().c_str());

        printf_with_border("%s %f %f %f", "CPU average load:", load_avg.one_min, load_avg.five_min,
                           load_avg.fifteen_min);

        printf_with_new_line("%s %ld%s", "CPU temp:", cpu_data.get_cpu_temp(), "°C");
        printf_with_border("%s %ld%s", "CPU critical temp:", cpu_data.get_cpu_critical_temperature(), "°C");

        printf_with_border("%s %d", "CPU core count:", cpu_data.get_core_count());

        printf_with_new_line("%s %lld%s", "CPU total:", cpu_usage.total, "%");
        printf_with_new_line("%s %lld%s", "CPU user:", cpu_usage.user, "%");
        printf_with_new_line("%s %lld%s", "CPU nice:", cpu_usage.nice, "%");
        printf_with_new_line("%s %lld%s", "CPU system:", cpu_usage.system, "%");
        printf_with_new_line("%s %lld%s", "CPU idle:", cpu_usage.idle, "%");
        printf_with_new_line("%s %lld%s", "CPU iowait:", cpu_usage.iowait, "%");
        printf_with_new_line("%s %lld%s", "CPU irq:", cpu_usage.irq, "%");
        printf_with_new_line("%s %lld%s", "CPU softirq:", cpu_usage.softirq, "%");
        printf_with_new_line("%s %lld%s", "CPU steal:", cpu_usage.steal, "%");
        printf_with_new_line("%s %lld%s", "CPU guest:", cpu_usage.guest, "%");
        printf_with_border("%s %lld%s", "CPU guest_nice:", cpu_usage.guest_nice, "%");

        for (int core_number = 0; core_number < cpu_data.get_core_count(); core_number++) {
            printf("%s%d: %lld", "Core", core_number, core_load.at(core_number));

            if (core_number != cpu_data.get_core_count() - 1) printf("\n");
        }

        printf("\n**************************** MEM INFO ****************************\n");

        mem::Data mem_data = mem_data_collector.collect();

        printf_with_border("%s%f GB", "Total RAM amount: ", mem_data.get_total_ram_amount().to_gigabytes());
        printf_with_border("%s%f GB", "Used RAM amount: ", mem_data.get_used_ram_amount().to_gigabytes());
        printf_with_border("%s%f GB", "Available RAM amount: ", mem_data.get_available_ram_amount().to_gigabytes());
        printf_with_border("%s%f GB", "Cached RAM amount: ", mem_data.get_cached_ram_amount().to_gigabytes());
        printf_with_border("%s%f GB", "Free RAM amount: ", mem_data.get_free_ram_amount().to_gigabytes());
    }, 1000);

    return 0;
}
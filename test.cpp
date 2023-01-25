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

void setInterval(auto function, int interval) {
    thread t([&]() {
        for(;;) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            function();
        }
    });

    t.join();
}

int main() {
    auto cpu_data_collector = cpu::DataCollector{};

    setInterval([&]() {
        system("clear");

        cpu::Data cpu_data = cpu_data_collector.collect();

        auto freq = cpu_data.get_cpu_frequency();
        auto load_avg = cpu_data.get_average_load();
        auto core_load = cpu_data.get_core_load();

        cout << "================= CPU frequency =================" << endl;
        cout << freq.value << " " << freq.units << endl;

        cout << "================= CPU name =================" << endl;
        cout << cpu_data.get_cpu_mame() << endl;

        cout << "================= CPU average load =================" << endl;
        cout << load_avg.one_min << " " << load_avg.five_min << " " << load_avg.fifteen_min << endl;

        cout << "================= CPU temp =================" << endl;
        cout << cpu_data.get_cpu_temp() << "°C" << endl;

        cout << "================= CPU core count =================" << endl;
        cout << cpu_data.get_core_count() << endl;

        cout << "================= CPU load =================" << endl;
        cout << cpu_data.get_cpu_load_percent() << "%" << endl;

        cout << "================= CPU cores load =================" << endl;
        int n = 0;

        while (n < cpu_data.get_core_count()) {
            cout << "C" << n << ": " << core_load.at(n).back();
            cout << "  C" << n + 1 << ": " <<  core_load.at(n + 1).back() << endl;

            n = n + 2;
        }
    }, 1000);


    return 0;
}
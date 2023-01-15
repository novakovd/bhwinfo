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
    t.detach();
}

int main() {
    lib::init();

    setInterval([&]() {
        system("clear");

        cpu::GetCpuFrequencyResult freq = cpu::get_cpu_frequency();
        shared::cpu_info cpu = cpu::collect();

        cout << "================= CPU frequency =================" << endl;
        cout << freq.value << " " << freq.units << endl;

        cout << "================= CPU name =================" << endl;
        cout << cpu::get_cpu_mame() << endl;

        cout << "================= CPU average load =================" << endl;
        cout << cpu.load_avg[0] << " " << cpu.load_avg[1] << " " << cpu.load_avg[2] << endl;

        cout << "================= CPU temp =================" << endl;
        cout << cpu.temp.at(0).back() << "°C" << endl;

        cout << "================= CPU core count =================" << endl;
        cout << shared::core_count << endl;

        cout << "================= CPU load =================" << endl;
        cout << cpu.cpu_percent.at("total").back() << "%" << endl;

        cout << "================= CPU cores load =================" << endl;
        int n = 0;

        while (n < shared::core_count) {
            cout << "C" << n << ": " << cpu.core_percent.at(n).back();
            cout << "  C" << n + 1 << ": " << cpu.core_percent.at(n + 1).back() << endl;

            n = n + 2;
        }

    }, 1000);

    for(;;); // keep main thread active;
}
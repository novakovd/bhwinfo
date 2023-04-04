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
#include "bhwinfo.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

using std::cout;
using std::endl;
using std::thread;
using std::to_string;

namespace ui = ftxui;

void set_interval(auto function, int interval) {
    thread t([&]() {
        for (;;) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
            function();
        }
    });

    t.join();
}

int main() {
    auto cpu_data_collector = cpu::DataCollector{};
    auto mem_data_collector = mem::DataCollector{};

    set_interval([&]() {
        system("clear");

        cpu::Data cpu_data = cpu_data_collector.collect();

        auto freq = cpu_data.get_cpu_frequency();
        auto load_avg = cpu_data.get_average_load();
        auto core_load = cpu_data.get_core_load();
        auto cpu_usage = cpu_data.get_cpu_usage();

        mem::Data mem_data = mem_data_collector.collect();

        vector<ui::Element> cores_ui;

        for (int core_number = 0; core_number < cpu_data.get_core_count(); core_number++) {
            cores_ui.push_back(ui::hbox({ui::text("Core_" + to_string(core_number) + ": " + to_string(core_load.at(core_number)))}));
        }

        vector<ui::Element> disks_ui;

        for(const mem::StorageUnit& disk: mem_data.get_disks()) {
            disks_ui.push_back(
                ui::vbox({
                    ui::text(disk.get_handle()),
                    ui::text("Total space: " + to_string(disk.get_total().to_gigabytes())),
                    ui::text("Used space: " + to_string(disk.get_used().to_gigabytes())),
                    ui::text("Free space: " + to_string(disk.get_free().to_gigabytes())),
                    ui::text("Used space percent: " + to_string(disk.get_used_percent()) + "%"),
                    ui::text("Free space percent: " + to_string(disk.get_free_percent())  + "%"),
                    ui::text("FS type: " + disk.get_fs_type()),
                    ui::text("Path: " + disk.get_path().string()),
                    ui::text("IO read: " + to_string(disk.get_io_read())),
                    ui::text("IO write: " + to_string(disk.get_io_write())),
                    ui::text("IO activity: " + to_string(disk.get_io_activity()))
                }) | ui::border
            );
        }

        ui::Element document =
                ui::vbox({
                    ui::hbox({
                        ui::vbox({
                            ui::hbox({ui::text("CPU frequency: " + to_string(freq.get_value()) + freq.get_units())}) | ui::border,
                            ui::hbox({ui::text("CPU name: " + cpu_data.get_cpu_mame())}) | ui::border,
                            ui::hbox({ui::text(
                                  "CPU average load: "
                                  + to_string(load_avg.get_one_min())
                                  + to_string(load_avg.get_five_min())
                                  + to_string(load_avg.get_fifteen_min())
                            )}) | ui::border,
                            ui::hbox({ui::text("CPU temp: " + to_string(cpu_data.get_cpu_temp()) + "°C")}) | ui::border,
                            ui::hbox({ui::text("CPU critical temp: " + to_string(cpu_data.get_cpu_critical_temperature()) + "°C")}) | ui::border,
                            ui::hbox({ui::text("CPU core count: " + to_string(cpu_data.get_core_count()))}) | ui::border,
                        }),
                        ui::vbox(cores_ui) | ui::border,
                        ui::vbox({
                            ui::text("CPU total: " + to_string(cpu_usage.get_total_percent()) + "%"),
                            ui::text("CPU user: " + to_string(cpu_usage.get_user_percent()) + "%"),
                            ui::text("CPU nice: "+ to_string(cpu_usage.get_nice_percent()) + "%"),
                            ui::text("CPU system: " + to_string(cpu_usage.get_system_percent()) + "%"),
                            ui::text("CPU idle: " + to_string(cpu_usage.get_idle_percent()) + "%"),
                            ui::text("CPU iowait: " + to_string(cpu_usage.get_iowait_percent()) + "%"),
                            ui::text("CPU irq: " + to_string(cpu_usage.get_irq_percent()) + "%"),
                            ui::text("CPU softirq: " + to_string(cpu_usage.get_softirq_percent()) + "%"),
                            ui::text("CPU steal: " + to_string(cpu_usage.get_steal_percent()) + "%"),
                            ui::text("CPU guest: " + to_string(cpu_usage.get_guest_percent()) + "%"),
                            ui::text("CPU guest_nice: " + to_string(cpu_usage.get_guest_nice_percent()) + "%"),
                        }) | ui::border,
                        ui::vbox({
                            ui::text("Total RAM amount: " + to_string(mem_data.get_total_ram_amount().to_gigabytes())),
                            ui::text(
                                "Used RAM amount: "
                                + to_string(mem_data.get_used_ram_amount().to_gigabytes())
                                + " - "
                                + to_string(mem_data.get_used_ram_amount().to_percent())
                                + "%"
                            ),
                            ui::text(
                                "Available RAM amount: "
                                + to_string(mem_data.get_available_ram_amount().to_gigabytes())
                                + " - "
                                + to_string(mem_data.get_available_ram_amount().to_percent())
                                + "%"
                            ),
                            ui::text(
                                "Cached RAM amount: "
                                 + to_string(mem_data.get_cached_ram_amount().to_gigabytes())
                                 + " - "
                                 + to_string(mem_data.get_cached_ram_amount().to_percent())
                                 + "%"
                            ),
                            ui::text(
                                "Free RAM amount: "
                                + to_string(mem_data.get_free_ram_amount().to_gigabytes())
                                + " - "
                                + to_string(mem_data.get_free_ram_amount().to_percent())
                                + "%"
                            ),
                        }) | ui::border
                    }),
                    ui::hbox({
                        ui::hbox(disks_ui)
                    })
                });

        auto screen = ui::Screen::Create(
                ui::Dimension::Full(),
                ui::Dimension::Fit(document)
        );

        Render(screen, document);
        screen.Print();
    }, 1000);

    return 0;
}
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
#include <numeric>
#include "cmath"
#include "ut.hpp"

using std::string;
using std::cmp_less;
using std::max;
using std::cmp_equal;
using std::cmp_greater;
using std::cmp_less;
using std::clamp;
using std::numeric_limits;
using std::round;
using std::streamsize;

using namespace std::literals;

namespace fs = std::filesystem;
namespace rh = robin_hood;

namespace cpu {
    struct GetCpuFrequencyResult {
        double value{}; // defaults to 0
        string units;
    };

    struct GetCpuAvgLoadResult {
        double one_min{};
        double five_min{};
        double fifteen_min{}; // 10 min for RedHat
    };

    struct Sensor {
        fs::path path;
        string label;
        int64_t temp{}; // defaults to 0
        int64_t high{}; // defaults to 0
        int64_t crit{}; // defaults to 0
    };

    rh::unordered_flat_map<string, long long> cpu_old = {
            {"totals", 0},
            {"idles", 0},
            {"user", 0},
            {"nice", 0},
            {"system", 0},
            {"idle", 0},
            {"iowait", 0},
            {"irq", 0},
            {"softirq", 0},
            {"steal", 0},
            {"guest", 0},
            {"guest_nice", 0}
    };

    const array time_names {
            "user"s, "nice"s, "system"s, "idle"s, "iowait"s,
            "irq"s, "softirq"s, "steal"s, "guest"s, "guest_nice"s
    };

    fs::path freq_path = "/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq";
    string cpu_sensor;
    vector<string> core_sensors;
    vector<long long> core_old_totals;
    vector<long long> core_old_idles;
    vector<string> available_sensors = {"Auto"};
    vector<string> available_fields;
    rh::unordered_flat_map<int, int> core_mapping;
    rh::unordered_flat_map<string, Sensor> found_sensors;
    shared::cpu_info current_cpu;
    bool cpu_temp_only{};   // defaults to false
    bool is_initilized{};   // defaults to false
    bool got_sensors;
    int width = 20;

    bool get_sensors() {
        bool got_cpu = false, got_core_temp = false;

        vector<fs::path> search_paths;

        try {
            //? Setup up paths to search for sensors
            if (fs::exists(fs::path("/sys/class/hwmon")) and access("/sys/class/hwmon", R_OK) != -1) {
                for (const auto& dir :fs::directory_iterator(fs::path("/sys/class/hwmon"))) {
                    fs::path add_path = fs::canonical(dir.path());

                    if (ut::vec::contains(search_paths, add_path)
                        or ut::vec::contains(search_paths, add_path / "device")) continue;

                    if (ut::str::contains(add_path, "coretemp"))
                        got_core_temp = true;

                    for (const auto & file : fs::directory_iterator(add_path)) {
                        if (string(file.path().filename()) == "device") {
                            for (const auto & dev_file : fs::directory_iterator(file.path())) {
                                string dev_filename = dev_file.path().filename();

                                if (dev_filename.starts_with("temp") and dev_filename.ends_with("_input")) {
                                    search_paths.push_back(file.path());
                                    break;
                                }
                            }
                        }

                        string filename = file.path().filename();

                        if (filename.starts_with("temp") and filename.ends_with("_input")) {
                            search_paths.push_back(add_path);
                            break;
                        }
                    }
                }
            }

            if (not got_core_temp and fs::exists(fs::path("/sys/devices/platform/coretemp.0/hwmon"))) {
                for (auto& d :fs::directory_iterator(fs::path("/sys/devices/platform/coretemp.0/hwmon"))) {
                    fs::path add_path = fs::canonical(d.path());

                    for (const auto& file : fs::directory_iterator(add_path)) {
                        string filename = file.path().filename();

                        if (filename.starts_with("temp") and filename.ends_with("_input") and not ut::vec::contains(search_paths, add_path)) {
                            search_paths.push_back(add_path);
                            got_core_temp = true;

                            break;
                        }
                    }
                }
            }

            //? Scan any found directories for temperature sensors
            if (not search_paths.empty()) {
                for (const auto& path : search_paths) {
                    const string pname = ut::file::read(path / "name", path.filename());
                    for (const auto & file : fs::directory_iterator(path)) {

                        const string file_suffix = "input";
                        const int file_id = atoi(file.path().filename().c_str() + 4); // skip "temp" prefix
                        string file_path = file.path();

                        if (!ut::str::contains(file_path, file_suffix)) {
                            continue;
                        }

                        const string basepath =
                                file_path.erase(file_path.find(file_suffix), file_suffix.length());
                        const string label =
                                ut::file::read(fs::path(basepath + "label"), "temp" + std::to_string(file_id));
                        const string sensor_name =
                                pname + "/" + label;
                        const int64_t temp =
                                stol(ut::file::read(fs::path(basepath + "input"), "0")) / 1000;
                        const int64_t high =
                                stol(ut::file::read(fs::path(basepath + "max"), "80000")) / 1000;
                        const int64_t crit =
                                stol(ut::file::read(fs::path(basepath + "crit"), "95000")) / 1000;

                        found_sensors[sensor_name] = {fs::path(basepath + "input"), label, temp, high, crit};

                        if (not got_cpu and (label.starts_with("Package id") or label.starts_with("Tdie"))) {
                            got_cpu = true;
                            cpu_sensor = sensor_name;
                        }
                        else if (label.starts_with("Core") or label.starts_with("Tccd")) {
                            got_core_temp = true;

                            if (not ut::vec::contains(core_sensors, sensor_name))
                                core_sensors.push_back(sensor_name);
                        }
                    }
                }
            }
            //? If no good candidate for cpu temp has been found scan /sys/class/thermal
            if (not got_cpu and fs::exists(fs::path("/sys/class/thermal"))) {
                const string rootpath = fs::path("/sys/class/thermal/thermal_zone");

                for (int i = 0; fs::exists(fs::path(rootpath + std::to_string(i))); i++) {
                    const fs::path basepath = rootpath + std::to_string(i);

                    if (not fs::exists(basepath / "temp")) continue;

                    const string label =ut::file::read(basepath / "type", "temp" + std::to_string(i));
                    const string sensor_name = "thermal" + std::to_string(i) + "/" + label;
                    const int64_t temp = stol(ut::file::read(basepath / "temp", "0")) / 1000;

                    int64_t high, crit;

                    for (int ii = 0; fs::exists(basepath / string("trip_point_" + std::to_string(ii) + "_temp")); ii++) {
                        const string trip_type =
                                ut::file::read(basepath / string("trip_point_" + std::to_string(ii) + "_type"));

                        if (not ut::type::is_in(trip_type, "high", "critical")) continue;

                        auto& val = (trip_type == "high" ? high : crit);

                        val = stol(ut::file::read(basepath / string("trip_point_" + std::to_string(ii) + "_temp"), "0")) / 1000;
                    }

                    if (high < 1) high = 80;
                    if (crit < 1) crit = 95;

                    found_sensors[sensor_name] = {basepath / "temp", label, temp, high, crit};
                }
            }

        }
        catch (...) {}

        if (not got_core_temp or core_sensors.empty()) {
            cpu_temp_only = true;
        }
        else {
            rng::sort(core_sensors, rng::less{});
            rng::stable_sort(core_sensors, [](const auto& a, const auto& b){
                return a.size() < b.size();
            });
        }

        if (cpu_sensor.empty() and not found_sensors.empty()) {
            for (const auto& [name, sensor] : found_sensors) {
                if (ut::str::contains(ut::str::to_lower(name), "cpu") or
                    ut::str::contains(ut::str::to_lower(name), "k10temp")
                        ) {
                    cpu_sensor = name;
                    break;
                }
            }
            if (cpu_sensor.empty()) {
                cpu_sensor = found_sensors.begin()->first;
                ut::logger::warning("No good candidate for cpu sensor found, using random from all found sensors.");
            }
        }

        return not found_sensors.empty();
    }

    void update_sensors() {
        if (cpu_sensor.empty()) return;

        const auto& cpu_s = cpu::cpu_sensor;

        found_sensors.at(cpu_s).temp = stol(ut::file::read(found_sensors.at(cpu_s).path, "0")) / 1000;
        current_cpu.temp.at(0).push_back(found_sensors.at(cpu_s).temp);
        current_cpu.temp_max = found_sensors.at(cpu_s).crit;
        if (current_cpu.temp.at(0).size() > 20) current_cpu.temp.at(0).pop_front();

        if (not cpu_temp_only) {
            vector<string> done;
            for (const auto& sensor : core_sensors) {
                if (ut::vec::contains(done, sensor)) continue;
                found_sensors.at(sensor).temp = stol(ut::file::read(found_sensors.at(sensor).path, "0")) / 1000;
                done.push_back(sensor);
            }
            for (const auto& [core, temp] : core_mapping) {
                if (cmp_less(core + 1, current_cpu.temp.size()) and cmp_less(temp, core_sensors.size())) {
                    current_cpu.temp.at(core + 1).push_back(found_sensors.at(core_sensors.at(temp)).temp);
                    if (current_cpu.temp.at(core + 1).size() > 20) current_cpu.temp.at(core + 1).pop_front();
                }
            }
        }
    }
}
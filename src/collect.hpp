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
#include <utility>
#include <any>
#include <unordered_map>
#include <algorithm>
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

namespace cpu {
    struct CpuFrequency {
        double value{}; // defaults to 0
        string units;
    };

    struct CpuAvgLoad {
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

    struct CpuInfo {
        std::unordered_map<string, deque<long long>> cpu_percent = {
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
        array<float, 3> load_avg{};
    };

    std::unordered_map<string, long long> CpuOld = {
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

    class StaticValuesAware {
    protected:
        string cpu_name;
        int core_count;
    };

    class Data : StaticValuesAware {
    private:
        long long cpu_load_percent;
        int64_t cpu_temp;
        CpuAvgLoad cpu_load_avg;
        vector<deque<long long>> core_load;
        CpuFrequency cpu_frequency;

    public:
        Data(
            long long cpu_load_percent,
            int64_t cpu_temp,
            CpuAvgLoad cpu_load_avg,
            vector<deque<long long>> core_load,
            CpuFrequency cpu_frequency,
            string cpu_name,
            int core_count
        ) {
            this->cpu_load_percent = cpu_load_percent;
            this->cpu_temp = cpu_temp;
            this->cpu_load_avg = cpu_load_avg;
            this->core_load = std::move(core_load);
            this->cpu_frequency = std::move(cpu_frequency);
            this->cpu_name = std::move(cpu_name);
            this->core_count = core_count;
        }

        [[nodiscard]] long long get_cpu_load_percent() const {
            return this->cpu_load_percent;
        }

        [[nodiscard]] int64_t get_cpu_temp() const {
            return this->cpu_temp;
        }

        CpuAvgLoad get_average_load() {
            return this->cpu_load_avg;
        }

        vector<deque<long long>> get_core_load() {
            return this->core_load;
        }

        CpuFrequency get_cpu_frequency() {
            return this->cpu_frequency;
        }

        string get_cpu_mame() {
            return this->cpu_name;
        }

        [[nodiscard]] int get_core_count() const {
            return this->core_count;
        };
    };

    class DataCollector : StaticValuesAware {
    public:
        DataCollector() {
            shared::init();

            if (not fs::exists(shared::freq_path) or access(shared::freq_path.c_str(), R_OK) == -1)
                shared::freq_path.clear();

            current_cpu.core_percent.insert(current_cpu.core_percent.begin(), shared::core_count, {});
            current_cpu.temp.insert(current_cpu.temp.begin(), shared::core_count + 1, {});
            core_old_totals.insert(core_old_totals.begin(),  shared::core_count, 0);
            core_old_idles.insert(core_old_idles.begin(),  shared::core_count, 0);

            for (auto& [field, vec] : current_cpu.cpu_percent) {
                if (not vec.empty()) available_fields.push_back(field);
            }

            got_sensors = get_sensors();

            for (const auto& [sensor, ignored] : found_sensors) {
                available_sensors.push_back(sensor);
            }

            cpu_name = get_cpu_mame();
            core_count = get_core_count();
        }
    private:
        string cpu_sensor;
        vector<string> core_sensors;
        vector<long long> core_old_totals;
        vector<long long> core_old_idles;
        vector<string> available_sensors = {"Auto"};
        vector<string> available_fields;
        std::unordered_map<int, int> core_mapping;
        std::unordered_map<string, Sensor> found_sensors;
        CpuInfo current_cpu;
        bool cpu_temp_only{};   // defaults to false
        bool got_sensors;
        const array<string ,10> time_names {
                "user"s, "nice"s, "system"s, "idle"s, "iowait"s,
                "irq"s, "softirq"s, "steal"s, "guest"s, "guest_nice"s
        };

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

            const auto& cpu_s = cpu_sensor;

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

        int get_core_count() {
            int core_count = sysconf(_SC_NPROCESSORS_ONLN);

            if (core_count < 1) {
                core_count = sysconf(_SC_NPROCESSORS_CONF);
                if (core_count < 1) {
                    core_count = 1;
                }
            }

            return core_count;
        }

        CpuFrequency get_cpu_frequency() {
            static int failed{}; // defaults to 0
            CpuFrequency result{};

            if (failed > 4)
                return result;

            try {
                double hz{}; // defaults to 0.0

                // Try to get freq from /sys/devices/system/cpu/cpufreq/policy first (faster)
                if (not shared::freq_path.empty()) {
                    hz = stod(ut::file::read(shared::freq_path, "0.0")) / 1000;

                    if (hz <= 0.0 and ++failed >= 2)
                        shared::freq_path.clear();
                }

                // If freq from /sys failed or is missing try to use /proc/cpuinfo
                if (hz <= 0.0) {
                    std::ifstream cpufreq(shared::proc_path / "cpuinfo");

                    if (cpufreq.good()) {
                        while (cpufreq.ignore(ut::maxStreamSize, '\n')) {
                            if (cpufreq.peek() == 'c') {
                                cpufreq.ignore(ut::maxStreamSize, ' ');
                                if (cpufreq.peek() == 'M') {
                                    cpufreq.ignore(ut::maxStreamSize, ':');
                                    cpufreq.ignore(1);
                                    cpufreq >> hz;
                                    break;
                                }
                            }
                        }
                    }
                }

                if (hz <= 1 or hz >= 1000000)
                    throw std::runtime_error("Failed to read /sys/devices/system/cpu/cpufreq/policy and /proc/cpuinfo.");

                if (hz >= 1000) {
                    if (hz >= 10000) result.value = round(hz / 1000);
                    else result.value = round(hz / 100) / 10.0;

                    result.units = "GHz";
                }
                else if (hz > 0) {
                    result.value = round(hz);
                    result.units = "MHz";
                }
            }
            catch (const std::exception& e) {
                if (++failed < 5)
                    return result;
                else {
                    ut::logger::warning("get_cpu_frequency() : " + string{e.what()});

                    return result;
                }
            }

            return result;
        }

        string get_cpu_mame() {
            string name;
            std::ifstream cpu_info(shared::proc_path / "cpuinfo");

            if (cpu_info.good()) {
                for (string instr; getline(cpu_info, instr, ':') and not instr.starts_with("model name");)
                    cpu_info.ignore(ut::maxStreamSize, '\n');

                if (cpu_info.bad()) return name;
                else if (not cpu_info.eof()) {
                    cpu_info.ignore(1);
                    getline(cpu_info, name);
                }
                else if (fs::exists("/sys/devices")) {
                    for (const auto& d : fs::directory_iterator("/sys/devices")) {
                        if (string(d.path().filename()).starts_with("arm")) {
                            name = d.path().filename();
                            break;
                        }
                    }
                    if (not name.empty()) {
                        auto name_vec = ut::str::split(name, '_');

                        if (name_vec.size() < 2) return ut::str::capitalize(name);
                        else
                            return
                                    ut::str::capitalize(name_vec.at(1))+
                                    (name_vec.size() > 2 ? ' '+
                                                           ut::str::capitalize(name_vec.at(2)) : "");
                    }

                }

                auto name_vec = ut::str::split(name);

                if ((ut::str::contains(name, "Xeon"s) or
                     ut::vec::contains(name_vec, "Duo"s)) and
                    ut::vec::contains(name_vec, "CPU"s)
                        ) {
                    auto cpu_pos = ut::vec::index(name_vec, "CPU"s);

                    if (cpu_pos < name_vec.size() - 1 and not name_vec.at(cpu_pos + 1).ends_with(')'))
                        name = name_vec.at(cpu_pos + 1);
                    else
                        name.clear();
                }
                else if (ut::vec::contains(name_vec, "Ryzen"s)) {
                    auto ryz_pos = ut::vec::index(name_vec, "Ryzen"s);

                    name = "Ryzen"	+ (ryz_pos < name_vec.size() - 1 ? ' ' + name_vec.at(ryz_pos + 1) : "")
                           + (ryz_pos < name_vec.size() - 2 ? ' ' + name_vec.at(ryz_pos + 2) : "");
                }
                else if (ut::str::contains(name, "Intel"s) and ut::vec::contains(name_vec, "CPU"s)) {
                    auto cpu_pos = ut::vec::index(name_vec, "CPU"s);

                    if (cpu_pos < name_vec.size() - 1 and not
                            name_vec.at(cpu_pos + 1).ends_with(')') and
                        name_vec.at(cpu_pos + 1) != "@")  name = name_vec.at(cpu_pos + 1);
                    else
                        name.clear();
                }
                else
                    name.clear();

                if (name.empty() and not name_vec.empty()) {
                    for (const auto& n : name_vec) {
                        if (n == "@") break;
                        name += n + ' ';
                    }

                    name.pop_back();

                    for (const auto& r : {"Processor", "CPU", "(R)", "(TM)", "Intel", "AMD", "Core"}) {
                        name = ut::str::replace(name, r, "");
                        name = ut::str::replace(name, "  ", " ");
                    }
                    name = ut::str::trim(name);
                }
            }

            return name;
        }

    public:
        Data collect() {
            auto& cpu = current_cpu;

            std::ifstream stream;

            try {
                //? Get cpu load averages from /proc/loadavg
                stream.open(shared::proc_path / "loadavg");

                if (stream.good()) {
                    stream >> cpu.load_avg[0] >> cpu.load_avg[1] >> cpu.load_avg[2];
                }
                stream.close();

                //? Get cpu total times for all cores from /proc/stat
                string cpu_name;
                stream.open(shared::proc_path / "stat");

                int i = 0;
                int target = shared::core_count;

                for (; i <= target or (stream.good() and stream.peek() == 'c'); i++) {
                    //? Make sure to add zero value for missing core values if at end of file
                    if ((not stream.good() or stream.peek() != 'c') and i <= target) {
                        if (i == 0) throw std::runtime_error("Failed to parse /proc/stat");
                        else {
                            //? Fix container sizes if new cores are detected
                            while (cmp_less(cpu.core_percent.size(), i)) {
                                core_old_totals.push_back(0);
                                core_old_idles.push_back(0);
                                cpu.core_percent.push_back({});
                            }
                            cpu.core_percent.at(i-1).push_back(0);
                        }
                    }
                    else {
                        if (i == 0) stream.ignore(ut::maxStreamSize, ' ');
                        else {
                            stream >> cpu_name;
                            int cpuNum = std::stoi(cpu_name.substr(3));
                            if (cpuNum >= target - 1) target = cpuNum + (stream.peek() == 'c' ? 2 : 1);

                            //? Add zero value for core if core number is missing from /proc/stat
                            while (i - 1 < cpuNum) {
                                //? Fix container sizes if new cores are detected
                                while (cmp_less(cpu.core_percent.size(), i)) {
                                    core_old_totals.push_back(0);
                                    core_old_idles.push_back(0);
                                    cpu.core_percent.push_back({});
                                }
                                cpu.core_percent[i-1].push_back(0);
                                if (cpu.core_percent.at(i-1).size() > 40) cpu.core_percent.at(i-1).pop_front();
                                i++;
                            }
                        }

                        //? Expected on kernel 2.6.3> : 0=user, 1=nice, 2=system, 3=idle, 4=iowait, 5=irq, 6=softirq, 7=steal, 8=guest, 9=guest_nice
                        vector<long long> times;
                        long long total_sum = 0;

                        for (uint64_t val; stream >> val; total_sum += val) {
                            times.push_back(val);
                        }
                        stream.clear();
                        if (times.size() < 4) throw std::runtime_error("Malformatted /proc/stat");

                        //? Subtract fields 8-9 and any future unknown fields
                        const long long totals = max(0ll, total_sum - (times.size() > 8 ? std::accumulate(times.begin() + 8, times.end(), 0) : 0));

                        //? Add iowait field if present
                        const long long idles = max(0ll, times.at(3) + (times.size() > 4 ? times.at(4) : 0));

                        //? Calculate values for totals from first line of stat
                        if (i == 0) {
                            const long long calc_totals = max(1ll, totals - CpuOld.at("totals"));
                            const long long calc_idles = max(1ll, idles - CpuOld.at("idles"));
                            CpuOld.at("totals") = totals;
                            CpuOld.at("idles") = idles;

                            //? Total usage of cpu
                            cpu.cpu_percent.at("total").push_back(clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll));

                            //? Populate cpu.cpu_percent with all fields from stat
                            for (int ii = 0; const auto& val : times) {
                                cpu.cpu_percent.at(time_names.at(ii)).push_back(clamp((long long)round((double)(val - CpuOld.at(time_names.at(ii))) * 100 / calc_totals), 0ll, 100ll));
                                CpuOld.at(time_names.at(ii)) = val;

                                if (++ii == 10) break;
                            }
                            continue;
                        }
                            //? Calculate cpu total for each core
                        else {
                            //? Fix container sizes if new cores are detected
                            while (cmp_less(cpu.core_percent.size(), i)) {
                                core_old_totals.push_back(0);
                                core_old_idles.push_back(0);
                                cpu.core_percent.push_back({});
                            }
                            const long long calc_totals = max(0ll, totals - core_old_totals.at(i-1));
                            const long long calc_idles = max(0ll, idles - core_old_idles.at(i-1));
                            core_old_totals.at(i-1) = totals;
                            core_old_idles.at(i-1) = idles;

                            cpu.core_percent.at(i-1).push_back(clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll));
                        }
                    }
                }

            }
            catch (const std::exception& e) {
                ut::logger::debug("collect() : " + string{e.what()});

                if (stream.bad()) throw std::runtime_error("Failed to read /proc/stat");
                else throw std::runtime_error("collect() : " + string{e.what()});
            }

            if (got_sensors)
                update_sensors();

            return Data {
                cpu.cpu_percent.at("total").back(),
                found_sensors.at( cpu_sensor).temp,
                CpuAvgLoad{cpu.load_avg[0], cpu.load_avg[1], cpu.load_avg[2]},
                cpu.core_percent,
                get_cpu_frequency(),
                cpu_name,
                core_count
            };
        }
    };
}
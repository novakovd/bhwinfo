#include <unistd.h>
#include <fstream>
#include <numeric>
#include "cmath"
#include "../include/cpu.hpp"

using std::round;
using std::clamp;
using std::max;

namespace cpu {
    CpuFrequency::CpuFrequency(const double& value, string units) : value(value), units(std::move(units)) {}

    const double& CpuFrequency::get_value() const {
        return value;
    }

    const string& CpuFrequency::get_units() const {
        return units;
    }

    CpuAvgLoad::CpuAvgLoad(
        const double& one_min,
        const double& five_min,
        const double& fifteen_min
    ) :
    one_min(one_min),
    five_min(five_min),
    fifteen_min(fifteen_min) {}

    const double& CpuAvgLoad::get_one_min() const {
        return one_min;
    }

    const double& CpuAvgLoad::get_five_min() const {
        return five_min;
    }

    const double& CpuAvgLoad::get_fifteen_min() const {
        return fifteen_min;
    }
    
    CpuUsage::CpuUsage(
        const long long int& total_percent,
        const long long int& user_percent,
        const long long int& nice_percent,
        const long long int& system_percent,
        const long long int& idle_percent,
        const long long int& iowait_percent,
        const long long int& irq_percent,
        const long long int& softirq_percent,
        const long long int& steal_percent,
        const long long int& guest_percent,
        const long long int& guest_nice_percent
    ) :
    total_percent(total_percent),
    user_percent(user_percent),
    nice_percent(nice_percent),
    system_percent(system_percent),
    idle_percent(idle_percent),
    iowait_percent(iowait_percent),
    irq_percent(irq_percent),
    softirq_percent(softirq_percent),
    steal_percent(steal_percent),
    guest_percent(guest_percent),
    guest_nice_percent(guest_nice_percent) {}

    const long long int& CpuUsage::get_total_percent() const {
        return total_percent;
    }

    const long long int& CpuUsage::get_user_percent() const {
        return user_percent;
    }

    const long long int& CpuUsage::get_nice_percent() const {
        return nice_percent;
    }

    const long long int& CpuUsage::get_system_percent() const {
        return system_percent;
    }

    const long long int& CpuUsage::get_idle_percent() const {
        return idle_percent;
    }

    const long long int& CpuUsage::get_iowait_percent() const {
        return iowait_percent;
    }

    const long long int& CpuUsage::get_irq_percent() const {
        return irq_percent;
    }

    const long long int& CpuUsage::get_softirq_percent() const {
        return softirq_percent;
    }

    const long long int& CpuUsage::get_steal_percent() const {
        return steal_percent;
    }

    const long long int& CpuUsage::get_guest_percent() const {
        return guest_percent;
    }

    const long long int& CpuUsage::get_guest_nice_percent() const {
        return guest_nice_percent;
    }

    StaticValuesAware::StaticValuesAware(
        const string& cpu_name, 
        int core_count, 
        long long int critical_temperature
    ) : 
    cpu_name(cpu_name), 
    core_count(core_count), 
    critical_temperature(critical_temperature) {}

    StaticValuesAware::StaticValuesAware() = default;

    Data::Data(
        const CpuUsage& cpu_usage,
        const int64_t& cpu_temp,
        const CpuAvgLoad& cpu_load_avg,
        const vector<long long>& core_load,
        const CpuFrequency& cpu_frequency,
        const string& cpu_name,
        const int& core_count,
        const long long& critical_temperature
    ) :
    StaticValuesAware(cpu_name, core_count, critical_temperature),
    cpu_usage(cpu_usage),
    cpu_temp(cpu_temp),
    cpu_load_avg(cpu_load_avg),
    core_load(core_load),
    cpu_frequency(cpu_frequency) {}

    const CpuUsage& Data::get_cpu_usage() const {
        return cpu_usage;
    }

    const int64_t& Data::get_cpu_temp() const {
        return cpu_temp;
    }

    const CpuAvgLoad& Data::get_average_load() {
        return cpu_load_avg;
    }

    const vector<long long>& Data::get_core_load() {
        return core_load;
    }

    const CpuFrequency& Data::get_cpu_frequency() {
        return cpu_frequency;
    }

    const string& Data::get_cpu_mame() {
        return cpu_name;
    }

    const int& Data::get_core_count() const {
        return core_count;
    }

    const long long& Data::get_cpu_critical_temperature() const {
        return critical_temperature;
    }

    DataCollector::DataCollector() {
        shared::init();

        if (not fs::exists(shared::freq_path) or access(shared::freq_path.c_str(), R_OK) == -1)
            shared::freq_path.clear();

        /** static values */
        cpu_name = get_cpu_mame();
        core_count = get_core_count();

        current_cpu.core_percent.insert(current_cpu.core_percent.begin(), core_count, {});
        core_old_totals.insert(core_old_totals.begin(),  core_count, 0);
        core_old_idles.insert(core_old_idles.begin(),  core_count, 0);

        got_sensors = get_sensors();

        for (const auto& [sensor, ignored] : found_sensors) {
            available_sensors.push_back(sensor);
        }
    }

    bool DataCollector::get_sensors() {
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
            }
        }

        return not found_sensors.empty();
    }

    void DataCollector::update_sensors() {
        if (cpu_sensor.empty()) return;

        const auto& cpu_s = cpu_sensor;

        found_sensors.at(cpu_s).temp = stol(ut::file::read(found_sensors.at(cpu_s).path, "0")) / 1000;
        current_cpu.critical_temperature = found_sensors.at(cpu_s).crit;
    }

    int DataCollector::get_core_count() {
        int core_count = sysconf(_SC_NPROCESSORS_ONLN);

        if (core_count < 1) {
            core_count = sysconf(_SC_NPROCESSORS_CONF);
            if (core_count < 1) {
                core_count = 1;
            }
        }

        return core_count;
    }

    CpuFrequency DataCollector::get_cpu_frequency() {
        static int failed{}; // defaults to 0
        double value{};
        string units{};

        if (failed > 4)
            return CpuFrequency{value, units};

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
                if (hz >= 10000) value = round(hz / 1000);
                else value = round(hz / 100) / 10.0;

                units = "GHz";
            }
            else if (hz > 0) {
                value = round(hz);
                units = "MHz";
            }
        }
        catch (const std::exception& e) {
            if (++failed < 5) return CpuFrequency{value, units};
            else return CpuFrequency{value, units};
        }

        return CpuFrequency{value, units};
    }

    string DataCollector::get_cpu_mame() {
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

    Data DataCollector::collect() {
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
            int target = core_count;

            for (; i <= target or (stream.good() and stream.peek() == 'c'); i++) {
                //? Make sure to add zero value for missing core values if at end of file
                if ((not stream.good() or stream.peek() != 'c') and i <= target) {
                    if (i == 0) throw std::runtime_error("Failed to parse /proc/stat");
                    else {
                        cpu.core_percent.at(i-1) = 0;
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
                            cpu.core_percent.at(i-1) = 0;
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
                        cpu.cpu_percent.at("total") = clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll);

                        //? Populate cpu.cpu_percent with all fields from stat
                        for (int ii = 0; const auto& val : times) {
                            cpu.cpu_percent.at(time_names.at(ii)) = clamp((long long)round((double)(val - CpuOld.at(time_names.at(ii))) * 100 / calc_totals), 0ll, 100ll);
                            CpuOld.at(time_names.at(ii)) = val;

                            if (++ii == 10) break;
                        }
                        continue;
                    }
                        //? Calculate cpu total for each core
                    else {
                        const long long calc_totals = max(0ll, totals - core_old_totals.at(i-1));
                        const long long calc_idles = max(0ll, idles - core_old_idles.at(i-1));
                        core_old_totals.at(i-1) = totals;
                        core_old_idles.at(i-1) = idles;

                        cpu.core_percent.at(i-1) = clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll);
                    }
                }
            }

        }
        catch (const std::exception& e) {
            if (stream.bad()) throw std::runtime_error("Failed to read /proc/stat");
            else throw std::runtime_error("collect() : " + string{e.what()});
        }

        if (got_sensors)
            update_sensors();

        return Data {
            CpuUsage{
            cpu.cpu_percent.at("total"),
            cpu.cpu_percent.at("user"),
            cpu.cpu_percent.at("nice"),
            cpu.cpu_percent.at("system"),
            cpu.cpu_percent.at("idle"),
            cpu.cpu_percent.at("iowait"),
            cpu.cpu_percent.at("irq"),
            cpu.cpu_percent.at("softirq"),
            cpu.cpu_percent.at("steal"),
            cpu.cpu_percent.at("guest"),
            cpu.cpu_percent.at("guest_nice")
            },
            found_sensors.at( cpu_sensor).temp,
            CpuAvgLoad{cpu.load_avg[0], cpu.load_avg[1], cpu.load_avg[2]},
            cpu.core_percent,
            get_cpu_frequency(),
            cpu_name,
            core_count,
            cpu.critical_temperature
        };
    }
}

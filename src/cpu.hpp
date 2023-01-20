#include "collect.hpp"

namespace cpu {
    class Data {
        private:
            long long cpu_load_percent;
            int64_t cpu_temp;
            GetCpuAvgLoadResult cpu_load_avg;
            vector<deque<long long>> core_load;

        public:
        Data(
                long long cpu_load_percent,
                int64_t cpu_temp,
                GetCpuAvgLoadResult cpu_load_avg,
                vector<deque<long long>> core_load

        ) {
            this->cpu_load_percent = cpu_load_percent;
            this->cpu_temp = cpu_temp;
            this->cpu_load_avg = cpu_load_avg;
            this->core_load = core_load;
        }

        long long get_cpu_load_percent() {
            return this->cpu_load_percent;
        }

        int64_t get_cpu_temp() {
            return this->cpu_temp;
        }

        GetCpuAvgLoadResult get_average_load() {
            return this->cpu_load_avg;
        }

        int get_core_count() {
            return shared::core_count;
        }

        vector<deque<long long>> get_core_load() {
            return this->core_load;
        }

        GetCpuFrequencyResult get_cpu_frequency() {
            static int failed{}; // defaults to 0
            GetCpuFrequencyResult result;

            if (failed > 4)
                return result;

            try {
                double hz{}; // defaults to 0.0

                // Try to get freq from /sys/devices/system/cpu/cpufreq/policy first (faster)
                if (not freq_path.empty()) {
                    hz = stod(ut::file::read(freq_path, "0.0")) / 1000;

                    if (hz <= 0.0 and ++failed >= 2)
                        freq_path.clear();
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
                    ut::logger::warning("get_cpu_hz() : " + string{e.what()});

                    return result;
                }
            }

            return result;
        }

        auto get_cpu_mame() -> std::basic_string<char> {
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
    };

    void init () {
        shared::init();

        if (not fs::exists(cpu::freq_path) or access(cpu::freq_path.c_str(), R_OK) == -1)
            cpu::freq_path.clear();

        cpu::current_cpu.core_percent.insert(cpu::current_cpu.core_percent.begin(), shared::core_count, {});
        cpu::current_cpu.temp.insert(cpu::current_cpu.temp.begin(), shared::core_count + 1, {});
        cpu::core_old_totals.insert(cpu::core_old_totals.begin(),  shared::core_count, 0);
        cpu::core_old_idles.insert(cpu::core_old_idles.begin(),  shared::core_count, 0);

        for (auto& [field, vec] : cpu::current_cpu.cpu_percent) {
            if (not vec.empty()) cpu::available_fields.push_back(field);
        }

        cpu::got_sensors = get_sensors();

        for (const auto& [sensor, ignored] : cpu::found_sensors) {
            cpu::available_sensors.push_back(sensor);
        }

        is_initilized = true;
    }

//    GetCpuAvgLoadResult get_average_load() {
//        if (not is_initilized) init();
//
//        GetCpuAvgLoadResult result;
//        std::ifstream stream;
//
//        stream.open(shared::proc_path / "loadavg");
//
//        if (stream.good()) {
//            stream >> result.one_min >> result.five_min >> result.fifteen_min;
//        }
//
//        stream.close();
//
//        return result;
//    }
//
//    int64_t get_cpu_temperature() {
//        if (not is_initilized) init();
//
//        update_sensors();
//
//        return found_sensors.at( cpu::cpu_sensor).temp;
//    }
//
//    int get_core_count() {
//        return shared::core_count;
//    }

    Data collect() {
        if (not is_initilized) init();

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
                        const long long calc_totals = max(1ll, totals - cpu_old.at("totals"));
                        const long long calc_idles = max(1ll, idles - cpu_old.at("idles"));
                        cpu_old.at("totals") = totals;
                        cpu_old.at("idles") = idles;

                        //? Total usage of cpu
                        cpu.cpu_percent.at("total").push_back(clamp((long long)round((double)(calc_totals - calc_idles) * 100 / calc_totals), 0ll, 100ll));

                        //? Reduce size if there are more values than needed for graph
                        while (cmp_greater(cpu.cpu_percent.at("total").size(), width * 2)) cpu.cpu_percent.at("total").pop_front();

                        //? Populate cpu.cpu_percent with all fields from stat
                        for (int ii = 0; const auto& val : times) {
                            cpu.cpu_percent.at(time_names.at(ii)).push_back(clamp((long long)round((double)(val - cpu_old.at(time_names.at(ii))) * 100 / calc_totals), 0ll, 100ll));
                            cpu_old.at(time_names.at(ii)) = val;

                            //? Reduce size if there are more values than needed for graph
                            while (cmp_greater(cpu.cpu_percent.at(time_names.at(ii)).size(), width * 2)) cpu.cpu_percent.at(time_names.at(ii)).pop_front();

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
            ut::logger::debug("Cpu::collect() : " + string{e.what()});

            if (stream.bad()) throw std::runtime_error("Failed to read /proc/stat");
            else throw std::runtime_error("Cpu::collect() : " + string{e.what()});
        }

        if (got_sensors)
            update_sensors();

        return Data {
                cpu.cpu_percent.at("total").back(),
                found_sensors.at( cpu::cpu_sensor).temp,
                GetCpuAvgLoadResult{cpu.load_avg[0], cpu.load_avg[1], cpu.load_avg[2]},
                cpu.core_percent
        };
    }
}
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
#include <fstream>
#include <ranges>
#include <atomic>
#include "ctime"
#include <unistd.h>
#include <deque>
#include <vector>
#include <sys/statvfs.h>

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
using std::array;
using std::deque;
using std::vector;
using std::string_view;
using std::atomic;

using namespace std::literals;

namespace fs = std::filesystem;
namespace rng = std::ranges;

namespace shared {
    fs::path proc_path, passwd_path;
    long page_size, clk_tck;
    fs::path freq_path;
    bool is_init{};

    inline void init() {
        if (is_init) return;

        // Shared global variables init
        proc_path =
                (fs::is_directory(fs::path("/proc")) and access("/proc", R_OK) != -1) ? "/proc" : "";

        if (proc_path.empty())
            throw std::runtime_error("Proc filesystem not found or no permission to read from it!");

        passwd_path =
                (fs::is_regular_file(fs::path("/etc/passwd")) and access("/etc/passwd", R_OK) != -1) ? "/etc/passwd" : "";

        freq_path = "/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq";

        page_size = sysconf(_SC_PAGE_SIZE);

        if (page_size <= 0) {
            page_size = 4096;
        }

        clk_tck = sysconf(_SC_CLK_TCK);

        if (clk_tck <= 0) {
            clk_tck = 100;
        }

        is_init = true;
    }
}


/**
 * generic utils
 */
namespace ut {
    constexpr auto maxStreamSize = std::numeric_limits<std::streamsize>::max();

    //* Sets atomic<bool> to true on construct, sets to false on destruct
    class atomic_lock {
        atomic<bool>& atom;
        bool not_true{}; // defaults to false
    public:
        explicit atomic_lock(atomic<bool>& atom, bool wait = false): atom(atom) {
            if (wait) while (not this->atom.compare_exchange_strong(this->not_true, true));
            else this->atom.store(true);
        }
        ~atomic_lock() {
            this->atom.store(false);
        }
    };

    string strf_time(const string& strf) {
        auto in_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm bt {};
        std::stringstream ss;
        ss << std::put_time(localtime_r(&in_time_t, &bt), strf.c_str());
        return ss.str();
    }

    double system_uptime() {
        string upstr;
        std::ifstream pread(shared::proc_path / "uptime");

        if (pread.good()) {
            try {
                getline(pread, upstr, ' ');
                pread.close();
                return stod(upstr);
            }
            catch (const std::invalid_argument&) {}
            catch (const std::out_of_range&) {}
        }
        throw std::runtime_error("Failed get uptime from from " + string{shared::proc_path} + "/uptime");
    }

    /**
     * file utils
     */
    namespace file {
        string read(const std::filesystem::path& path, const string& fallback = "");

        string read(const std::filesystem::path& path, const string& fallback) {
            if (not fs::exists(path)) return fallback;

            string out;

            try {
                std::ifstream file(path);

                for (string readstr; getline(file, readstr); out += readstr);
            }
            catch (const std::exception& e) {
                return fallback;
            }

            return (out.empty() ? fallback : out);
        }
    }

    /**
     * string utils
     */
    namespace str {
        inline string capitalize(string str) {
            str.at(0) = toupper(str.at(0));

            return str;
        }

        template <typename T>
        inline bool contains(const string& str, const T& find_val) {
            return str.find(find_val) != string::npos;
        }

        auto split(const string& str, const char& delim = ' ') -> vector<string> {
            vector<string> out;

            for (const auto& s : str 	| rng::views::split(delim)
                                 | rng::views::transform([](auto &&rng) {
                return string_view(&*rng.begin(), rng::distance(rng));
            })) {
                if (not s.empty()) out.emplace_back(s);
            }

            return out;
        }

        string replace(const string& str, const string& from, const string& to) {
            string out = str;

            for (size_t start_pos = out.find(from); start_pos != string::npos; start_pos = out.find(from)) {
                out.replace(start_pos, from.length(), to);
            }

            return out;
        }

        string ltrim(const string& str, const string& t_str) {
            string_view str_v{str};

            while (str_v.starts_with(t_str))
                str_v.remove_prefix(t_str.size());

            return string{str_v};
        }

        string rtrim(const string& str, const string& t_str) {
            string_view str_v{str};

            while (str_v.ends_with(t_str))
                str_v.remove_suffix(t_str.size());

            return string{str_v};
        }

        //* Left/right-trim <t_str> from <str> and return new string
        inline string trim(const string& str, const string& t_str = " ") {
            return ltrim(rtrim(str, t_str), t_str);
        }

        //* Return <str> with only lowercase characters
        inline string to_lower(string str) {
            std::ranges::for_each(str, [](char& c) { c = ::tolower(c); } );

            return str;
        }
    }

    /**
     * vector utils
     */
    namespace vec {
        template <typename T, typename T2>
        inline bool contains(const vector<T>& vec, const T2& find_val) {
            return std::ranges::find(vec, find_val) != vec.end();
        }

        template <typename T>
        inline size_t index(const vector<T>& vec, const T& find_val) {
            return std::ranges::distance(vec.begin(), std::ranges::find(vec, find_val));
        }
    }

    /**
     * type utils
     */
    namespace type {
        template<typename First, typename ... T>
        inline bool is_in(const First& first, const T& ... t) {
            return ((first == t) or ...);
        }
    }

    /**
     * logger
     */
    namespace logger {
        fs::path log_file_path = "./log/node-hw-info.log";
        std::atomic<bool> busy (false);
        bool first = true;
        const string tdf = "%Y/%m/%d (%T) | ";
        uid_t real_uid, set_uid;

        const vector<string> log_levels = {
                "DISABLED",
                "ERROR",
                "WARNING",
                "INFO",
                "DEBUG",
        };

        /**
         * Wrapper for lowering privileges if using SUID bit and currently isn't using real userid
         */
        class lose_priv {
            int status = -1;
        public:
            lose_priv() {
                if (geteuid() != real_uid) {
                    this->status = seteuid(real_uid);
                }
            }
            ~lose_priv() {
                if (status == 0) {
                    status = seteuid(set_uid);
                }
            }
        };

        void log_write(const size_t level, const string& msg) {
            if (log_file_path.empty()) return;

            atomic_lock lck(busy, true);
            lose_priv neutered{};
            std::error_code ec;

            try {
                if (fs::exists(log_file_path) and fs::file_size(log_file_path, ec) > 1024 << 10 and not ec) {
                    auto old_log = log_file_path;
                    old_log += ".1";

                    if (fs::exists(old_log))
                        fs::remove(old_log, ec);

                    if (not ec)
                        fs::rename(log_file_path, old_log, ec);
                }
                if (not ec) {
                    std::ofstream lwrite(log_file_path, std::ios::app);
                    if (first) {
                        first = false;
                    }
                    lwrite << strf_time(tdf) << log_levels.at(level) << ": " << msg << "\n";
                }
                else log_file_path.clear();
            }
            catch (const std::exception& e) {
                log_file_path.clear();
                throw std::runtime_error("Exception in Logger::log_write() : " + string{e.what()});
            }
        }

        inline void error(const string& msg) { log_write(1, msg); }
        inline void warning(const string& msg) { log_write(2, msg); }
        inline void info(const string& msg) { log_write(3, msg); }
        inline void debug(const string& msg) { log_write(4, msg); }
    }
}

namespace cpu {
    class CpuFrequency {
    private:
        double value{};
        string units{};

    public:
        CpuFrequency(double value, string  units) : value(value), units(std::move(units)) {}

        [[nodiscard]] double get_value() const {
            return value;
        }

        [[nodiscard]] const string& get_units() const {
            return units;
        }
    };

    class CpuAvgLoad {
    private:
        double one_min{};
        double five_min{};
        double fifteen_min{}; // 10 min for RedHat

    public:
        CpuAvgLoad(
            double one_min,
            double five_min,
            double fifteen_min
        ) :
        one_min(one_min),
        five_min(five_min),
        fifteen_min(fifteen_min) {}

        [[nodiscard]] double get_one_min() const {
            return one_min;
        }

        [[nodiscard]] double get_five_min() const {
            return five_min;
        }

        [[nodiscard]] double get_fifteen_min() const {
            return fifteen_min;
        }
    };

    class CpuUsage {
        long long total_percent{};
        long long user_percent{};
        long long nice_percent{};
        long long system_percent{};
        long long idle_percent{};
        long long iowait_percent{};
        long long irq_percent{};
        long long softirq_percent{};
        long long steal_percent{};
        long long guest_percent{};
        long long guest_nice_percent{};

    public:
        CpuUsage(
            long long int total_percent,
            long long int user_percent,
            long long int nice_percent,
            long long int system_percent,
            long long int idle_percent,
            long long int iowait_percent,
            long long int irq_percent,
            long long int softirq_percent,
            long long int steal_percent,
            long long int guest_percent,
            long long int guest_nice_percent
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
        guest_nice_percent(
        guest_nice_percent) {}

        [[nodiscard]] long long int get_total_percent() const {
            return total_percent;
        }

        [[nodiscard]] long long int get_user_percent() const {
            return user_percent;
        }

        [[nodiscard]] long long int get_nice_percent() const {
            return nice_percent;
        }

        [[nodiscard]] long long int get_system_percent() const {
            return system_percent;
        }

        [[nodiscard]] long long int get_idle_percent() const {
            return idle_percent;
        }

        [[nodiscard]] long long int get_iowait_percent() const {
            return iowait_percent;
        }

        [[nodiscard]] long long int get_irq_percent() const {
            return irq_percent;
        }

        [[nodiscard]] long long int get_softirq_percent() const {
            return softirq_percent;
        }

        [[nodiscard]] long long int get_steal_percent() const {
            return steal_percent;
        }

        [[nodiscard]] long long int get_guest_percent() const {
            return guest_percent;
        }

        [[nodiscard]] long long int get_guest_nice_percent() const {
            return guest_nice_percent;
        }
    };

    class StaticValuesAware {
    protected:
        string cpu_name;
        int core_count;
        long long critical_temperature;
    };

    class Data : StaticValuesAware {
    private:
        CpuUsage cpu_usage{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        int64_t cpu_temp;
        CpuAvgLoad cpu_load_avg{0, 0, 0};
        vector<long long> core_load;
        CpuFrequency cpu_frequency{0, ""};

    public:
        Data(
            CpuUsage cpu_usage,
            int64_t cpu_temp,
            CpuAvgLoad cpu_load_avg,
            vector<long long> core_load,
            CpuFrequency cpu_frequency,
            string cpu_name,
            int core_count,
            long long critical_temperature
        ) {
            this->cpu_usage = cpu_usage;
            this->cpu_temp = cpu_temp;
            this->cpu_load_avg = cpu_load_avg;
            this->core_load = std::move(core_load);
            this->cpu_frequency = std::move(cpu_frequency);
            this->cpu_name = std::move(cpu_name);
            this->core_count = core_count;
            this->critical_temperature = critical_temperature;
        }

        [[nodiscard]] CpuUsage get_cpu_usage() const {
            return this->cpu_usage;
        }

        [[nodiscard]] int64_t get_cpu_temp() const {
            return this->cpu_temp;
        }

        [[nodiscard]] CpuAvgLoad get_average_load() {
            return this->cpu_load_avg;
        }

        [[nodiscard]] vector<long long> get_core_load() {
            return this->core_load;
        }

        [[nodiscard]] CpuFrequency get_cpu_frequency() {
            return this->cpu_frequency;
        }

        [[nodiscard]] string get_cpu_mame() {
            return this->cpu_name;
        }

        [[nodiscard]] int get_core_count() const {
            return this->core_count;
        };

        [[nodiscard]] long long get_cpu_critical_temperature() const {
            return this->critical_temperature;
        }
    };

    class DataCollector : StaticValuesAware {
    public:
        DataCollector() {
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
    private:
        struct Sensor {
            fs::path path;
            string label;
            int64_t temp{}; // defaults to 0
            int64_t high{}; // defaults to 0
            int64_t crit{}; // defaults to 0
        };
        struct CpuInfo {
            std::unordered_map<string, long long> cpu_percent = {
                    {"total", 0},
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
            vector<long long> core_percent;
            long long critical_temperature{};
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
        string cpu_sensor;
        vector<string> core_sensors;
        vector<long long> core_old_totals;
        vector<long long> core_old_idles;
        vector<string> available_sensors = {"Auto"};
        std::unordered_map<string, Sensor> found_sensors;
        CpuInfo current_cpu;
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
            current_cpu.critical_temperature = found_sensors.at(cpu_s).crit;
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
                if (++failed < 5)
                    return CpuFrequency{value, units};
                else {
                    ut::logger::warning("get_cpu_frequency() : " + string{e.what()});

                    return CpuFrequency{value, units};
                }
            }

            return CpuFrequency{value, units};
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
                ut::logger::debug("collect() : " + string{e.what()});

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
    };
}

namespace mem {
    class GenericMemUnit {
    private:
        uint64_t bytes;

    public:
        explicit GenericMemUnit(uint64_t bytes) : bytes(bytes) {}

        [[nodiscard]] double to_gigabytes() const {
            return (double)this->bytes/(1024 * 1024 * 1024);
        }

        [[nodiscard]] double to_megabytes() const {
            return (double)this->bytes/(1024 * 1024);
        }

        [[nodiscard]] double to_kilobytes() const {
            return (double)this->bytes/1024;
        }
    };

    class RamUnit : public GenericMemUnit {
    private:
        long long percent;

    public:
        RamUnit(uint64_t bytes, long long int percent) : GenericMemUnit(bytes), percent(percent) {}

        [[nodiscard]] long long to_percent() const {
            return this->percent;
        }
    };

    class StorageUnit {
        GenericMemUnit total;
        GenericMemUnit used;
        GenericMemUnit free;
        int used_percent;
        int free_percent;
        string handle;
        string fs_type;
        long long io_read;
        long long io_write;
        long long io_activity;
        fs::path path;

    public:
        StorageUnit(
            const GenericMemUnit &total,
            const GenericMemUnit &used,
            const GenericMemUnit &free,
            int usedPercent,
            int freePercent,
            string& handle,
            string& fs_type,
            long long io_read,
            long long io_write,
            long long io_activity,
            fs::path& path
        ) :
        total(total),
        used(used),
        free(free),
        used_percent(usedPercent),
        free_percent(freePercent),
        handle(handle),
        fs_type(fs_type),
        io_read(io_read),
        io_write(io_write),
        io_activity(io_activity),
        path(path) {}

        [[nodiscard]] const GenericMemUnit& get_total() const {
            return this->total;
        }

        [[nodiscard]] const GenericMemUnit& get_used() const {
            return this->used;
        }

        [[nodiscard]] const GenericMemUnit& get_free() const {
            return this->free;
        }

        [[nodiscard]] int get_used_percent() const {
            return this->used_percent;
        }

        [[nodiscard]] int get_free_percent() const {
            return this->free_percent;
        }

        [[nodiscard]] const string& get_handle() const {
            return this->handle;
        }

        [[nodiscard]] const string& get_fs_type() const {
            return this->fs_type;
        }

        [[nodiscard]] long long int get_io_read() const {
            return this->io_read;
        }

        [[nodiscard]] long long int get_io_write() const {
            return this->io_write;
        }

        [[nodiscard]] long long int get_io_activity() const {
            return this->io_activity;
        }

        [[nodiscard]] const fs::path& get_path() const {
            return this->path;
        }
    };

    class StaticValuesAware {
    protected:
        GenericMemUnit total_ram_amount{0};
    };

    class Data : StaticValuesAware {
    private:
        RamUnit available_ram_amount{0, 0};
        RamUnit cached_ram_amount{0, 0};
        RamUnit free_ram_amount{0, 0};
        RamUnit used_ram_amount{0, 0};
        vector<StorageUnit> disks;

    public:
        Data(
            const GenericMemUnit& total_ram_amount,
            const RamUnit& available_ram_amount,
            const RamUnit& cached_ram_amount,
            const RamUnit& free_ram_amount,
            const RamUnit& used_ram_amount,
            const vector<StorageUnit>& disks
        ) :
        available_ram_amount(available_ram_amount),
        cached_ram_amount(cached_ram_amount),
        free_ram_amount(free_ram_amount),
        used_ram_amount(used_ram_amount),
        disks(disks) {
            this->total_ram_amount = total_ram_amount;
        }

        [[nodiscard]] const GenericMemUnit& get_total_ram_amount() const {
            return this->total_ram_amount;
        }

        [[nodiscard]] const RamUnit& get_available_ram_amount() const {
            return this->available_ram_amount;
        }

        [[nodiscard]] const RamUnit& get_cached_ram_amount() const {
            return this->cached_ram_amount;
        }

        [[nodiscard]] const RamUnit& get_free_ram_amount() const {
            return this->free_ram_amount;
        }

        [[nodiscard]] const RamUnit& get_used_ram_amount() const {
            return this->used_ram_amount;
        }

        [[nodiscard]] const vector<StorageUnit>& get_disks() const {
            return this->disks;
        }
    };

    class DataCollector : StaticValuesAware {
    public:
        DataCollector() {
            shared::init();

            this->total_ram_amount = GenericMemUnit{this->get_total_ram_amount()};
            this->old_uptime = ut::system_uptime();
        }

    private:
        struct DiskInfo {
            std::filesystem::path dev;
            string name;
            string fstype{};
            std::filesystem::path stat{};
            uint64_t total{};
            uint64_t used{};
            uint64_t free{};
            int used_percent{};
            int free_percent{};

            array<uint64_t, 3> old_io = {0, 0, 0};
            long long io_read = {};
            long long io_write = {};
            long long io_activity = {};
        };

        struct MemInfo {
            std::unordered_map<string, uint64_t> stats =
                    {{"used", 0}, {"available", 0}, {"cached", 0}, {"free", 0},
                     {"swap_total", 0}, {"swap_used", 0}, {"swap_free", 0}};
            std::unordered_map<string, long long> percent =
                    {{"used", {}}, {"available", {}}, {"cached", {}}, {"free", {}},
                     {"swap_total", {}}, {"swap_used", {}}, {"swap_free", {}}};
            std::unordered_map<string, DiskInfo> disks;
            vector<string> disks_order;
        };

        bool has_swap{}; // defaults to false
        vector<string> fstab;
        fs::file_time_type fstab_time;
        int disk_ios{}; // defaults to 0
        vector<string> last_found;
        MemInfo current_mem{};
        const array<string, 4> mem_names { "used"s, "available"s, "cached"s, "free"s };
        const array<string, 2> swap_names { "swap_used"s, "swap_free"s };
        double old_uptime;

        uint64_t get_total_ram_amount() {
            std::ifstream mem_info(shared::proc_path / "meminfo");
            int64_t totalMem;

            if (mem_info.good()) {
                mem_info.ignore(ut::maxStreamSize, ':');
                mem_info >> totalMem;
                totalMem <<= 10;
            }

            if (not mem_info.good() or totalMem == 0)
                throw std::runtime_error("Could not get total memory size from /proc/meminfo");

            return totalMem;
        }

    public:
        Data collect() {
            auto totalMem = this->get_total_ram_amount();
            auto &mem = current_mem;

            mem.stats.at("swap_total") = 0;

            //? Read memory info from /proc/meminfo
            std::ifstream meminfo(shared::proc_path / "meminfo");

            if (meminfo.good()) {
                bool got_avail = false;

                for (string label; meminfo.peek() != 'D' and meminfo >> label;) {
                    if (label == "MemFree:") {
                        meminfo >> mem.stats.at("free");
                        mem.stats.at("free") <<= 10;
                    } else if (label == "MemAvailable:") {
                        meminfo >> mem.stats.at("available");
                        mem.stats.at("available") <<= 10;
                        got_avail = true;
                    } else if (label == "Cached:") {
                        meminfo >> mem.stats.at("cached");
                        mem.stats.at("cached") <<= 10;
                    } else if (label == "SwapTotal:") {
                        meminfo >> mem.stats.at("swap_total");
                        mem.stats.at("swap_total") <<= 10;
                    } else if (label == "SwapFree:") {
                        meminfo >> mem.stats.at("swap_free");
                        mem.stats.at("swap_free") <<= 10;
                        break;
                    }

                    meminfo.ignore(ut::maxStreamSize, '\n');
                }
                if (not got_avail) mem.stats.at("available") = mem.stats.at("free") + mem.stats.at("cached");
                mem.stats.at("used") = totalMem - (mem.stats.at("available") <= totalMem ? mem.stats.at("available")
                                                                                         : mem.stats.at("free"));

                if (mem.stats.at("swap_total") > 0)
                    mem.stats.at("swap_used") = mem.stats.at("swap_total") - mem.stats.at("swap_free");
            } else {
                throw std::runtime_error("Failed to read /proc/meminfo");
            }

            meminfo.close();

            //? Calculate percentages
            for (const auto &name: mem_names) {
                mem.percent.at(name) = round((double) mem.stats.at(name) * 100 / totalMem);
            }

            if (mem.stats.at("swap_total") > 0) {
                for (const auto &name: swap_names) {
                    mem.percent.at(name) = round((double) mem.stats.at(name) * 100 / mem.stats.at("swap_total"));
                }

                has_swap = true;
            } else {
                has_swap = false;
            }

            //? Get disks stats
            static vector<string> ignore_list;
            double uptime = ut::system_uptime();

            try {
                bool filter_exclude = false;
                auto &disks = mem.disks;
                std::ifstream diskread;

                //? Get list of "real" filesystems from /proc/filesystems
                vector<string> fstypes;

                diskread.open(shared::proc_path / "filesystems");
                if (diskread.good()) {
                    for (string fstype; diskread >> fstype;) {
                        if (not ut::type::is_in(fstype, "nodev", "squashfs", "nullfs", "zfs", "wslfs", "drvfs"))
                            fstypes.push_back(fstype);
                        diskread.ignore(ut::maxStreamSize, '\n');
                    }
                }
                else {
                    throw std::runtime_error("Failed to read /proc/filesystems");
                }

                diskread.close();

                //? Get mounts from /etc/mtab or /proc/self/mounts
                diskread.open((fs::exists("/etc/mtab") ? fs::path("/etc/mtab") : shared::proc_path / "self/mounts"));

                if (diskread.good()) {
                    vector<string> found;
                    found.reserve(last_found.size());
                    string dev, mountpoint, fstype;

                    while (not diskread.eof()) {
                        std::error_code ec;
                        diskread >> dev >> mountpoint >> fstype;
                        diskread.ignore(ut::maxStreamSize, '\n');

                        if (ut::vec::contains(ignore_list, mountpoint) or ut::vec::contains(found, mountpoint)) continue;

                        if (ut::vec::contains(fstab, mountpoint) or ut::vec::contains(fstypes, fstype)) {
                            found.push_back(mountpoint);

                            //? Save mountpoint, name, fstype, dev path and path to /sys/block stat file
                            if (not disks.contains(mountpoint)) {
                                disks[mountpoint] = DiskInfo{fs::canonical(dev, ec),
                                                             fs::path(mountpoint).filename(), fstype};
                                if (disks.at(mountpoint).dev.empty()) disks.at(mountpoint).dev = dev;
                                if (disks.at(mountpoint).name.empty())
                                    disks.at(mountpoint).name = (mountpoint == "/" ? "root" : mountpoint);

                                string devname = disks.at(mountpoint).dev.filename();

                                int c = 0;
                                while (devname.size() >= 2) {
                                    if (fs::exists("/sys/block/" + devname + "/stat", ec) and
                                        access(string("/sys/block/" + devname + "/stat").c_str(), R_OK) == 0) {
                                        if (c > 0 and fs::exists("/sys/block/" + devname + '/' +
                                                                 disks.at(mountpoint).dev.filename().string() +
                                                                 "/stat", ec))
                                            disks.at(mountpoint).stat = "/sys/block/" + devname + '/' + disks.at(
                                                    mountpoint).dev.filename().string() + "/stat";
                                        else
                                            disks.at(mountpoint).stat = "/sys/block/" + devname + "/stat";
                                        break;
                                        //? Set ZFS stat filepath
                                    }

                                    devname.resize(devname.size() - 1);
                                    c++;
                                }
                            }
                        }
                    }

                    //? Remove disks no longer mounted or filtered out
                    if (has_swap) found.push_back("swap");

                    for (auto it = disks.begin(); it != disks.end();) {
                        if (not ut::vec::contains(found, it->first))
                            it = disks.erase(it);
                        else
                            it++;
                    }

                    last_found = std::move(found);
                } else {
                    throw std::runtime_error("Failed to get mounts from /etc/mtab and /proc/self/mounts");
                }

                diskread.close();

                //? Get disk/partition stats
                bool new_ignored = false;
                for (auto &[mountpoint, disk]: disks) {
                    if (std::error_code ec; not fs::exists(mountpoint, ec)
                        or ut::vec::contains(ignore_list, mountpoint)
                    ) {
                        continue;
                    }

                    struct statvfs64 vfs;

                    if (statvfs64(mountpoint.c_str(), &vfs) < 0) {
                        ut::logger::warning("Failed to get disk/partition stats for mount \"" + mountpoint +
                                        "\" with statvfs64 error code: " + std::to_string(errno) + ". Ignoring...");
                        ignore_list.push_back(mountpoint);

                        new_ignored = true;

                        continue;
                    }

                    disk.total = vfs.f_blocks * vfs.f_frsize;
                    disk.free = vfs.f_bavail * vfs.f_frsize;
                    disk.used = disk.total - disk.free;
                    disk.used_percent = round((double) disk.used * 100 / disk.total);
                    disk.free_percent = 100 - disk.used_percent;


                }

                //? Remove any problematic disks added to the ignore_list
                if (new_ignored) {
                    for (auto it = disks.begin(); it != disks.end();) {
                        if (ut::vec::contains(ignore_list, it->first))
                            it = disks.erase(it);
                        else
                            it++;
                    }
                }

                //? Setup disks order in UI and add swap if enabled
                mem.disks_order.clear();

                if (disks.contains("/")) mem.disks_order.push_back("/");

                if (has_swap) {
                    mem.disks_order.push_back("swap");

                    if (not disks.contains("swap")) disks["swap"] = {"", "swap", "swap"};

                    disks.at("swap").total = mem.stats.at("swap_total");
                    disks.at("swap").used = mem.stats.at("swap_used");
                    disks.at("swap").free = mem.stats.at("swap_free");
                    disks.at("swap").used_percent = mem.percent.at("swap_used");
                    disks.at("swap").free_percent = mem.percent.at("swap_free");
                }
                for (const auto &name: last_found)

                if (not ut::type::is_in(name, "/", "swap")) mem.disks_order.push_back(name);

                //? Get disks IO
                uint64_t sectors_read, sectors_write, io_ticks, io_ticks_temp;
                disk_ios = 0;
                for (auto &[ignored, disk]: disks) {
                    if (disk.stat.empty() or access(disk.stat.c_str(), R_OK) != 0) continue;

                    diskread.open(disk.stat);
                    if (diskread.good()) {
                        disk_ios++;

                        for (int i = 0; i < 2; i++) {
                            diskread >> std::ws;
                            diskread.ignore(ut::maxStreamSize, ' ');
                        }
                        diskread >> sectors_read;

                        disk.io_read = max((uint64_t) 0, (sectors_read - disk.old_io.at(0)) * 512);
                        disk.old_io.at(0) = sectors_read;

                        for (int i = 0; i < 3; i++) {
                            diskread >> std::ws;
                            diskread.ignore(ut::maxStreamSize, ' ');
                        }

                        diskread >> sectors_write;

                        disk.io_write = max((uint64_t) 0, (sectors_write - disk.old_io.at(1)) * 512);
                        disk.old_io.at(1) = sectors_write;

                        for (int i = 0; i < 2; i++) {
                            diskread >> std::ws;
                            diskread.ignore(ut::maxStreamSize, ' ');
                        }
                        diskread >> io_ticks;
                        disk.io_activity = clamp((long) round(
                                                         (double) (io_ticks - disk.old_io.at(2)) / (uptime - old_uptime) / 10), 0l,
                                                 100l);
                        disk.old_io.at(2) = io_ticks;
                    } else {
                        ut::logger::debug("Error in Mem::collect() : when opening " + string{disk.stat});
                    }

                    diskread.close();
                }
                old_uptime = uptime;
            }
            catch (const std::exception &e) {
                ut::logger::warning("Error in Mem::collect() : " + string{e.what()});
            }

            //TODO REFACTOR
            vector<StorageUnit> dsk;

            for(auto& d: mem.disks) {
                dsk.push_back(StorageUnit {
                    GenericMemUnit{d.second.total},
                    GenericMemUnit{d.second.used},
                    GenericMemUnit{d.second.used},
                    d.second.used_percent,
                    d.second.free_percent,
                    d.second.name,
                    d.second.fstype,
                    d.second.io_read,
                    d.second.io_write,
                    d.second.io_activity,
                    d.second.dev
                });
            }

            return Data {
                this->total_ram_amount,
                RamUnit{mem.stats.at("available"), mem.percent.at("available")},
                RamUnit{mem.stats.at("cached"), mem.percent.at("cached")},
                RamUnit{mem.stats.at("free"), mem.percent.at("free")},
                RamUnit{mem.stats.at("used"), mem.percent.at("used")},
                dsk
            };
        }
    };
}
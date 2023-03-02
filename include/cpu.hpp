#ifndef HWINFO_CPU_HPP
#define HWINFO_CPU_HPP

#include "unordered_map"
#include "ut.hpp"

using std::array;
using namespace std::literals;

namespace fs = std::filesystem;

namespace cpu {
    class CpuFrequency {
    private:
        double value{};
        string units{};

    public:
        CpuFrequency(const double& value, string units);

        [[nodiscard]] const double& get_value() const;
        [[nodiscard]] const string& get_units() const;
    };

    class CpuAvgLoad {
    private:
        double one_min{};
        double five_min{};
        double fifteen_min{}; // 10 min for RedHat

    public:
        CpuAvgLoad(const double& one_min, const double& five_min, const double& fifteen_min);

        [[nodiscard]] const double& get_one_min() const;
        [[nodiscard]] const double& get_five_min() const;
        [[nodiscard]] const double& get_fifteen_min() const;
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
        );

        [[nodiscard]] const long long int& get_total_percent() const;
        [[nodiscard]] const long long int& get_user_percent() const;
        [[nodiscard]] const long long int& get_nice_percent() const;
        [[nodiscard]] const long long int& get_system_percent() const;
        [[nodiscard]] const long long int& get_idle_percent() const;
        [[nodiscard]] const long long int& get_iowait_percent() const;
        [[nodiscard]] const long long int& get_irq_percent() const;
        [[nodiscard]] const long long int& get_softirq_percent() const;
        [[nodiscard]] const long long int& get_steal_percent() const;
        [[nodiscard]] const long long int& get_guest_percent() const;
        [[nodiscard]] const long long int& get_guest_nice_percent() const;
    };

    class StaticValuesAware {
    protected:
        string cpu_name;
        int core_count{};
        long long critical_temperature{};

        StaticValuesAware();
        StaticValuesAware(const string& cpu_name, int core_count, long long int critical_temperature);
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
            const CpuUsage& cpu_usage,
            const int64_t& cpu_temp,
            const CpuAvgLoad& cpu_load_avg,
            const vector<long long>& core_load,
            const CpuFrequency& cpu_frequency,
            const string& cpu_name,
            const int& core_count,
            const long long& critical_temperature
        );

        [[nodiscard]] const CpuUsage& get_cpu_usage() const;
        [[nodiscard]] const int64_t& get_cpu_temp() const;
        [[nodiscard]] const CpuAvgLoad& get_average_load();
        [[nodiscard]] const vector<long long>& get_core_load();
        [[nodiscard]] const CpuFrequency& get_cpu_frequency();
        [[nodiscard]] const string& get_cpu_mame();
        [[nodiscard]] const int& get_core_count() const;
        [[nodiscard]] const long long& get_cpu_critical_temperature() const;
    };

    class DataCollector : StaticValuesAware {
    public:
        DataCollector();

        Data collect();

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

        bool get_sensors();
        void update_sensors();
        int get_core_count();
        CpuFrequency get_cpu_frequency();
        string get_cpu_mame();
    };
}

#endif //HWINFO_CPU_HPP

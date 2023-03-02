#ifndef HWINFO_MEM_HPP
#define HWINFO_MEM_HPP

#include <unordered_map>
#include "ut.hpp"

namespace mem {
    class GenericMemUnit {
    private:
        uint64_t bytes;

    public:
        explicit GenericMemUnit(const uint64_t& bytes);

        [[nodiscard]] double to_gigabytes() const;
        [[nodiscard]] double to_megabytes() const;
        [[nodiscard]] double to_kilobytes() const;
    };

    class RamUnit : public GenericMemUnit {
    private:
        long long percent;

    public:
        RamUnit(const uint64_t& bytes, const long long int& percent);

        [[nodiscard]] const long long& to_percent() const;
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
            const int& usedPercent,
            const int& freePercent,
            string handle,
            string fs_type,
            const long long& io_read,
            const long long& io_write,
            const long long& io_activity,
            fs::path path
        );

        [[nodiscard]] const GenericMemUnit& get_total() const;
        [[nodiscard]] const GenericMemUnit& get_used() const;
        [[nodiscard]] const GenericMemUnit& get_free() const;
        [[nodiscard]] const int& get_used_percent() const;
        [[nodiscard]] const int& get_free_percent() const;
        [[nodiscard]] const string& get_handle() const;
        [[nodiscard]] const string& get_fs_type() const;
        [[nodiscard]] const long long int& get_io_read() const;
        [[nodiscard]] const long long int& get_io_write() const;
        [[nodiscard]] const long long int& get_io_activity() const;
        [[nodiscard]] const fs::path& get_path() const;
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
        );

        [[nodiscard]] const GenericMemUnit& get_total_ram_amount() const;
        [[nodiscard]] const RamUnit& get_available_ram_amount() const;
        [[nodiscard]] const RamUnit& get_cached_ram_amount() const;
        [[nodiscard]] const RamUnit& get_free_ram_amount() const;
        [[nodiscard]] const RamUnit& get_used_ram_amount() const;
        [[nodiscard]] const vector<StorageUnit>& get_disks() const;
    };

    class DataCollector : StaticValuesAware {
    public:
        DataCollector();

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

        uint64_t get_total_ram_amount();

    public:
        Data collect();
    };
}

#endif //HWINFO_MEM_HPP

#include "cmath"
#include <sys/statvfs.h>
#include "../include/mem.hpp"

using std::clamp;
using std::round;
using std::max;

namespace mem {
    GenericMemUnit::GenericMemUnit(const uint64_t& bytes) : bytes(bytes) {};

    double GenericMemUnit::to_gigabytes() const {
        return (double)bytes/(1024 * 1024 * 1024);
    }

    double GenericMemUnit::to_megabytes() const {
        return (double)bytes/(1024 * 1024);
    }

    double GenericMemUnit::to_kilobytes() const {
        return (double)bytes/1024;
    }

    RamUnit::RamUnit(const uint64_t& bytes, const long long& percent): GenericMemUnit(bytes), percent(percent) {};

    const long long& RamUnit::to_percent() const {
        return percent;
    }

    StorageUnit::StorageUnit(
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
    ) :
    total(total),
    used(used),
    free(free),
    used_percent(usedPercent),
    free_percent(freePercent),
    handle(std::move(handle)),
    fs_type(std::move(fs_type)),
    io_read(io_read),
    io_write(io_write),
    io_activity(io_activity),
    path(std::move(path)) {}

    const GenericMemUnit& StorageUnit::get_total() const {
        return total;
    }

    const GenericMemUnit& StorageUnit::get_used() const {
        return used;
    }

    const GenericMemUnit& StorageUnit::get_free() const {
        return free;
    }

    const int& StorageUnit::get_used_percent() const {
        return used_percent;
    }

    const int& StorageUnit::get_free_percent() const {
        return free_percent;
    }

    const string& StorageUnit::get_handle() const {
        return handle;
    }

    const string& StorageUnit::get_fs_type() const {
        return fs_type;
    }

    const long long int& StorageUnit::get_io_read() const {
        return io_read;
    }

    const long long int& StorageUnit::get_io_write() const {
        return io_write;
    }

    const long long int& StorageUnit::get_io_activity() const {
        return io_activity;
    }

    const fs::path& StorageUnit::get_path() const {
        return path;
    }

    Data::Data(
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

    const GenericMemUnit& Data::get_total_ram_amount() const {
        return total_ram_amount;
    }

    const RamUnit& Data::get_available_ram_amount() const {
        return available_ram_amount;
    }

    const RamUnit& Data::get_cached_ram_amount() const {
        return cached_ram_amount;
    }

    const RamUnit& Data::get_free_ram_amount() const {
        return free_ram_amount;
    }

    const RamUnit& Data::get_used_ram_amount() const {
        return used_ram_amount;
    }

    const vector<StorageUnit>& Data::get_disks() const {
        return disks;
    }

    DataCollector::DataCollector() {
        shared::init();

        this->total_ram_amount = GenericMemUnit{this->get_total_ram_amount()};
        this->old_uptime = ut::system_uptime();
    }

    uint64_t DataCollector::get_total_ram_amount() {
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

    Data DataCollector::collect() {
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

            diskread.close();
        }
        old_uptime = uptime;

        vector<StorageUnit> dsk;

        for(auto& d: mem.disks) {
            dsk.push_back(StorageUnit {
                GenericMemUnit{d.second.total},
                GenericMemUnit{d.second.used},
                GenericMemUnit{d.second.free},
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
}
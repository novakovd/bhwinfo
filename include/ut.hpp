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

#ifndef HWINFO_UT_HPP
#define HWINFO_UT_HPP

#include <filesystem>
#include <utility>
#include <algorithm>
#include <fstream>
#include <ranges>
#include <unistd.h>
#include <vector>

using std::string;
using std::array;
using std::vector;
using std::string_view;

using namespace std::literals;

namespace fs = std::filesystem;
namespace rng = std::ranges;

namespace shared {
    inline fs::path proc_path, passwd_path;
    inline long page_size, clk_tck;
    inline fs::path freq_path;
    inline bool is_init{};

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


/** generic utils */
namespace ut {
    inline constexpr auto maxStreamSize = std::numeric_limits<std::streamsize>::max();

    inline double system_uptime() {
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

    /** file utils */
    namespace file {
        inline string read(const std::filesystem::path& path, const string& fallback = "");

        inline string read(const std::filesystem::path& path, const string& fallback) {
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

    /** string utils */
    namespace str {
        inline string capitalize(string str) {
            str.at(0) = toupper(str.at(0));

            return str;
        }

        template <typename T>
        inline bool contains(const string& str, const T& find_val) {
            return str.find(find_val) != string::npos;
        }

        inline auto split(const string& str, const char& delim = ' ') -> vector<string> {
            vector<string> out;

            for (const auto& s : str 	| rng::views::split(delim)
                                 | rng::views::transform([](auto &&rng) {
                return string_view(&*rng.begin(), rng::distance(rng));
            })) {
                if (not s.empty()) out.emplace_back(s);
            }

            return out;
        }

        inline string replace(const string& str, const string& from, const string& to) {
            string out = str;

            for (size_t start_pos = out.find(from); start_pos != string::npos; start_pos = out.find(from)) {
                out.replace(start_pos, from.length(), to);
            }

            return out;
        }

        inline string ltrim(const string& str, const string& t_str) {
            string_view str_v{str};

            while (str_v.starts_with(t_str))
                str_v.remove_prefix(t_str.size());

            return string{str_v};
        }

        inline string rtrim(const string& str, const string& t_str) {
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

    /** vector utils */
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

    /** type utils */
    namespace type {
        template<typename First, typename ... T>
        inline bool is_in(const First& first, const T& ... t) {
            return ((first == t) or ...);
        }
    }
}

#endif //HWINFO_UT_HPP

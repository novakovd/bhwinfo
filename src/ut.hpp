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
#include <fstream>
#include <ranges>
#include <atomic>
#include "ctime"
#include "shared.hpp"

using std::string;
using std::vector;
using std::string_view;
using std::atomic;

namespace fs = std::filesystem;
namespace rng = std::ranges;

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

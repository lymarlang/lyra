#ifndef WATCH_DAEMON_HH
#define WATCH_DAEMON_HH

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include <chrono>
#include <thread>
#include <map>

#if defined(__linux__)
#include <sys/inotify.h>
#include <unistd.h>
#include <poll.h>
#include <fcntl.h>
#endif

namespace fs = std::filesystem;

class WatchDaemon {
public:
    static void watch(const std::vector<std::string>& watch_dirs,
                      const std::function<void()>& on_change,
                      int max_runs = -1) {
        std::cout << "[Watch] Starting native file watch daemon...\n";
        for (const auto& d : watch_dirs) {
            std::cout << "  - Watching directory: " << d << "\n";
        }
        std::cout << "[Watch] Press Ctrl+C to stop.\n";

        // Initial run
        on_change();
        int runs = 1;
        if (max_runs > 0 && runs >= max_runs) return;

#if defined(__linux__)
        int inotify_fd = inotify_init1(IN_NONBLOCK);
        if (inotify_fd >= 0) {
            std::vector<int> wd_list;
            for (const auto& d : watch_dirs) {
                if (fs::exists(d)) {
                    int wd = inotify_add_watch(inotify_fd, d.c_str(), IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO);
                    if (wd >= 0) wd_list.push_back(wd);
                    try {
                        for (const auto& entry : fs::recursive_directory_iterator(d)) {
                            if (entry.is_directory()) {
                                int sub_wd = inotify_add_watch(inotify_fd, entry.path().string().c_str(),
                                                               IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_TO);
                                if (sub_wd >= 0) wd_list.push_back(sub_wd);
                            }
                        }
                    } catch (...) {}
                }
            }

            struct pollfd pfd;
            pfd.fd = inotify_fd;
            pfd.events = POLLIN;

            char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));

            while (max_runs <= 0 || runs < max_runs) {
                int poll_ret = poll(&pfd, 1, 300); // 300ms timeout
                if (poll_ret > 0 && (pfd.revents & POLLIN)) {
                    bool changed = false;
                    while (read(inotify_fd, buf, sizeof(buf)) > 0) {
                        changed = true;
                    }
                    if (changed) {
                        std::cout << "\n[Watch] Detected file system changes. Re-executing...\n";
                        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Debounce
                        on_change();
                        runs++;
                        if (max_runs > 0 && runs >= max_runs) break;
                    }
                }
            }

            for (int wd : wd_list) {
                inotify_rm_watch(inotify_fd, wd);
            }
            close(inotify_fd);
            return;
        }
#endif

        // Portable polling fallback (or macOS/Windows baseline)
        std::map<std::string, fs::file_time_type> last_timestamps;
        auto scan_timestamps = [&]() {
            std::map<std::string, fs::file_time_type> ts;
            for (const auto& d : watch_dirs) {
                if (!fs::exists(d)) continue;
                try {
                    for (const auto& entry : fs::recursive_directory_iterator(d)) {
                        if (entry.is_regular_file()) {
                            ts[entry.path().string()] = entry.last_write_time();
                        }
                    }
                } catch (...) {}
            }
            return ts;
        };

        last_timestamps = scan_timestamps();

        while (max_runs <= 0 || runs < max_runs) {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            auto current_timestamps = scan_timestamps();
            bool changed = false;
            if (current_timestamps.size() != last_timestamps.size()) {
                changed = true;
            } else {
                for (const auto& [p, t] : current_timestamps) {
                    if (last_timestamps.find(p) == last_timestamps.end() || last_timestamps[p] != t) {
                        changed = true;
                        break;
                    }
                }
            }

            if (changed) {
                std::cout << "\n[Watch] Detected file system changes. Re-executing...\n";
                last_timestamps = current_timestamps;
                on_change();
                runs++;
                if (max_runs > 0 && runs >= max_runs) break;
            }
        }
    }
};

#endif

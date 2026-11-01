#ifndef SEMVER_HH
#define SEMVER_HH

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
    std::string prerelease; // e.g. "alpha.1", "beta"
    std::string build;      // e.g. "20240322", ignored in comparison

    static bool is_digits(const std::string& str) {
        return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
    }

    static std::vector<std::string> split(const std::string& s, char delim) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delim)) {
            tokens.push_back(token);
        }
        return tokens;
    }

    static Version parse(const std::string& v_str) {
        Version v = {0, 0, 0, "", ""};
        std::string cleaned = v_str;
        while (!cleaned.empty() && std::isspace(static_cast<unsigned char>(cleaned.front()))) {
            cleaned.erase(cleaned.begin());
        }
        while (!cleaned.empty() && std::isspace(static_cast<unsigned char>(cleaned.back()))) {
            cleaned.pop_back();
        }
        if (!cleaned.empty() && (cleaned[0] == '^' || cleaned[0] == '~' || cleaned[0] == 'v' || cleaned[0] == '=')) {
            cleaned = cleaned.substr(1);
        }
        
        // Extract build metadata (+...)
        size_t plus_pos = cleaned.find('+');
        if (plus_pos != std::string::npos) {
            v.build = cleaned.substr(plus_pos + 1);
            cleaned = cleaned.substr(0, plus_pos);
        }

        // Extract prerelease (-...)
        size_t dash_pos = cleaned.find('-');
        if (dash_pos != std::string::npos) {
            v.prerelease = cleaned.substr(dash_pos + 1);
            cleaned = cleaned.substr(0, dash_pos);
        }

        std::stringstream ss(cleaned);
        std::string part;
        if (std::getline(ss, part, '.')) {
            try { v.major = std::stoi(part); } catch (...) { v.major = 0; }
        }
        if (std::getline(ss, part, '.')) {
            try { v.minor = std::stoi(part); } catch (...) { v.minor = 0; }
        }
        if (std::getline(ss, part, '.')) {
            try { v.patch = std::stoi(part); } catch (...) { v.patch = 0; }
        }
        
        return v;
    }

    static int compare_prerelease(const std::string& pre1, const std::string& pre2) {
        if (pre1.empty() && pre2.empty()) return 0;
        if (pre1.empty()) return 1;  // normal version > prerelease
        if (pre2.empty()) return -1; // prerelease < normal version

        auto parts1 = split(pre1, '.');
        auto parts2 = split(pre2, '.');

        size_t min_len = std::min(parts1.size(), parts2.size());
        for (size_t i = 0; i < min_len; ++i) {
            const auto& p1 = parts1[i];
            const auto& p2 = parts2[i];

            bool p1_num = is_digits(p1);
            bool p2_num = is_digits(p2);

            if (p1_num && p2_num) {
                long long n1 = std::stoll(p1);
                long long n2 = std::stoll(p2);
                if (n1 != n2) return (n1 < n2) ? -1 : 1;
            } else if (p1_num && !p2_num) {
                return -1;
            } else if (!p1_num && p2_num) {
                return 1;
            } else {
                if (p1 != p2) return (p1 < p2) ? -1 : 1;
            }
        }

        if (parts1.size() != parts2.size()) {
            return (parts1.size() < parts2.size()) ? -1 : 1;
        }

        return 0;
    }

    int compare(const Version& other) const {
        if (major != other.major) return (major < other.major) ? -1 : 1;
        if (minor != other.minor) return (minor < other.minor) ? -1 : 1;
        if (patch != other.patch) return (patch < other.patch) ? -1 : 1;
        return compare_prerelease(prerelease, other.prerelease);
    }

    bool operator<(const Version& other) const { return compare(other) < 0; }
    bool operator==(const Version& other) const { return compare(other) == 0; }
    bool operator!=(const Version& other) const { return compare(other) != 0; }
    bool operator<=(const Version& other) const { return compare(other) <= 0; }
    bool operator>(const Version& other) const { return compare(other) > 0; }
    bool operator>=(const Version& other) const { return compare(other) >= 0; }

    std::string to_string() const {
        std::string s = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
        if (!prerelease.empty()) s += "-" + prerelease;
        if (!build.empty()) s += "+" + build;
        return s;
    }
};

class Semver {
public:
    static std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    static bool satisfies_single(const Version& v, std::string req) {
        req = trim(req);
        if (req.empty() || req == "*") return true;

        if (req.find('x') != std::string::npos || req.find('X') != std::string::npos || req.find('*') != std::string::npos) {
            std::replace(req.begin(), req.end(), 'X', '*');
            std::replace(req.begin(), req.end(), 'x', '*');
            if (req.front() == '=' || req.front() == 'v') req = req.substr(1);
            req = trim(req);

            auto parts = Version::split(req, '.');
            if (parts.size() >= 1 && parts[0] != "*") {
                int maj = std::stoi(parts[0]);
                if (v.major != maj) return false;
            }
            if (parts.size() >= 2 && parts[1] != "*") {
                int min = std::stoi(parts[1]);
                if (v.minor != min) return false;
            }
            if (parts.size() >= 3 && parts[2] != "*") {
                int pat = std::stoi(parts[2]);
                if (v.patch != pat) return false;
            }
            return true;
        }

        if (req[0] == '^') {
            Version r = Version::parse(req.substr(1));
            if (v < r) return false;
            if (r.major > 0) {
                return v.major == r.major;
            } else if (r.minor > 0) {
                return v.major == 0 && v.minor == r.minor;
            } else {
                return v.major == 0 && v.minor == 0 && v.patch == r.patch;
            }
        } else if (req[0] == '~') {
            Version r = Version::parse(req.substr(1));
            if (v < r) return false;
            return v.major == r.major && v.minor == r.minor;
        } else if (req.rfind(">=", 0) == 0) {
            Version r = Version::parse(req.substr(2));
            return v >= r;
        } else if (req.rfind("<=", 0) == 0) {
            Version r = Version::parse(req.substr(2));
            return v <= r;
        } else if (req.rfind("!=", 0) == 0) {
            Version r = Version::parse(req.substr(2));
            return v != r;
        } else if (req[0] == '>') {
            Version r = Version::parse(req.substr(1));
            return v > r;
        } else if (req[0] == '<') {
            Version r = Version::parse(req.substr(1));
            return v < r;
        } else if (req[0] == '=') {
            Version r = Version::parse(req.substr(1));
            return v == r;
        } else {
            Version r = Version::parse(req);
            return v == r;
        }
    }

    static bool satisfies(const std::string& version_str, const std::string& requirement_str) {
        std::string req = trim(requirement_str);
        if (req.empty() || req == "*") return true;

        Version v = Version::parse(version_str);

        if (req.find("||") != std::string::npos) {
            auto or_parts = Version::split(req, '|');
            for (auto& part : or_parts) {
                part = trim(part);
                if (!part.empty() && satisfies(version_str, part)) {
                    return true;
                }
            }
            return false;
        }

        size_t hyphen_pos = req.find(" - ");
        if (hyphen_pos != std::string::npos) {
            std::string low_str = trim(req.substr(0, hyphen_pos));
            std::string high_str = trim(req.substr(hyphen_pos + 3));
            Version low = Version::parse(low_str);
            Version high = Version::parse(high_str);
            return (v >= low && v <= high);
        }

        std::string norm = req;
        for (char& c : norm) {
            if (c == ',') c = ' ';
        }

        std::stringstream ss(norm);
        std::string atom;
        std::vector<std::string> atoms;
        while (ss >> atom) {
            if (atom == ">=" || atom == "<=" || atom == "!=" || atom == ">" || atom == "<" || atom == "=") {
                std::string next_val;
                if (ss >> next_val) {
                    atoms.push_back(atom + next_val);
                }
            } else {
                atoms.push_back(atom);
            }
        }

        for (const auto& a : atoms) {
            if (!satisfies_single(v, a)) {
                return false;
            }
        }

        return true;
    }
};

#endif

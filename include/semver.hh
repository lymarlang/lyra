#ifndef SEMVER_HH
#define SEMVER_HH

#include <string>
#include <vector>
#include <sstream>
#include <iostream>

struct Version {
    int major;
    int minor;
    int patch;

    static Version parse(const std::string& v_str) {
        Version v = {0, 0, 0};
        std::string cleaned = v_str;
        if (!cleaned.empty() && (cleaned[0] == '^' || cleaned[0] == '~' || cleaned[0] == 'v')) {
            cleaned = cleaned.substr(1);
        }
        
        std::stringstream ss(cleaned);
        std::string part;
        if (std::getline(ss, part, '.')) v.major = std::stoi(part);
        if (std::getline(ss, part, '.')) v.minor = std::stoi(part);
        if (std::getline(ss, part, '.')) v.patch = std::stoi(part);
        
        return v;
    }

    bool operator<(const Version& other) const {
        if (major != other.major) return major < other.major;
        if (minor != other.minor) return minor < other.minor;
        return patch < other.patch;
    }

    bool operator==(const Version& other) const {
        return major == other.major && minor == other.minor && patch == other.patch;
    }

    bool operator<=(const Version& other) const {
        return *this < other || *this == other;
    }

    std::string to_string() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
};

class Semver {
public:
    static bool satisfies(const std::string& version, const std::string& requirement) {
        if (requirement.empty() || requirement == "*") return true;
        
        Version v = Version::parse(version);
        
        if (requirement[0] == '^') {
            Version req = Version::parse(requirement.substr(1));
            return v.major == req.major && req <= v;
        } else if (requirement[0] == '~') {
            Version req = Version::parse(requirement.substr(1));
            return v.major == req.major && v.minor == req.minor && req <= v;
        } else if (requirement.substr(0, 2) == ">=") {
            Version req = Version::parse(requirement.substr(2));
            return req <= v;
        } else {
            Version req = Version::parse(requirement);
            return v == req;
        }
    }
};

#endif

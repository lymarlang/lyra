#ifndef CONFIG_PARSER_HH
#define CONFIG_PARSER_HH

#include "nol.hh"
#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <vector>

class ConfigParser {
public:
    static std::string trim(const std::string& str, const std::string& chars = " \t\r\n\"") {
        size_t first = str.find_first_not_of(chars);
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(chars);
        return str.substr(first, (last - first + 1));
    }

    static PackageMetadata parse(const std::string& filename) {
        std::ifstream file(filename);
        PackageMetadata meta;
        if (!file.is_open()) return meta;

        std::string line;
        bool in_package_section = false;
        bool in_dependencies_section = false;
        bool in_features_section = false;
        while (std::getline(file, line)) {
            size_t comment_pos = line.find('#');
            if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
            
            std::string trimmed_line = trim(line, " \t\r\n");
            if (trimmed_line.empty()) continue;

            if (trimmed_line == "[package]") {
                in_package_section = true;
                in_dependencies_section = false;
                in_features_section = false;
                continue;
            } else if (trimmed_line == "[dependencies]") {
                in_dependencies_section = true;
                in_package_section = false;
                in_features_section = false;
                continue;
            } else if (trimmed_line == "[features]") {
                in_features_section = true;
                in_package_section = false;
                in_dependencies_section = false;
                continue;
            } else if (trimmed_line[0] == '[') {
                in_package_section = false;
                in_dependencies_section = false;
                in_features_section = false;
                continue;
            }

            size_t eq_pos = line.find('=');
            if (eq_pos == std::string::npos) continue;

            std::string key = trim(line.substr(0, eq_pos));
            std::string value = trim(line.substr(eq_pos + 1), " \t\r\n");

            if (in_package_section) {
                if (key == "name") meta.name = trim(value);
                else if (key == "version") meta.version = trim(value);
            } else if (in_dependencies_section) {
                Dependency dep;
                dep.name = key;

                if (!value.empty() && value.front() == '{') {
                    // Manual extraction for { key = value, ... }
                    size_t path_pos = value.find("path");
                    if (path_pos != std::string::npos) {
                        size_t s1 = value.find('\"', path_pos);
                        size_t s2 = value.find('\"', s1 + 1);
                        if (s1 != std::string::npos && s2 != std::string::npos) {
                            dep.path = value.substr(s1 + 1, s2 - s1 - 1);
                        }
                    }

                    size_t git_pos = value.find("git");
                    if (git_pos != std::string::npos) {
                        size_t s1 = value.find('\"', git_pos);
                        size_t s2 = value.find('\"', s1 + 1);
                        if (s1 != std::string::npos && s2 != std::string::npos) {
                            dep.git = value.substr(s1 + 1, s2 - s1 - 1);
                        }
                    }

                    size_t tag_pos = value.find("tag");
                    if (tag_pos != std::string::npos) {
                        size_t s1 = value.find('\"', tag_pos);
                        size_t s2 = value.find('\"', s1 + 1);
                        if (s1 != std::string::npos && s2 != std::string::npos) {
                            dep.tag = value.substr(s1 + 1, s2 - s1 - 1);
                        }
                    }
                    
                    size_t feat_pos = value.find("features");
                    if (feat_pos != std::string::npos) {
                        size_t b1 = value.find('[', feat_pos);
                        size_t b2 = value.find(']', b1 + 1);
                        if (b1 != std::string::npos && b2 != std::string::npos) {
                            std::string inner = value.substr(b1 + 1, b2 - b1 - 1);
                            size_t start = 0;
                            while (start < inner.size()) {
                                size_t q1 = inner.find('\"', start);
                                if (q1 == std::string::npos) break;
                                size_t q2 = inner.find('\"', q1 + 1);
                                if (q2 == std::string::npos) break;
                                dep.features.push_back(inner.substr(q1 + 1, q2 - q1 - 1));
                                start = q2 + 1;
                            }
                        }
                    }

                    size_t ver_pos = value.find("version");
                    if (ver_pos != std::string::npos) {
                        size_t s1 = value.find('\"', ver_pos);
                        size_t s2 = value.find('\"', s1 + 1);
                        if (s1 != std::string::npos && s2 != std::string::npos) {
                            dep.version = value.substr(s1 + 1, s2 - s1 - 1);
                        }
                    }
                } else {
                    dep.version = trim(value);
                }
                meta.dependencies[key] = dep;
            } else if (in_features_section) {
                std::vector<std::string> feat_list;
                size_t b1 = value.find('[');
                size_t b2 = value.find(']');
                if (b1 != std::string::npos && b2 != std::string::npos) {
                    std::string inner = value.substr(b1 + 1, b2 - b1 - 1);
                    size_t start = 0;
                    while (start < inner.size()) {
                        size_t q1 = inner.find('\"', start);
                        if (q1 == std::string::npos) break;
                        size_t q2 = inner.find('\"', q1 + 1);
                        if (q2 == std::string::npos) break;
                        feat_list.push_back(inner.substr(q1 + 1, q2 - q1 - 1));
                        start = q2 + 1;
                    }
                }
                meta.features[key] = feat_list;
            }
        }
        return meta;
    }
};

#endif

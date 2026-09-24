#include "handlers.hh"
#include "config_parser.hh"
#include "registry_client.hh"
#include "index_client.hh"
#include "crypto_helper.hh"
#include "lyra_common.hh"
#include "nol.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

static bool update_manifest_dependency(const std::string& pkg_name, const std::string& pkg_version, const std::string& path = "") {
    std::ifstream infile("lymar.nol");
    if (!infile.is_open()) {
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(infile)), std::istreambuf_iterator<char>());
    infile.close();

    try {
        NOL::Document doc = NOL::parse(content);
        NOL::Value root = doc.data();
        if (!root.isObject()) root = NOL::Object{};

        NOL::Object& obj = root.asObject();
        if (obj.find("dependencies") == obj.end() || !obj["dependencies"].isObject()) {
            obj["dependencies"] = NOL::Object{};
        }

        NOL::Object& deps = obj["dependencies"].asObject();
        if (path.empty()) {
            deps[pkg_name] = pkg_version;
        } else {
            NOL::Object dep_obj;
            dep_obj["path"] = path;
            deps[pkg_name] = dep_obj;
        }

        std::ofstream outfile("lymar.nol");
        if (!outfile.is_open()) return false;
        outfile << "# Lymar project configuration\n\n";
        outfile << root.dump(2, 0, true);
        outfile.close();
        return true;
    } catch (...) {
        return false;
    }
}

int handle_publish(int argc, char** argv) {
    (void)argc;
    (void)argv;
    try {
        PackageMetadata meta = ConfigParser::parse("lymar.nol");
        if (meta.name.empty()) {
            std::cerr << "error: could not find lymar.nol or package name\n";
            return 1;
        }

        if (!Lyra::is_safe_string(meta.name) || !Lyra::is_safe_string(meta.version)) {
            std::cerr << "error: unsafe package name or version\n";
            return 1;
        }

        std::cout << "Packaging project '" << meta.name << "' version " << meta.version << "...\n";
        
        std::string tarball = meta.name + "-" + meta.version + ".tar.gz";
        std::string pack_cmd = "tar -czf \"" + tarball + "\" src lymar.nol";
        if (std::system(pack_cmd.c_str()) != 0) {
            std::cerr << "error: failed to create tarball\n";
            return 1;
        }

        std::string file_hash = CryptoHelper::sha256_file(tarball);

        // Sync index repository
        IndexClient::sync_index(true);
        fs::path index_dir = Lyra::get_index_dir();
        fs::path pkg_index_file = index_dir / "packages" / (meta.name + ".nol");
        fs::create_directories(pkg_index_file.parent_path());

        IndexPackage pkg;
        bool pkg_exists = IndexClient::get_package(meta.name, pkg);
        pkg.name = meta.name;

        // ── Ownership verification ──
        auto [pub_name, pub_email] = Lyra::get_git_identity();
        if (pub_email.empty()) {
            std::cerr << "error: git user.email is not configured.\n";
            std::cerr << "  Run: git config --global user.email \"you@example.com\"\n";
            return 1;
        }

        PackageOwner current_user;
        current_user.name = pub_name;
        current_user.email = pub_email;

        if (pkg_exists && !pkg.owners.empty()) {
            // Package has existing owners — verify the current user is one of them
            bool is_owner = false;
            for (const auto& owner : pkg.owners) {
                if (owner.matches(current_user)) {
                    is_owner = true;
                    break;
                }
            }
            if (!is_owner) {
                std::cerr << "error: permission denied.\n";
                std::cerr << "  Package '" << meta.name << "' is owned by:\n";
                for (const auto& owner : pkg.owners) {
                    std::cerr << "    - " << owner.name << " <" << owner.email << ">\n";
                }
                std::cerr << "  Your identity: " << pub_name << " <" << pub_email << ">\n";
                std::cerr << "  Only package owners can publish updates.\n";
                return 1;
            }
            std::cout << "Ownership verified: " << pub_name << " <" << pub_email << ">\n";
        } else {
            // First publish — register the current user as the owner
            pkg.owners.clear();
            pkg.owners.push_back(current_user);
            std::cout << "Registering owner: " << pub_name << " <" << pub_email << ">\n";
        }

        IndexVersion iv;
        iv.version = meta.version;
        iv.hash = "sha256:" + file_hash;

        std::string git_url = meta.repository;
        if (git_url.empty()) {
            std::string tmp_git = ".git_remote.tmp";
            std::system(("git config --get remote.origin.url > \"" + tmp_git + "\" " + LYRA_DEV_NULL).c_str());
            std::ifstream gf(tmp_git);
            if (gf.is_open()) {
                std::getline(gf, git_url);
                gf.close();
                fs::remove(tmp_git);
            }
        }
        while (!git_url.empty() && (git_url.back() == '\n' || git_url.back() == '\r' || git_url.back() == ' ')) {
            git_url.pop_back();
        }

        if (!git_url.empty()) {
            iv.git = git_url;
            iv.tag = "v" + meta.version;
            if (pkg.repository.empty()) {
                pkg.repository = git_url;
            }
        }

        if (pkg.description.empty() && !meta.description.empty()) {
            pkg.description = meta.description;
        }
        if (pkg.repository.empty() && !meta.repository.empty()) {
            pkg.repository = meta.repository;
        }

        for (const auto& [dep_name, dep] : meta.dependencies) {
            iv.dependencies[dep_name] = dep.version.empty() ? "*" : dep.version;
        }

        pkg.versions[meta.version] = iv;

        NOL::Builder builder;
        NOL::Object pkg_obj;
        pkg_obj["name"] = pkg.name;
        if (!pkg.description.empty()) pkg_obj["description"] = pkg.description;
        if (!pkg.repository.empty()) pkg_obj["repository"] = pkg.repository;
        builder.set("package", pkg_obj);

        // Write owners list
        NOL::Array owners_arr;
        for (const auto& owner : pkg.owners) {
            NOL::Object owner_obj;
            owner_obj["name"] = owner.name;
            owner_obj["email"] = owner.email;
            owners_arr.push_back(owner_obj);
        }
        builder.set("owners", owners_arr);

        NOL::Object versions_obj;
        for (const auto& [ver, v_info] : pkg.versions) {
            NOL::Object v_obj;
            if (!v_info.url.empty()) v_obj["url"] = v_info.url;
            if (!v_info.git.empty()) v_obj["git"] = v_info.git;
            if (!v_info.tag.empty()) v_obj["tag"] = v_info.tag;
            if (!v_info.hash.empty()) v_obj["hash"] = v_info.hash;
            if (!v_info.dependencies.empty()) {
                NOL::Object deps_obj;
                for (const auto& [d_name, d_ver] : v_info.dependencies) {
                    deps_obj[d_name] = d_ver;
                }
                v_obj["dependencies"] = deps_obj;
            }
            versions_obj[ver] = v_obj;
        }
        builder.set("versions", versions_obj);

        std::ofstream out_index(pkg_index_file);
        if (out_index.is_open()) {
            out_index << "# Lyra Package Index Entry\n\n";
            out_index << builder.build().dump(2);
            out_index.close();

            std::cout << "\n[OK] Package manifest generated at:\n  " << pkg_index_file.string() << "\n";

            // Automatically commit and push to the central index
            std::string idx = index_dir.string();
            std::string rel_path = "packages/" + meta.name + ".nol";
            std::string commit_msg = "Publish " + meta.name + " " + meta.version;

            std::cout << "Publishing to central index (" << Lyra::get_default_index_url() << ")...\n";

            std::string git_add = "git -C \"" + idx + "\" add \"" + rel_path + "\"";
            if (std::system(git_add.c_str()) != 0) {
                std::cerr << "error: git add failed in index repo\n";
                return 1;
            }

            // Check if there are actual changes to commit
            std::string git_diff = "git -C \"" + idx + "\" diff --cached --quiet";
            int diff_result = std::system(git_diff.c_str());
            if (diff_result == 0) {
                std::cout << "Package " << meta.name << "@" << meta.version << " is already published (no changes).\n";
            } else {
                std::string git_commit = "git -C \"" + idx + "\" commit -m \"" + commit_msg + "\"";
                if (std::system(git_commit.c_str()) != 0) {
                    std::cerr << "error: git commit failed in index repo\n";
                    return 1;
                }

                // Pull latest changes before pushing to avoid non-fast-forward rejections
                std::string git_pull = "git -C \"" + idx + "\" pull --rebase origin main";
                std::system(git_pull.c_str());

                std::string git_push = "git -C \"" + idx + "\" push origin main";
                if (std::system(git_push.c_str()) != 0) {
                    std::cerr << "error: git push failed. Check your credentials and remote access.\n";
                    return 1;
                }

                std::cout << "\n[OK] Published " << meta.name << "@" << meta.version << " to " << Lyra::get_default_index_url() << "\n";
            }
        }

        char* reg_url = std::getenv("LYRA_REGISTRY_URL");
        if (reg_url && *reg_url) {
            RegistryClient::publish(meta.name, meta.version, tarball);
        }

        fs::remove(tarball);
        std::cout << "Package " << meta.name << "@" << meta.version << " ready.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error in handle_publish: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "unknown error in handle_publish\n";
        return 1;
    }
}

int handle_add(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "error: missing package name\n";
        return 1;
    }

    std::string pkg_name = argv[2];
    std::string pkg_version = (argc > 3) ? argv[3] : "*";

    if (!Lyra::is_safe_string(pkg_name)) {
        std::cerr << "error: unsafe package name\n";
        return 1;
    }

    std::cout << "Adding dependency '" << pkg_name << "' (" << pkg_version << ")...\n";

    if (update_manifest_dependency(pkg_name, pkg_version)) {
        std::cout << "Updated lymar.nol. Run 'lyra update' to fetch dependencies.\n";
        return 0;
    } else {
        std::cerr << "error: could not update lymar.nol\n";
        return 1;
    }
}

int handle_link(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "error: missing path to link\n";
        std::cerr << "usage: lyra link <path>\n";
        return 1;
    }

    fs::path target_path = fs::absolute(argv[2]);
    if (!fs::exists(target_path / "lymar.nol")) {
        std::cerr << "error: no lymar.nol found at " << target_path << "\n";
        return 1;
    }

    PackageMetadata target_meta = ConfigParser::parse((target_path / "lymar.nol").string());
    if (target_meta.name.empty()) {
        std::cerr << "error: could not parse lymar.nol at " << target_path << "\n";
        return 1;
    }

    std::cout << "Linking package '" << target_meta.name << "' from " << target_path << "...\n";

    if (update_manifest_dependency(target_meta.name, "", target_path.string())) {
        std::cout << "Successfully linked '" << target_meta.name << "'.\n";
        return 0;
    } else {
        std::cerr << "error: could not update lymar.nol\n";
        return 1;
    }
}

int handle_pack(int argc, char** argv) {
    PackageMetadata meta = ConfigParser::parse("lymar.nol");
    if (meta.name.empty()) {
        std::cerr << "error: could not find lymar.nol or package name\n";
        return 1;
    }

    if (!Lyra::is_safe_string(meta.name) || !Lyra::is_safe_string(meta.version)) {
        std::cerr << "error: unsafe package name or version in lymar.nol\n";
        return 1;
    }

    std::string tarball = meta.name + "-" + meta.version + ".tar.gz";
    std::cout << "Packaging " << meta.name << " v" << meta.version << " into " << tarball << "...\n";

    std::string pack_cmd = "tar -czf \"" + tarball + "\" src lymar.nol";
    int result = std::system(pack_cmd.c_str());

    if (result == 0) {
        std::cout << "Successfully packaged to " << tarball << "\n";
    } else {
        std::cerr << "error: failed to create package\n";
    }

    return (result == 0) ? 0 : 1;
}

int handle_install(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "error: missing path to package tarball\n";
        std::cerr << "usage: lyra install <path.tar.gz>\n";
        return 1;
    }

    fs::path tar_path = fs::absolute(argv[2]);
    if (!fs::exists(tar_path)) {
        std::cerr << "error: file not found: " << tar_path << "\n";
        return 1;
    }

    fs::path temp_dir = fs::temp_directory_path() / ("lyra_install_temp_" + std::to_string(Lyra::get_process_id()));
    fs::create_directories(temp_dir);

    std::string extract_cmd = "tar -xzf \"" + tar_path.string() + "\" -C \"" + temp_dir.string() + "\"";
    if (std::system(extract_cmd.c_str()) != 0) {
        std::cerr << "error: failed to extract " << tar_path << "\n";
        fs::remove_all(temp_dir);
        return 1;
    }

    PackageMetadata meta = ConfigParser::parse((temp_dir / "lymar.nol").string());
    if (meta.name.empty()) {
        std::cerr << "error: could not parse lymar.nol in tarball\n";
        fs::remove_all(temp_dir);
        return 1;
    }

    if (!Lyra::is_safe_string(meta.name) || !Lyra::is_safe_string(meta.version)) {
        std::cerr << "error: unsafe package name or version in tarball\n";
        fs::remove_all(temp_dir);
        return 1;
    }

    fs::path cache_dir = Lyra::get_cache_dir();
    fs::path pkg_cache_path = cache_dir / (meta.name + "-" + meta.version);

    fs::create_directories(cache_dir);
    if (fs::exists(pkg_cache_path)) {
        fs::remove_all(pkg_cache_path);
    }

    std::error_code ec;
    fs::rename(temp_dir, pkg_cache_path, ec);
    if (ec) {
        fs::create_directories(pkg_cache_path);
        fs::copy(temp_dir, pkg_cache_path, fs::copy_options::recursive);
        fs::remove_all(temp_dir);
    }

    std::cout << "Installed " << meta.name << "@" << meta.version << " to cache.\n";

    if (update_manifest_dependency(meta.name, meta.version)) {
        std::cout << "Added " << meta.name << " to lymar.nol\n";
    }

    return 0;
}

int handle_search(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "error: missing search query\n";
        return 1;
    }

    std::string query = argv[2];
    std::cout << "Searching for '" << query << "' in index (" << Lyra::get_default_index_url() << ")...\n";
    auto results = IndexClient::search(query);
    if (results.empty()) {
        std::cout << "No packages found matching '" << query << "'.\n";
        return 0;
    }

    std::cout << "Found " << results.size() << " package(s):\n";
    for (const auto& pkg : results) {
        std::string latest_ver = "unknown";
        if (!pkg.versions.empty()) {
            latest_ver = pkg.versions.rbegin()->first;
        }
        std::cout << "  - " << pkg.name << " (" << latest_ver << ")";
        if (!pkg.description.empty()) {
            std::cout << " - " << pkg.description;
        }
        std::cout << "\n";
    }
    return 0;
}

int handle_info(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "error: missing package name\n";
        return 1;
    }

    std::string name = argv[2];
    IndexPackage pkg;
    if (!IndexClient::get_package(name, pkg)) {
        std::cerr << "error: package '" << name << "' not found in index (" << Lyra::get_default_index_url() << ")\n";
        return 1;
    }

    std::cout << "Package: " << pkg.name << "\n";
    if (!pkg.description.empty()) {
        std::cout << "Description: " << pkg.description << "\n";
    }
    if (!pkg.repository.empty()) {
        std::cout << "Repository: " << pkg.repository << "\n";
    }
    std::cout << "Versions (" << pkg.versions.size() << "):\n";
    for (const auto& [ver, details] : pkg.versions) {
        std::cout << "  - " << ver;
        if (!details.git.empty()) {
            std::cout << " [git: " << details.git << (details.tag.empty() ? "" : ("@" + details.tag)) << "]";
        } else if (!details.url.empty()) {
            std::cout << " [tarball]";
        }
        std::cout << "\n";
    }
    return 0;
}

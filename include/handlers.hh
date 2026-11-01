#ifndef HANDLERS_H
#define HANDLERS_H

#include <string>
#include <vector>

int handle_init();
int handle_new(int argc, char** argv);
int handle_repl(int argc, char** argv);
int handle_expand(int argc, char** argv);
int handle_run(int argc, char** argv);
int handle_build(int argc, char** argv);
int handle_update(int argc, char** argv);
int handle_add(int argc, char** argv);
int handle_link(int argc, char** argv);
int handle_publish(int argc, char** argv);
int handle_pack(int argc, char** argv);
int handle_install(int argc, char** argv);
int handle_search(int argc, char** argv);
int handle_info(int argc, char** argv);
int handle_test(int argc, char** argv);
int handle_deps(int argc, char** argv);
int handle_tree(int argc, char** argv);
int handle_why(int argc, char** argv);
int handle_sign(int argc, char** argv);
int handle_verify(int argc, char** argv);
int handle_keygen(int argc, char** argv);
int handle_clean();
int handle_cache_clear();
int handle_doctor();
int handle_env();
int handle_audit(int argc, char** argv);
int handle_license(int argc, char** argv);
int handle_sbom(int argc, char** argv);
int handle_watch(int argc, char** argv);
int handle_bench(int argc, char** argv);
int handle_lint(int argc, char** argv);
int handle_doc(int argc, char** argv);
int handle_profile(int argc, char** argv);
int handle_deb(int argc, char** argv);
int handle_msi(int argc, char** argv);
int handle_appimage(int argc, char** argv);
int handle_snap(int argc, char** argv);
int handle_flatpak(int argc, char** argv);
int handle_zip(int argc, char** argv);
int handle_dmg(int argc, char** argv);
int handle_toolchain(int argc, char** argv);
int handle_lsp(int argc, char** argv);
int handle_fmt(int argc, char** argv);
int handle_vendor(int argc, char** argv);

#endif

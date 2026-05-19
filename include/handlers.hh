#ifndef HANDLERS_H
#define HANDLERS_H

#include <string>
#include <vector>

int handle_init();
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
int handle_clean();
int handle_cache_clear();
int handle_doctor();
int handle_env();
int handle_audit(int argc, char** argv);
int handle_bench(int argc, char** argv);

#endif

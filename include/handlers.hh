#ifndef HANDLERS_H
#define HANDLERS_H

int handle_init();
int handle_run(int argc, char** argv);
int handle_build(int argc, char** argv);
int handle_update(int argc, char** argv);
int handle_add(int argc, char** argv);
int handle_publish(int argc, char** argv);
int handle_test(int argc, char** argv);
int handle_deps(int argc, char** argv);
int handle_doctor();

#endif

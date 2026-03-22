#ifndef PROCESS_HELPER_HH
#define PROCESS_HELPER_HH

#include <string>
#include <vector>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace Lyra {

inline int run_command(const std::string& executable, const std::vector<std::string>& args) {
#ifdef _WIN32
    std::string command_line = "\"" + executable + "\"";
    for (const auto& arg : args) {
        command_line += " \"" + arg + "\"";
    }
    
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessA(NULL, (char*)command_line.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return -1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(exit_code);
#else
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        std::vector<char*> c_args;
        c_args.push_back(const_cast<char*>(executable.c_str()));
        for (const auto& arg : args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);

        // For debug
        // std::cout << "Executing: " << executable << std::endl;
        // for (auto arg : args) std::cout << "  arg: " << arg << std::endl;
        
        execvp(executable.c_str(), c_args.data());
        // If execvp returns, it failed
        perror("execvp");
        std::cerr << "error: failed to execute " << executable << "\n";
        exit(1);
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        return -1;
    } else {
        return -1;
    }
#endif
}

} // namespace Lyra

#endif

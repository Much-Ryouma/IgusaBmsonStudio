#include "ibs/infra/process/ProcessRunner.hpp"

#include "ibs/infra/fs/Utf8Path.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace ibs::infra {

// Why not 引数を空白でつなぐだけにする: Windows ではコマンドラインを 1 本の文字列で渡し、受け取る側が規則に従って
// 分け直すため、空白・引用符・末尾の \ を含むパス（曲のフォルダ名など）は規則どおりに引用しないと別の引数に割れる
std::wstring quoteWindowsArgument(const std::wstring& argument) {
    if (!argument.empty() && argument.find_first_of(L" \t\"") == std::wstring::npos) {
        return argument;
    }
    std::wstring quoted = L"\"";
    std::size_t backslashes = 0;
    for (const wchar_t character : argument) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }
        if (character == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
        } else {
            quoted.append(backslashes, L'\\');
        }
        backslashes = 0;
        quoted.push_back(character);
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

int runProcessAndWait(const std::string& program, const std::vector<std::string>& arguments,
                      const std::filesystem::path& workingDirectory) {
#if defined(_WIN32)
    std::wstring commandLine = quoteWindowsArgument(pathFromUtf8(program).wstring());
    for (const auto& argument : arguments) {
        commandLine += L' ';
        commandLine += quoteWindowsArgument(pathFromUtf8(argument).wstring());
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const std::wstring directory = workingDirectory.wstring();
    if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        directory.empty() ? nullptr : directory.c_str(), &startup, &process)) {
        throw ProcessError("起動できません: " + program);
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(exitCode);
#else
    const pid_t child = fork();
    if (child < 0) {
        throw ProcessError("起動できません: " + program);
    }
    if (child == 0) {
        if (!workingDirectory.empty() && chdir(workingDirectory.c_str()) != 0) {
            _exit(127);
        }
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(program.c_str()));
        for (const auto& argument : arguments) {
            argv.push_back(const_cast<char*>(argument.c_str()));
        }
        argv.push_back(nullptr);
        execvp(program.c_str(), argv.data());
        _exit(127);
    }
    int status = 0;
    if (waitpid(child, &status, 0) < 0) {
        throw ProcessError("終了を待てません: " + program);
    }
    const int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    if (exitCode == 127) {
        throw ProcessError("起動できません: " + program);
    }
    return exitCode;
#endif
}

}  // namespace ibs::infra

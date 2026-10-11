#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace ibs::infra {

class ProcessError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

[[nodiscard]] std::wstring quoteWindowsArgument(const std::wstring& argument);

int runProcessAndWait(const std::string& program, const std::vector<std::string>& arguments,
                      const std::filesystem::path& workingDirectory);

}  // namespace ibs::infra

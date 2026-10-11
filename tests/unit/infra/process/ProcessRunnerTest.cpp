#include <gtest/gtest.h>

#include "ibs/infra/process/ProcessRunner.hpp"

namespace ibs::infra {
namespace {

TEST(ProcessRunner, PRC001_QuotesWindowsArgumentsSoTheyStayWhole) {
    EXPECT_EQ(quoteWindowsArgument(L"plain"), L"plain");
    EXPECT_EQ(quoteWindowsArgument(L""), L"\"\"");
    EXPECT_EQ(quoteWindowsArgument(L"C:\\My Songs\\a.bmson"), L"\"C:\\My Songs\\a.bmson\"");
    EXPECT_EQ(quoteWindowsArgument(L"C:\\My Songs\\"), L"\"C:\\My Songs\\\\\"");
    EXPECT_EQ(quoteWindowsArgument(L"say \"hi\""), L"\"say \\\"hi\\\"\"");
}

#if !defined(_WIN32)
TEST(ProcessRunner, PRC002_ReturnsTheExitCodeOrThrowsWhenItCannotStart) {
    EXPECT_EQ(runProcessAndWait("sh", {"-c", "exit 3"}, {}), 3);
    EXPECT_EQ(runProcessAndWait("sh", {"-c", "test \"$(pwd)\" = /"}, "/"), 0);
    EXPECT_THROW(static_cast<void>(runProcessAndWait("ibs-no-such-program", {}, {})), ProcessError);
}
#endif

}  // namespace
}  // namespace ibs::infra

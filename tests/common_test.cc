#include "distribuild/common/logging.h"

#include "gtest/gtest.h"

static void test_logging() {
    LOG_DEBUG("debug");
    LOG_TRACE("trace");
    LOG_INFO("info");
    LOG_WARN("warn");
    LOG_ERROR("error");
}

TEST(LogFatal, test) {
    EXPECT_EXIT({
        LOG_FATAL("Testing LOG_FATAL");
    }, ::testing::KilledBySignal(SIGABRT), "");
}

int distribuild::min_log_level = 0;

int main(int argc, char** argv) {
    test_logging();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
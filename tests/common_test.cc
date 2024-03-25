#include "distribuild/common/logging.h"
#include "distribuild/common/dir.h"

#include "gtest/gtest.h"

static void test_logging() {
    LOG_DEBUG("debug [{}]", 1);
    LOG_TRACE("trace");
    LOG_INFO("info");
    LOG_WARN("warn");
    LOG_ERROR("error");
}

TEST(logging, common_mod_test) {
    EXPECT_EXIT({
        LOG_FATAL("Testing LOG_FATAL");
    }, ::testing::KilledBySignal(SIGABRT), "");

    EXPECT_EXIT({
        DISTBU_CHECK(0 == 1, "test");
    }, ::testing::KilledBySignal(SIGABRT), "");
}

TEST(dir, common_mod_test) {
    distbu::MkDir("test/test");
    distbu::RemoveDir("test/test");
    distbu::RemoveDir("test");
}

int distbu::min_log_level = 0;

int main(int argc, char** argv) {
    test_logging();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
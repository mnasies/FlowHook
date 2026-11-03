#include <filesystem>
#include <fstream>


#include "../../src/include/flowhook_core.h"
#include "../utest.h"

using namespace std;
using namespace flowhook;
namespace fs = std::filesystem;

bool FLOWHOOK_DEBUG = false;
bool FLOWHOOK_VERBOSE = false;
bool FLOWHOOK_QUIET = false;

static const char* LAZY_CONFIG_DIR = "/tmp/fh_lazy_test";

// Tasks are persisted into a fresh config dir by a temporary FULL core in
// setup. The core under test here is created lazily, so it has no task
// runners and must rely on the config fallback for read-only methods.
struct LazyFixture
{
    FlowHookCore* fh;
};

UTEST_F_SETUP(LazyFixture)
{
    fs::remove_all(LAZY_CONFIG_DIR);
    fs::create_directories(LAZY_CONFIG_DIR);
    setenv("FLOWHOOK_CONFIG_DIR_TEST", LAZY_CONFIG_DIR, 1);

    // task working dirs must exist for create_task to succeed
    fs::create_directories("/tmp/fh_test");
    fs::create_directories("/tmp/fh_test2");

    {
        auto f = FlowHookCore::create();
        ASSERT_TRUE(f.isOk());
        auto* full = f.unwrap();
        ASSERT_TRUE(full->create_task("task_1", "/tmp/fh_test").isOk());
        ASSERT_TRUE(full->create_task("task_2", "/tmp/fh_test2").isOk());
        ASSERT_TRUE(full->set_depth("/tmp/fh_test", 3).isOk());
        ASSERT_TRUE(full->set_timeout("/tmp/fh_test", 42).isOk());
        ASSERT_TRUE(full->activate_task("/tmp/fh_test2").isOk());
        delete full;
    }

    auto l = FlowHookCore::lazy_create();
    ASSERT_TRUE(l.isOk());
    utest_fixture->fh = l.unwrap();
}

UTEST_F_TEARDOWN(LazyFixture)
{
    delete utest_fixture->fh;
    fs::remove_all(LAZY_CONFIG_DIR);
    unsetenv("FLOWHOOK_CONFIG_DIR_TEST");
}

// ---------------------------------------------------------------------------------
// LAZY CREATION
// ---------------------------------------------------------------------------------

// renamed neighbor working dirs must not break lazy init
UTEST_F(LazyFixture, lazy_create_survives_renamed_task_dir)
{
    fs::rename("/tmp/fh_test2", "/tmp/fh_test2_renamed");

    auto l = FlowHookCore::lazy_create();
    EXPECT_TRUE(l.isOk());
    if (l.isOk()) {
        auto* fh = l.unwrap();
        // both tasks are still in the persisted config
        auto tasks = fh->get_tasks();
        EXPECT_EQ(tasks.size(), 2);
        delete fh;

        // restore so teardown state stays consistent
        fs::rename("/tmp/fh_test2_renamed", "/tmp/fh_test2");
    }
}

// ---------------------------------------------------------------------------------
// CONFIG FALLBACK FOR READ-ONLY METHODS
// ---------------------------------------------------------------------------------

UTEST_F(LazyFixture, is_task_persisted)
{
    EXPECT_TRUE(utest_fixture->fh->is_task("/tmp/fh_test"));
    EXPECT_TRUE(utest_fixture->fh->is_task("/tmp/fh_test2"));
}

UTEST_F(LazyFixture, is_task_unknown_id)
{
    EXPECT_FALSE(utest_fixture->fh->is_task("/tmp/fh_nonexistent"));
}

UTEST_F(LazyFixture, is_task_active_persisted)
{
    // task_1 was never activated -> inactive
    auto r1 = utest_fixture->fh->is_task_active("/tmp/fh_test");
    ASSERT_TRUE(r1.isOk());
    EXPECT_FALSE(r1.unwrap());

    // task_2 was activated in setup -> active
    auto r2 = utest_fixture->fh->is_task_active("/tmp/fh_test2");
    ASSERT_TRUE(r2.isOk());
    EXPECT_TRUE(r2.unwrap());
}

UTEST_F(LazyFixture, is_task_active_unknown_id)
{
    EXPECT_TRUE(utest_fixture->fh->is_task_active("/tmp/fh_nonexistent").isErr());
}

UTEST_F(LazyFixture, get_task_depth_persisted)
{
    auto r = utest_fixture->fh->get_task_depth("/tmp/fh_test");
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.unwrap(), 3);
}

UTEST_F(LazyFixture, get_task_depth_unknown_id)
{
    EXPECT_TRUE(utest_fixture->fh->get_task_depth("/tmp/fh_nonexistent").isErr());
}

UTEST_F(LazyFixture, get_task_timeout_persisted)
{
    auto r = utest_fixture->fh->get_task_timeout("/tmp/fh_test");
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.unwrap(), 42);
}

UTEST_F(LazyFixture, get_task_timeout_unknown_id)
{
    EXPECT_TRUE(utest_fixture->fh->get_task_timeout("/tmp/fh_nonexistent").isErr());
}

UTEST_F(LazyFixture, get_tasks_lists_persisted_tasks)
{
    auto tasks = utest_fixture->fh->get_tasks();
    EXPECT_EQ(tasks.size(), 2);
    EXPECT_EQ(tasks[0].id, "/tmp/fh_test");
    EXPECT_EQ(tasks[1].id, "/tmp/fh_test2");
}

UTEST_F(LazyFixture, reanchor_on_is_task_when_dir_renamed)
{
    fs::rename("/tmp/fh_test2", "/tmp/fh_test2_renamed");

    // Before re-anchoring, old path was registered. Now check on the new path
    // must detect the marker, re-anchor the task, and return true.
    EXPECT_TRUE(utest_fixture->fh->is_task("/tmp/fh_test2_renamed"));

    // Config must now reflect the new working directory
    auto r = utest_fixture->fh->is_task_active("/tmp/fh_test2_renamed");
    ASSERT_TRUE(r.isOk());
    EXPECT_TRUE(r.unwrap());

    // Old working directory is no longer registered
    EXPECT_FALSE(utest_fixture->fh->is_task("/tmp/fh_test2"));

    // Clean up
    fs::rename("/tmp/fh_test2_renamed", "/tmp/fh_test2");
}

UTEST_F(LazyFixture, reanchor_refused_if_original_dir_still_exists)
{
    fs::create_directories("/tmp/fh_duplicate");
    std::error_code ec;
    fs::copy_file("/tmp/fh_test/.flowhook", "/tmp/fh_duplicate/.flowhook", ec);
    ASSERT_FALSE(ec);

    // /tmp/fh_test still exists on disk, so re-anchoring must NOT adopt /tmp/fh_duplicate
    EXPECT_FALSE(utest_fixture->fh->is_task("/tmp/fh_duplicate"));

    fs::remove_all("/tmp/fh_duplicate");
}

UTEST_F(LazyFixture, reanchor_refused_if_marker_corrupted)
{
    fs::rename("/tmp/fh_test2", "/tmp/fh_test2_corrupt");
    std::ofstream("/tmp/fh_test2_corrupt/.flowhook") << "not valid json";

    EXPECT_FALSE(utest_fixture->fh->is_task("/tmp/fh_test2_corrupt"));

    fs::remove_all("/tmp/fh_test2_corrupt");
    fs::create_directories("/tmp/fh_test2");
}

// ////////////////////////////////////////////////////////////////////////////////////////
// Full core must not be affected by the read-method refactor: the same lookups
// through a FULL core still work.

struct FullFixture
{
    FlowHookCore* fh;
};

UTEST_F_SETUP(FullFixture)
{
    fs::remove_all("/tmp/fh_full_test");
    fs::create_directories("/tmp/fh_full_test");
    setenv("FLOWHOOK_CONFIG_DIR_TEST", "/tmp/fh_full_test", 1);

    fs::create_directories("/tmp/fh_test");

    auto f = FlowHookCore::create();
    ASSERT_TRUE(f.isOk());
    utest_fixture->fh = f.unwrap();
    ASSERT_TRUE(utest_fixture->fh->create_task("task_1", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->set_depth("/tmp/fh_test", 2).isOk());
}

UTEST_F_TEARDOWN(FullFixture)
{
    delete utest_fixture->fh;
    fs::remove_all("/tmp/fh_full_test");
    unsetenv("FLOWHOOK_CONFIG_DIR_TEST");
}

UTEST_F(FullFixture, full_core_read_methods_still_work)
{
    EXPECT_TRUE(utest_fixture->fh->is_task("/tmp/fh_test"));
    auto d = utest_fixture->fh->get_task_depth("/tmp/fh_test");
    ASSERT_TRUE(d.isOk());
    EXPECT_EQ(d.unwrap(), 2);
}

UTEST_MAIN()

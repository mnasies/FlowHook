#include <filesystem>


#include "../../src/include/flowhook_core.h"
#include "../utest.h"
#include "../../src/include/types.h"

using namespace std;
using namespace flowhook;
namespace fs = std::filesystem;

bool FLOWHOOK_DEBUG = false;
bool FLOWHOOK_VERBOSE = false;
bool FLOWHOOK_QUIET = false;


struct FlowHookFixture
{
    FlowHookCore* fh;
};

UTEST_F_SETUP(FlowHookFixture)
{
    fs::create_directories("/tmp/fh_test");
    fs::create_directories("/tmp/fh_test2");
    fs::create_directories("/tmp/fh_test/task_test");
    setenv("FLOWHOOK_CONFIG_DIR_TEST", "/tmp/fh_test", 1);

    auto f = FlowHookCore::create();
    ASSERT_TRUE(f.isOk());
    utest_fixture->fh = f.unwrap();
}

UTEST_F_TEARDOWN(FlowHookFixture)
{
    delete utest_fixture->fh;
    fs::remove_all("/tmp/fh_test");
    unsetenv("FLOWHOOK_CONFIG_DIR_TEST");
}

// ---------------------------------------------------------------------------------
// CREATE / DELELTE TASK
// ---------------------------------------------------------------------------------

UTEST_F(FlowHookFixture, create_task)
{
    EXPECT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
}
UTEST_F(FlowHookFixture, create_empty_task)
{
    EXPECT_TRUE(utest_fixture->fh->create_task("", "/tmp/fh_test").isErr());
}
UTEST_F(FlowHookFixture, create_task_twice)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isErr());
}
UTEST_F(FlowHookFixture, create_task_with_invalid_path)
{
    EXPECT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test/non_existent").isErr());
}
UTEST_F(FlowHookFixture, delete_task)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    auto r = utest_fixture->fh->delete_task("/tmp/fh_test");
    EXPECT_TRUE(r.isOk());
    if(r.isErr()) cout << r.getErrMessage() << endl;
}
UTEST_F(FlowHookFixture, delete_non_existent_task)
{
    EXPECT_TRUE(utest_fixture->fh->delete_task("non_existent_task").isErr());
}
UTEST_F(FlowHookFixture, delete_task_twice)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task("/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task("/tmp/fh_test").isErr());
}

// ----------------------------------------------------------------------------------
// SET / DELETE TASK PATH
// ----------------------------------------------------------------------------------

UTEST_F(FlowHookFixture, set_task_path)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    auto r = utest_fixture->fh->set_task_path("/tmp/fh_test", "/tmp/fh_test/task_test");
    EXPECT_TRUE(r.isOk());
}
UTEST_F(FlowHookFixture, set_task_path_non_existent_task)
{
    EXPECT_TRUE(utest_fixture->fh->set_task_path("non_existent_task", "/tmp/fh_test/task_test").isErr());
}
UTEST_F(FlowHookFixture, set_task_path_invalid_path)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->set_task_path("/tmp/fh_test", "/invalid/path").isErr());
}
UTEST_F(FlowHookFixture, set_empty_task_path)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->set_task_path("/tmp/fh_test", "").isErr());
}
UTEST_F(FlowHookFixture, delete_task_path)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->set_task_path("/tmp/fh_test", "/tmp/fh_test/task_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task_path("/tmp/fh_test", "/tmp/fh_test/task_test").isOk());
}
UTEST_F(FlowHookFixture, delete_non_existent_task_path)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task_path("/non_existent_task", "/tmp/fh_test/task_test").isErr());
}
UTEST_F(FlowHookFixture, delete_task_path_with_wrong_path)
{
    EXPECT_TRUE(utest_fixture->fh->delete_task_path("/tmp/fh_test", "/wrong/path").isErr());
}
UTEST_F(FlowHookFixture, delete_empty_task_path)
{
    EXPECT_TRUE(utest_fixture->fh->delete_task_path("/tmp/fh_test", "").isErr());
}

// ----------------------------------------------------------------------------------
// SET / DELETE TASK COMMAND
// ----------------------------------------------------------------------------------

UTEST_F(FlowHookFixture, set_task_command)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->set_task_command("/tmp/fh_test", "ls").isOk());
}
UTEST_F(FlowHookFixture, set_task_command_non_existent_task)
{
    EXPECT_TRUE(utest_fixture->fh->set_task_command("non_existent_task", "ls").isErr());
}
UTEST_F(FlowHookFixture, set_task_command_empty_command)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->set_task_command("/tmp/fh_test", "").isErr());
}
UTEST_F(FlowHookFixture, set_task_command_twice)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->set_task_command("/tmp/fh_test", "ls").isOk());
    EXPECT_TRUE(utest_fixture->fh->set_task_command("/tmp/fh_test", "ls").isErr());
}
UTEST_F(FlowHookFixture, delete_task_command)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->set_task_command("/tmp/fh_test", "ls").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task_command("/tmp/fh_test", "ls").isOk());
}
UTEST_F(FlowHookFixture, delete_task_command_twice)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->set_task_command("/tmp/fh_test", "ls").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task_command("/tmp/fh_test", "ls").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task_command("/tmp/fh_test", "ls").isErr());
}
UTEST_F(FlowHookFixture, delete_nonexistent_task_command)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task_command("/tmp/fh_test", "ls").isErr());
}

// ----------------------------------------------------------------------------------
// DELETE TASK ON SUCCESS / DELETE TASK ON FAILURE
// ----------------------------------------------------------------------------------

UTEST_F(FlowHookFixture, delete_task_on_success)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->set_task_on_success("/tmp/fh_test", "ls").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task_on_success("/tmp/fh_test", "ls").isOk());
}
UTEST_F(FlowHookFixture, delete_task_on_success_non_existent_task)
{
    EXPECT_TRUE(utest_fixture->fh->delete_task_on_success("non_existent_task", "ls").isErr());
}
UTEST_F(FlowHookFixture, delete_task_on_success_non_existent_command)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task_on_success("/tmp/fh_test", "non_existent_command").isErr());
}
UTEST_F(FlowHookFixture, delete_task_on_success_twice)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->set_task_on_success("/tmp/fh_test", "ls").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task_on_success("/tmp/fh_test", "ls").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task_on_success("test_task", "ls").isErr());
}
UTEST_F(FlowHookFixture, delete_task_on_success_empty_command)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task_on_success("/tmp/fh_test", "").isErr());
}

UTEST_F(FlowHookFixture, delete_task_on_failure)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->set_task_on_failure("/tmp/fh_test", "ls").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task_on_failure("/tmp/fh_test", "ls").isOk());
}
UTEST_F(FlowHookFixture, delete_task_on_failure_non_existent_task)
{
    EXPECT_TRUE(utest_fixture->fh->delete_task_on_failure("non_existent_task", "ls").isErr());
}
UTEST_F(FlowHookFixture, delete_task_on_failure_non_existent_command)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task_on_failure("/tmp/fh_test", "non_existent_command").isErr());
}
UTEST_F(FlowHookFixture, delete_task_on_failure_twice)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->set_task_on_failure("/tmp/fh_test", "ls").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task_on_failure("/tmp/fh_test", "ls").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task_on_failure("/tmp/fh_test", "ls").isErr());
}
UTEST_F(FlowHookFixture, delete_task_on_failure_empty_command)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->delete_task_on_failure("/tmp/fh_test", "").isErr());
}

// ----------------------------------------------------------------------------------
// START / STOP TASK
// ----------------------------------------------------------------------------------

UTEST_F(FlowHookFixture, start_task)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->start_task("/tmp/fh_test").isOk());
    // For now, we assume that the `start` call on TaskRunner succeeds.
    // In a real integration test, one might want to verify the task is actually running.
    // This is a minimal test to cover the FlowHookCore API.
    utest_fixture->fh->stop_task("/tmp/fh_test").isOk(); // Clean up
}
UTEST_F(FlowHookFixture, start_task_non_existent)
{
    EXPECT_TRUE(utest_fixture->fh->start_task("non_existent_task").isErr());
}
UTEST_F(FlowHookFixture, start_task_twice)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->start_task("/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->start_task("/tmp/fh_test").isErr());
    utest_fixture->fh->stop_task("/tmp/fh_test").isOk(); // Clean up
}
UTEST_F(FlowHookFixture, stop_task)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->start_task("/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->stop_task("/tmp/fh_test").isOk());
}
UTEST_F(FlowHookFixture, stop_task_non_existent)
{
    EXPECT_TRUE(utest_fixture->fh->stop_task("non_existent_task").isErr());
}
UTEST_F(FlowHookFixture, stop_task_twice)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->start_task("/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->stop_task("/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->stop_task("/tmp/fh_test").isErr()); // TaskRunner::stop() might return error if not running
}

// ----------------------------------------------------------------------------------
// ACTIVATE / DEACTIVATE TASK
// ----------------------------------------------------------------------------------

UTEST_F(FlowHookFixture, activate_task)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    auto r = utest_fixture->fh->activate_task("/tmp/fh_test");
    EXPECT_TRUE(r.isOk());
    if(r.isErr()) cout << r.getErrMessage() << endl;
}
UTEST_F(FlowHookFixture, activate_task_non_existent)
{
    EXPECT_TRUE(utest_fixture->fh->activate_task("non_existent_task").isErr());
}
UTEST_F(FlowHookFixture, activate_task_twice)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->activate_task("/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->activate_task("/tmp/fh_test").isOk()); // idempotent
}
UTEST_F(FlowHookFixture, deactivate_task)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->activate_task("/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->deactivate_task("/tmp/fh_test").isOk());
}
UTEST_F(FlowHookFixture, deactivate_task_non_existent)
{
    EXPECT_TRUE(utest_fixture->fh->deactivate_task("non_existent_task").isErr());
}
UTEST_F(FlowHookFixture, deactivate_task_twice)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->activate_task("/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->deactivate_task("/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->deactivate_task("/tmp/fh_test").isOk()); // idempotent
}

// ----------------------------------------------------------------------------------
// START ALL / STOP ALL / START ACTIVE
// ----------------------------------------------------------------------------------

UTEST_F(FlowHookFixture, start_all_no_tasks)
{
    EXPECT_TRUE(utest_fixture->fh->start_all().isErr());
}
UTEST_F(FlowHookFixture, start_all_one_task)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_1", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->start_all().isOk());
    utest_fixture->fh->stop_all().isOk(); // Clean up
}
UTEST_F(FlowHookFixture, start_all_multiple_tasks)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_1", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_2", "/tmp/fh_test2").isOk());
    EXPECT_TRUE(utest_fixture->fh->start_all().isOk());
    utest_fixture->fh->stop_all().isOk(); // Clean up
}
UTEST_F(FlowHookFixture, start_all_skips_degraded_task)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_1", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_2", "/tmp/fh_test2").isOk());

    delete utest_fixture->fh;
    fs::remove_all("/tmp/fh_test2");

    auto f = FlowHookCore::create();
    ASSERT_TRUE(f.isOk());
    utest_fixture->fh = f.unwrap();

    EXPECT_TRUE(utest_fixture->fh->start_all().isOk());
    utest_fixture->fh->stop_all().isOk();

    fs::create_directories("/tmp/fh_test2");
}
UTEST_F(FlowHookFixture, stop_all_no_tasks)
{
    EXPECT_TRUE(utest_fixture->fh->stop_all().isErr());
}
UTEST_F(FlowHookFixture, stop_all_one_task)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_1", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->start_task("/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->stop_all().isOk());
}
UTEST_F(FlowHookFixture, stop_all_multiple_tasks)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_1", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_2", "/tmp/fh_test2").isOk());
    ASSERT_TRUE(utest_fixture->fh->start_all().isOk());
    EXPECT_TRUE(utest_fixture->fh->stop_all().isOk());
}

UTEST_F(FlowHookFixture, start_active_no_tasks)
{
    EXPECT_TRUE(utest_fixture->fh->start_active().isErr()); // idempotent
}
UTEST_F(FlowHookFixture, start_active_no_active_tasks)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_1", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->start_active().isErr());
}
UTEST_F(FlowHookFixture, start_active_one_active_task)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_1", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->activate_task("/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->start_active().isOk());
    utest_fixture->fh->stop_task("/tmp/fh_test").isOk(); // Clean up
}
UTEST_F(FlowHookFixture, start_active_multiple_active_tasks)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_1", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_2", "/tmp/fh_test2").isOk());
    ASSERT_TRUE(utest_fixture->fh->activate_task("/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->activate_task("/tmp/fh_test2").isOk());
    EXPECT_TRUE(utest_fixture->fh->start_active().isOk());
    utest_fixture->fh->stop_all().isOk(); // Clean up
}
UTEST_F(FlowHookFixture, start_active_some_active_some_inactive)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_1", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_2", "/tmp/fh_test2").isOk());
    ASSERT_TRUE(utest_fixture->fh->activate_task("/tmp/fh_test").isOk());
    // test_task_2 remains inactive
    EXPECT_TRUE(utest_fixture->fh->start_active().isOk());
    utest_fixture->fh->stop_task("/tmp/fh_test").isOk(); // Clean up
}

// ----------------------------------------------------------------------------------
// GET TASKS
// ----------------------------------------------------------------------------------

UTEST_F(FlowHookFixture, get_tasks_empty)
{
    std::vector<Task> tasks = utest_fixture->fh->get_tasks();
    EXPECT_TRUE(tasks.empty());
}

UTEST_F(FlowHookFixture, get_tasks_one_task)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_1", "/tmp/fh_test").isOk());
    std::vector<Task> tasks = utest_fixture->fh->get_tasks();
    EXPECT_EQ(tasks.size(), 1);
    EXPECT_EQ(tasks[0].name, "test_task_1");
}

UTEST_F(FlowHookFixture, get_tasks_multiple_tasks)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_1", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_2", "/tmp/fh_test2").isOk());
    std::vector<Task> tasks = utest_fixture->fh->get_tasks();
    EXPECT_EQ(tasks.size(), 2);
    // Tasks should be in the order they were created or added to task_runners
    EXPECT_EQ(tasks[0].working_dir, "/tmp/fh_test");
    EXPECT_EQ(tasks[1].working_dir, "/tmp/fh_test2");
}

UTEST_F(FlowHookFixture, get_tasks_after_deletion)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_1", "/tmp/fh_test").isOk());
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task_2", "/tmp/fh_test2").isOk());
    ASSERT_TRUE(utest_fixture->fh->delete_task("/tmp/fh_test2").isOk());
    std::vector<Task> tasks = utest_fixture->fh->get_tasks();
    EXPECT_EQ(tasks.size(), 1);
    EXPECT_EQ(tasks[0].working_dir, "/tmp/fh_test");
}


// ----------------------------------------------------------------------------------
// SET TASK ON SUCCESS / SET TASK ON FAILURE
// ----------------------------------------------------------------------------------

UTEST_F(FlowHookFixture, set_task_on_success)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->set_task_on_success("/tmp/fh_test", "ls").isOk());
}
UTEST_F(FlowHookFixture, set_task_on_failure)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->set_task_on_failure("/tmp/fh_test", "ls").isOk());
}
UTEST_F(FlowHookFixture, set_task_on_success_twice)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->set_task_on_success("/tmp/fh_test", "ls").isOk());
    EXPECT_TRUE(utest_fixture->fh->set_task_on_success("/tmp/fh_test", "ls").isErr());
}
UTEST_F(FlowHookFixture, set_task_on_failure_twice)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->set_task_on_failure("/tmp/fh_test", "ls").isOk());
    EXPECT_TRUE(utest_fixture->fh->set_task_on_failure("/tmp/fh_test", "ls").isErr());
}
UTEST_F(FlowHookFixture, set_empty_task_on_success)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->set_task_on_success("/tmp/fh_test", "").isErr());
}
UTEST_F(FlowHookFixture, set_empty_task_on_failure)
{
    ASSERT_TRUE(utest_fixture->fh->create_task("test_task", "/tmp/fh_test").isOk());
    EXPECT_TRUE(utest_fixture->fh->set_task_on_failure("/tmp/fh_test", "").isErr());
}
UTEST_F(FlowHookFixture, set_task_on_success_nonexistent_task)
{
    EXPECT_TRUE(utest_fixture->fh->set_task_on_success("nonexistent_task", "ls").isErr());
}
UTEST_F(FlowHookFixture, set_task_on_failure_nonexistent_task)
{
    EXPECT_TRUE(utest_fixture->fh->set_task_on_failure("nonexistent_task", "ls").isErr());
}

UTEST_MAIN()

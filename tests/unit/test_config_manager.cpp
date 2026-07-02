#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>


#include "../../src/include/types.h"
#include "../utest.h"
#include "../../src/include/json.hpp"
#include "../config_test.h"
#include "version.h"

namespace fs = std::filesystem;
using namespace flowhook;
using namespace std;
using namespace nlohmann;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
struct ConfigManagerFixture
{
    ConfigManager *cm;
    Task *t;
    Task *t2;
    Task *t3;
};

UTEST_F_SETUP(ConfigManagerFixture)
{
    setenv("FLOWHOOK_CONFIG_DIR_TEST", "/tmp/cm_test", 1);
    auto r = ConfigManager::create();
    ASSERT_TRUE(r.isOk());
    utest_fixture->cm = r.unwrap();

    fs::create_directories("/tmp/cm_test");
    fs::create_directories("/tmp/cm_test2");

    utest_fixture->t = new Task("/tmp/cm_test", "test_task", 3, {"ls"},
        {"/tmp/cm_test_file.txt"}, {"/tmp/cm_test"}, vector<string>{"ls"}, vector<string>{"ls"}, {"*.o"}, {".git"}, true, true);

    utest_fixture->t2 = new Task("test_task2", "/tmp/cm_test2", 3, {"ls"},
        {"/tmp/cm_test_file.txt"}, {"/tmp/cm_test"}, vector<string>{"ls"}, vector<string>{"cd, ls"}, {"*.o"}, {".git"}, true, true);

    utest_fixture->t3 = new Task("/tmp/cm_test", "test_task", 3, {"cd", "make"},
        {"/tmp/cm_test_file.txt"}, {"/tmp/cm_test"}, vector<string>{"ls"}, vector<string>{"cd, ls"}, {"*.o"}, {".git"}, true, true);
}

UTEST_F_TEARDOWN(ConfigManagerFixture)
{
    delete utest_fixture->cm;
    delete utest_fixture->t;
    delete utest_fixture->t2;
    delete utest_fixture->t3;
    fs::remove_all("/tmp/cm_test");
    fs::remove_all("/tmp/cm_test2");
    unsetenv("FLOWHOOK_CONFIG_DIR_TEST");
}

// --------------------------------------------------------------------------------------------
// Create
// --------------------------------------------------------------------------------------------
UTEST_F(ConfigManagerFixture, create)
{
    ASSERT_NE(utest_fixture->cm, nullptr);
    EXPECT_TRUE(fs::exists("/tmp/cm_test/config.json"));
    EXPECT_TRUE(ConfigManagerTest::get_config(utest_fixture->cm)["version"] == FLOWHOOK_VERSION);
}

// --------------------------------------------------------------------------------------------
// log task
// --------------------------------------------------------------------------------------------

UTEST_F(ConfigManagerFixture, log_task)
{
    auto c = utest_fixture->cm->log_task(*utest_fixture->t);
    EXPECT_TRUE(c.isOk());
}

UTEST_F(ConfigManagerFixture, log_multiple_tasks)
{
    auto c = utest_fixture->cm->log_task(*utest_fixture->t);
    auto c2 = utest_fixture->cm->log_task(*utest_fixture->t2);
    EXPECT_TRUE(c.isOk());
    EXPECT_TRUE(c2.isOk());
}

UTEST_F(ConfigManagerFixture, log_empty_task)
{
    auto c = utest_fixture->cm->log_task(Task());
    EXPECT_TRUE(c.isErr());
}

UTEST_F(ConfigManagerFixture, log_task_twice)
{
    EXPECT_TRUE(utest_fixture->cm->log_task(*utest_fixture->t).isOk());
    EXPECT_TRUE(utest_fixture->cm->log_task(*utest_fixture->t).isErr());
}

UTEST_F(ConfigManagerFixture, log_task_inbatch)
{
    vector<Task> tasks;
    tasks.push_back(*utest_fixture->t);
    tasks.push_back(*utest_fixture->t2);
    auto c = utest_fixture->cm->log_task_inbatch(tasks);
    EXPECT_TRUE(c.isOk());
}


// --------------------------------------------------------------------------------------------
// delete task
// --------------------------------------------------------------------------------------------

UTEST_F(ConfigManagerFixture, delete_task)
{
    auto c = utest_fixture->cm->log_task(*utest_fixture->t);
    ASSERT_TRUE(c.isOk());

    auto c2 = utest_fixture->cm->delete_task(*utest_fixture->t);
    EXPECT_TRUE(c2.isOk());
}


// --------------------------------------------------------------------------------------------
// update task
// --------------------------------------------------------------------------------------------

UTEST_F(ConfigManagerFixture, update_task)
{
    auto c = utest_fixture->cm->log_task(*utest_fixture->t);
    ASSERT_TRUE(c.isOk());

    auto c2 = utest_fixture->cm->update_task(*utest_fixture->t3);
    EXPECT_TRUE(c2.isOk());
    if(c2.isErr())
    {
        cout << c2.getErrMessage() << endl;
    }
}


// --------------------------------------------------------------------------------------------
// flush
// --------------------------------------------------------------------------------------------


UTEST_F(ConfigManagerFixture, flush)
{
    vector<Task> tasks;
    tasks.push_back(*utest_fixture->t);
    tasks.push_back(*utest_fixture->t2);
    ASSERT_TRUE(utest_fixture->cm->log_task_inbatch(tasks).isOk());
    auto f = utest_fixture->cm->flush();
    EXPECT_TRUE(f.isOk());
    if(f.isErr())
    {
        cout << f.unwrapErr().message << "--------------------------------------------------------------------------------" << endl;
    }

    json test_json = utest_fixture->cm->getjson();
    ifstream file("/tmp/cm_test/config.json");
    json content = json::parse(file);
    EXPECT_EQ(test_json.size(), content.size());
}


// --------------------------------------------------------------------------------------------
// purge config
// --------------------------------------------------------------------------------------------

UTEST_F(ConfigManagerFixture, purge_config)
{
    auto c = utest_fixture->cm->log_task(*utest_fixture->t);
    ASSERT_TRUE(c.isOk());
    auto c2 = utest_fixture->cm->purge_config();
    EXPECT_TRUE(c2.isOk());

    auto config_path = "/tmp/cm_test/config.json";
    EXPECT_TRUE(fs::file_size(config_path) == 0);
}

// --------------------------------------------------------------------------------------------
// get tasks
// --------------------------------------------------------------------------------------------

UTEST_F(ConfigManagerFixture, get_tasks)
{
    vector<Task> tasks;
    tasks.push_back(*utest_fixture->t);
    tasks.push_back(*utest_fixture->t2);
    ASSERT_TRUE(utest_fixture->cm->log_task_inbatch(tasks).isOk());

    auto r = utest_fixture->cm->get_tasks();
    EXPECT_TRUE(r.isOk());
    vector<Task> tasks2 = r.unwrap();
    EXPECT_EQ(tasks.size(), tasks2.size());
}


UTEST_MAIN()

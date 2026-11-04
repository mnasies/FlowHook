#include <filesystem>
#include <fstream>
#include <regex>

#include "../../src/include/types.h"
#include "../../src/include/identity.h"
#include "../utest.h"

using namespace flowhook;
using namespace std;
namespace fs = std::filesystem;

static const char* ID_DIR = "/tmp/identity_test";

struct IdentityFixture {};

UTEST_F_SETUP(IdentityFixture)
{
    fs::remove_all(ID_DIR);
    fs::create_directories(ID_DIR);
}

UTEST_F_TEARDOWN(IdentityFixture)
{
    fs::remove_all(ID_DIR);
}

// --------------------------------------------------------------------------------------------
// generate_uuid
// --------------------------------------------------------------------------------------------

UTEST_F(IdentityFixture, uuid_format)
{
    string u = generate_uuid();
    // must look like a v4 uuid: 8-4-4-4-12 with the version/variant nibbles set
    const std::regex uuid_re(
        "^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$");
    EXPECT_STRNE(u.c_str(), "");
    EXPECT_TRUE(std::regex_match(u, uuid_re));
}

UTEST_F(IdentityFixture, uuid_unique)
{
    string a = generate_uuid();
    string b = generate_uuid();
    EXPECT_STRNE(a.c_str(), b.c_str());
}

// --------------------------------------------------------------------------------------------
// marker write / read / delete
// --------------------------------------------------------------------------------------------

UTEST_F(IdentityFixture, marker_round_trip)
{
    Task t;
    t.uuid = "marker-uuid-1";
    t.name = "test_task";

    EXPECT_TRUE(write_marker(ID_DIR, t).isOk());

    auto r = read_marker(ID_DIR);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.unwrap().uuid, "marker-uuid-1");
    EXPECT_EQ(r.unwrap().task_name, "test_task");
}

UTEST_F(IdentityFixture, marker_read_missing)
{
    auto r = read_marker(ID_DIR);
    EXPECT_TRUE(r.isErr());
    EXPECT_EQ(r.getErrCode(), ErrorCode::PATH_NOT_FOUND);
}

UTEST_F(IdentityFixture, marker_read_corrupted)
{
    Task t;
    t.uuid = "marker-uuid-1";
    t.name = "test_task";
    ASSERT_TRUE(write_marker(ID_DIR, t).isOk());
    // corrupt the marker file
    ofstream(fs::path(ID_DIR) / ".flowhook") << "{ not json";

    auto r = read_marker(ID_DIR);
    EXPECT_TRUE(r.isErr());
    EXPECT_EQ(r.getErrCode(), ErrorCode::CONFIG_PARSE_FAILED);
}

UTEST_F(IdentityFixture, marker_write_overwrites)
{
    Task a; a.uuid = "uuid-first"; a.name = "first";
    Task b; b.uuid = "uuid-second"; b.name = "second";
    ASSERT_TRUE(write_marker(ID_DIR, a).isOk());
    ASSERT_TRUE(write_marker(ID_DIR, b).isOk());
    auto r = read_marker(ID_DIR);
    ASSERT_TRUE(r.isOk());
    EXPECT_EQ(r.unwrap().uuid, "uuid-second");
}

UTEST_F(IdentityFixture, marker_write_missing_dir)
{
    Task t;
    t.uuid = "u";
    auto r = write_marker("/tmp/identity_test/nonexistent", t);
    EXPECT_TRUE(r.isErr());
    EXPECT_EQ(r.getErrCode(), ErrorCode::PATH_NOT_FOUND);
}

UTEST_F(IdentityFixture, marker_write_empty_uuid)
{
    // a marker without a uuid would be an unusable identity carrier
    Task t;
    t.uuid = "";
    t.name = "task";
    EXPECT_TRUE(write_marker(ID_DIR, t).isOk());

    auto r = read_marker(ID_DIR);
    EXPECT_TRUE(r.isErr());
    EXPECT_EQ(r.getErrCode(), ErrorCode::CONFIG_PARSE_FAILED);
}

UTEST_F(IdentityFixture, marker_delete)
{
    Task t;
    t.uuid = "marker-uuid-1";
    t.name = "test_task";
    ASSERT_TRUE(write_marker(ID_DIR, t).isOk());
    EXPECT_TRUE(fs::exists(fs::path(ID_DIR) / ".flowhook"));

    EXPECT_TRUE(delete_marker(ID_DIR).isOk());
    EXPECT_FALSE(fs::exists(fs::path(ID_DIR) / ".flowhook"));
    // deleting again must stay Ok (idempotent)
    EXPECT_TRUE(delete_marker(ID_DIR).isOk());
}

UTEST_F(IdentityFixture, marker_path)
{
    EXPECT_EQ(marker_path("/tmp/abc"), "/tmp/abc/.flowhook");
}

UTEST_MAIN()

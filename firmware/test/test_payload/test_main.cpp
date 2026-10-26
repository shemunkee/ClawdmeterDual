// payload_parse: long-standing fields keep their defaults, fast-clock fields
// are optional and clamped.
//   pio test -d firmware -e native -f test_payload
#include <unity.h>
#include <string.h>
#include "payload.h"

void setUp(void) {}
void tearDown(void) {}

static void test_rejects_malformed_json(void) {
    UsageData d = {};
    d.session_pct = 42;
    const char* err = nullptr;
    TEST_ASSERT_FALSE(payload_parse("{\"s\":", &d, &err));
    TEST_ASSERT_NOT_NULL(err);
    TEST_ASSERT_EQUAL_FLOAT(42, d.session_pct);   // untouched on failure
}

static void test_empty_object_gives_legacy_defaults(void) {
    UsageData d;
    TEST_ASSERT_TRUE(payload_parse("{}", &d, nullptr));
    TEST_ASSERT_EQUAL_FLOAT(0, d.session_pct);
    TEST_ASSERT_EQUAL_INT(-1, d.session_reset_mins);
    TEST_ASSERT_EQUAL_INT(-1, d.weekly_reset_mins);
    TEST_ASSERT_EQUAL_STRING("unknown", d.status);
    TEST_ASSERT_FALSE(d.chime);
    TEST_ASSERT_FALSE(d.beep);
    TEST_ASSERT_EQUAL_STRING("", d.alert_msg);
    TEST_ASSERT_FALSE(d.enterprise);
    TEST_ASSERT_EQUAL_INT(30, d.period_days);
    TEST_ASSERT_EQUAL_INT(24, d.clock_fmt);
    TEST_ASSERT_FALSE(d.ok);
    TEST_ASSERT_TRUE(d.valid);
    // fast clock absent
    TEST_ASSERT_EQUAL_INT(-1, d.activity);
    TEST_ASSERT_EQUAL_CHAR('\0', d.event);
    TEST_ASSERT_FALSE(d.has_event_seq);
    TEST_ASSERT_EQUAL_INT(-1, d.sessions);
    TEST_ASSERT_FALSE(d.replay);
}

static void test_legacy_fields(void) {
    UsageData d;
    TEST_ASSERT_TRUE(payload_parse(
        "{\"s\":12.5,\"sr\":240,\"w\":38,\"wr\":1500,\"st\":\"allowed\",\"c\":1,\"b\":1,"
        "\"m\":\"Claude needs your permission\",\"x\":1,\"acct\":\"ent\",\"tp\":40,"
        "\"pd\":31,\"rd\":\"Jul 1\",\"t\":1760000000,\"tf\":12,\"ok\":true}", &d, nullptr));
    TEST_ASSERT_EQUAL_FLOAT(12.5f, d.session_pct);
    TEST_ASSERT_EQUAL_INT(240, d.session_reset_mins);
    TEST_ASSERT_EQUAL_FLOAT(38, d.weekly_pct);
    TEST_ASSERT_EQUAL_INT(1500, d.weekly_reset_mins);
    TEST_ASSERT_EQUAL_STRING("allowed", d.status);
    TEST_ASSERT_TRUE(d.chime);
    TEST_ASSERT_TRUE(d.beep);
    TEST_ASSERT_EQUAL_STRING("Claude needs your permission", d.alert_msg);
    TEST_ASSERT_TRUE(d.alert_clear);
    TEST_ASSERT_TRUE(d.enterprise);
    TEST_ASSERT_EQUAL_INT(40, d.time_pct);
    TEST_ASSERT_EQUAL_INT(31, d.period_days);
    TEST_ASSERT_EQUAL_STRING("Jul 1", d.reset_date);
    TEST_ASSERT_EQUAL_INT64(1760000000L, (int64_t)d.clock_epoch);
    TEST_ASSERT_EQUAL_INT(12, d.clock_fmt);
    TEST_ASSERT_TRUE(d.ok);
}

static void test_fast_clock_fields(void) {
    UsageData d;
    TEST_ASSERT_TRUE(payload_parse("{\"a\":2,\"e\":\"b\",\"es\":4000000000,\"n\":3,\"rp\":1}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(2, d.activity);
    TEST_ASSERT_EQUAL_CHAR('b', d.event);
    TEST_ASSERT_TRUE(d.has_event_seq);
    TEST_ASSERT_EQUAL_UINT32(4000000000UL, d.event_seq);
    TEST_ASSERT_EQUAL_INT(3, d.sessions);
    TEST_ASSERT_TRUE(d.replay);
}

static void test_fast_clock_clamps(void) {
    UsageData d;
    TEST_ASSERT_TRUE(payload_parse("{\"a\":9,\"n\":-4,\"es\":0}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(3, d.activity);
    TEST_ASSERT_EQUAL_INT(0, d.sessions);
    TEST_ASSERT_TRUE(d.has_event_seq);             // 0 is a real seq, not absent
    TEST_ASSERT_TRUE(payload_parse("{\"a\":-1}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(0, d.activity);
}

static void test_attention_animation(void) {
    UsageData d;
    TEST_ASSERT_TRUE(payload_parse("{\"s\":1}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(-1, d.attn_anim_s);      // absent: keep the saved setting
    TEST_ASSERT_TRUE(payload_parse("{\"aa\":0}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(0, d.attn_anim_s);       // off
    TEST_ASSERT_TRUE(payload_parse("{\"aa\":30}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(30, d.attn_anim_s);
    TEST_ASSERT_TRUE(payload_parse("{\"aa\":999}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(120, d.attn_anim_s);
}

static void test_liveliness(void) {
    UsageData d;
    TEST_ASSERT_TRUE(payload_parse("{\"ok\":true}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(-1, d.liveliness);       // absent: keep the saved level
    TEST_ASSERT_TRUE(payload_parse("{\"lv\":1}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(1, d.liveliness);
    TEST_ASSERT_TRUE(payload_parse("{\"lv\":9}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(5, d.liveliness);
    TEST_ASSERT_TRUE(payload_parse("{\"lv\":0}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(1, d.liveliness);
}

static void test_pace_marker(void) {
    UsageData d;
    TEST_ASSERT_TRUE(payload_parse("{\"ok\":true}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(-1, d.pace_marker);      // absent: keep the saved setting
    TEST_ASSERT_TRUE(payload_parse("{\"pm\":1}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(1, d.pace_marker);
    TEST_ASSERT_TRUE(payload_parse("{\"pm\":0}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(0, d.pace_marker);
    TEST_ASSERT_TRUE(payload_parse("{\"pm\":7}", &d, nullptr));
    TEST_ASSERT_EQUAL_INT(1, d.pace_marker);
}

static void test_single_account_is_not_split(void) {
    UsageData d;
    TEST_ASSERT_TRUE(payload_parse("{\"s\":12,\"sr\":30,\"ok\":true}", &d, nullptr));
    TEST_ASSERT_FALSE(d.split);
    TEST_ASSERT_EQUAL_STRING("", d.label);
    TEST_ASSERT_EQUAL_STRING("", d.label2);
}

static void test_second_account(void) {
    UsageData d;
    TEST_ASSERT_TRUE(payload_parse(
        "{\"s\":12,\"sr\":30,\"w\":5,\"wr\":900,\"l\":\"Personal\",\"s2\":64.4,\"sr2\":120,"
        "\"w2\":71,\"wr2\":4000,\"ok2\":true,\"l2\":\"Work\",\"ok\":true}", &d, nullptr));
    TEST_ASSERT_TRUE(d.split);
    TEST_ASSERT_TRUE(d.ok2);
    TEST_ASSERT_EQUAL_FLOAT(64.4f, d.session2_pct);
    TEST_ASSERT_EQUAL_INT(120, d.session2_reset_mins);
    TEST_ASSERT_EQUAL_FLOAT(71, d.weekly2_pct);
    TEST_ASSERT_EQUAL_INT(4000, d.weekly2_reset_mins);
    TEST_ASSERT_FALSE(d.enterprise2);
    TEST_ASSERT_EQUAL_STRING("Personal", d.label);
    TEST_ASSERT_EQUAL_STRING("Work", d.label2);
}

static void test_second_account_without_data(void) {
    UsageData d;
    // ok2:false alone still means two accounts: the bottom half shows "no data".
    TEST_ASSERT_TRUE(payload_parse("{\"s\":12,\"ok2\":false,\"ok\":true}", &d, nullptr));
    TEST_ASSERT_TRUE(d.split);
    TEST_ASSERT_FALSE(d.ok2);
    TEST_ASSERT_EQUAL_INT(-1, d.session2_reset_mins);
    TEST_ASSERT_EQUAL_INT(-1, d.weekly2_reset_mins);
    TEST_ASSERT_TRUE(payload_parse("{\"s2\":3,\"acct2\":\"ent\"}", &d, nullptr));
    TEST_ASSERT_TRUE(d.ok2);                       // s2 without ok2: a reading
    TEST_ASSERT_TRUE(d.enterprise2);
}

static void test_long_strings_truncate(void) {
    UsageData d;
    TEST_ASSERT_TRUE(payload_parse("{\"st\":\"abcdefghijklmnopqrstuvwxyz\"}", &d, nullptr));
    TEST_ASSERT_EQUAL_size_t(sizeof(d.status) - 1, strlen(d.status));
    TEST_ASSERT_TRUE(payload_parse("{\"l2\":\"abcdefghijklmnopqrstuvwxyz\"}", &d, nullptr));
    TEST_ASSERT_EQUAL_size_t(sizeof(d.label2) - 1, strlen(d.label2));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_rejects_malformed_json);
    RUN_TEST(test_empty_object_gives_legacy_defaults);
    RUN_TEST(test_legacy_fields);
    RUN_TEST(test_fast_clock_fields);
    RUN_TEST(test_fast_clock_clamps);
    RUN_TEST(test_liveliness);
    RUN_TEST(test_attention_animation);
    RUN_TEST(test_pace_marker);
    RUN_TEST(test_single_account_is_not_split);
    RUN_TEST(test_second_account);
    RUN_TEST(test_second_account_without_data);
    RUN_TEST(test_long_strings_truncate);
    return UNITY_END();
}

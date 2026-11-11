#include <gtest/gtest.h>
#include "fake_power_backend.hpp"
#include <powrprof.h>
#include <algorithm>

using namespace vigilantune;

static GUID fixture_guid(unsigned int variant) {
    GUID result = GUID_TYPICAL_POWER_SAVINGS;
    result.Data1 ^= variant;
    return result;
}

TEST(FakePowerBackend, DuplicateHasIndependentSettingsAndName) {
    const auto original = fixture_guid(0);
    const auto duplicate_id = fixture_guid(1);
    const auto subgroup = fixture_guid(2);
    const auto setting = fixture_guid(3);
    FakePowerBackend backend(original, L"Original");
    ASSERT_EQ(backend.seed_setting(original, subgroup, setting, 40, 60), ERROR_SUCCESS);

    GUID duplicate{};
    ASSERT_EQ(backend.duplicate_scheme(original, &duplicate_id, duplicate), ERROR_SUCCESS);
    EXPECT_TRUE(IsEqualGUID(duplicate, duplicate_id));
    ASSERT_EQ(backend.set_friendly_name(duplicate, L"Copy"), ERROR_SUCCESS);
    ASSERT_EQ(backend.write_value(duplicate, subgroup, setting, PowerSource::AC, 10), ERROR_SUCCESS);

    DWORD value = 0;
    ASSERT_EQ(backend.read_value(original, subgroup, setting, PowerSource::AC, value), ERROR_SUCCESS);
    EXPECT_EQ(value, 40u);
    ASSERT_EQ(backend.read_value(duplicate, subgroup, setting, PowerSource::AC, value), ERROR_SUCCESS);
    EXPECT_EQ(value, 10u);
    ASSERT_EQ(backend.read_value(duplicate, subgroup, setting, PowerSource::DC, value), ERROR_SUCCESS);
    EXPECT_EQ(value, 60u);

    std::wstring name;
    ASSERT_EQ(backend.friendly_name(original, name), ERROR_SUCCESS);
    EXPECT_EQ(name, L"Original");
    ASSERT_EQ(backend.friendly_name(duplicate, name), ERROR_SUCCESS);
    EXPECT_EQ(name, L"Copy");
}

TEST(FakePowerBackend, TracksActiveAndRejectsMissingOrDuplicateSchemes) {
    const auto original = fixture_guid(0);
    const auto other = fixture_guid(1);
    const auto missing = fixture_guid(2);
    FakePowerBackend backend(original, L"Original");

    GUID active{};
    ASSERT_EQ(backend.active_scheme(active), ERROR_SUCCESS);
    EXPECT_TRUE(IsEqualGUID(active, original));
    EXPECT_NE(backend.set_active_scheme(missing), ERROR_SUCCESS);

    GUID duplicate{};
    ASSERT_EQ(backend.duplicate_scheme(original, &other, duplicate), ERROR_SUCCESS);
    EXPECT_EQ(backend.duplicate_scheme(original, &other, duplicate), ERROR_ALREADY_EXISTS);
    ASSERT_EQ(backend.set_active_scheme(other), ERROR_SUCCESS);
    ASSERT_EQ(backend.active_scheme(active), ERROR_SUCCESS);
    EXPECT_TRUE(IsEqualGUID(active, other));
    ASSERT_EQ(backend.set_active_scheme(original), ERROR_SUCCESS);
    ASSERT_EQ(backend.delete_scheme(other), ERROR_SUCCESS);
    EXPECT_NE(backend.set_active_scheme(other), ERROR_SUCCESS);
}

TEST(FakePowerBackend, EnumeratesSchemesAndGeneratesDistinctIds) {
    const auto original = fixture_guid(0);
    FakePowerBackend backend(original, L"Original");
    GUID duplicate{};
    ASSERT_EQ(backend.duplicate_scheme(original, nullptr, duplicate), ERROR_SUCCESS);
    EXPECT_FALSE(IsEqualGUID(original, duplicate));

    std::vector<GUID> schemes;
    ASSERT_EQ(backend.enumerate_schemes(schemes), ERROR_SUCCESS);
    EXPECT_EQ(schemes.size(), 2u);
    EXPECT_TRUE(std::any_of(schemes.begin(), schemes.end(), [&](const GUID &id) { return IsEqualGUID(id, original); }));
    EXPECT_TRUE(
        std::any_of(schemes.begin(), schemes.end(), [&](const GUID &id) { return IsEqualGUID(id, duplicate); }));
}

TEST(FakePowerBackend, ReadBackDoesNotProveSettingExists) {
    const auto original = fixture_guid(0);
    const auto subgroup = fixture_guid(1);
    const auto known = fixture_guid(2);
    const auto unknown = fixture_guid(3);
    FakePowerBackend backend(original, L"Original");
    ASSERT_EQ(backend.seed_setting(original, subgroup, known, 40, 60), ERROR_SUCCESS);

    ASSERT_EQ(backend.write_value(original, subgroup, unknown, PowerSource::AC, 17), ERROR_SUCCESS);
    DWORD value = 0;
    ASSERT_EQ(backend.read_value(original, subgroup, unknown, PowerSource::AC, value), ERROR_SUCCESS);
    EXPECT_EQ(value, 17u);

    std::vector<GUID> settings;
    ASSERT_EQ(backend.enumerate_settings(original, subgroup, settings), ERROR_SUCCESS);
    EXPECT_EQ(settings.size(), 1u);
    EXPECT_TRUE(std::any_of(settings.begin(), settings.end(), [&](const GUID &id) { return IsEqualGUID(id, known); }));
    EXPECT_FALSE(
        std::any_of(settings.begin(), settings.end(), [&](const GUID &id) { return IsEqualGUID(id, unknown); }));
}

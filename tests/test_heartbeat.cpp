#include <gtest/gtest.h>
#include "../src/shared/heartbeat.hpp"

TEST(HeartbeatIdentity, PublishesTheVigilanTuneMappingForTheWatchdog) {
    EXPECT_STREQ(vigilantune::HEARTBEAT_MAPPING_NAME, L"Local\\VigilanTune_Heartbeat");
    const auto name = L"Local\\VigilanTune_Heartbeat_Test_" + std::to_wstring(GetCurrentProcessId());

    vigilantune::HeartbeatWriter writer(name.c_str());
    ASSERT_TRUE(writer.valid());

    HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, name.c_str());
    ASSERT_NE(mapping, nullptr);
    CloseHandle(mapping);

    vigilantune::HeartbeatReader reader(name.c_str());
    ASSERT_TRUE(reader.valid());
    writer.tick();
    EXPECT_EQ(reader.get_tick_count(), 1u);
}

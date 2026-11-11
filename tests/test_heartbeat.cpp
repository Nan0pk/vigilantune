#include <gtest/gtest.h>
#include "../src/shared/heartbeat.hpp"

TEST(HeartbeatIdentity, PublishesTheVigilanTuneMappingForTheWatchdog) {
    nanoloop::HeartbeatWriter writer;
    ASSERT_TRUE(writer.valid());

    HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, L"Local\\VigilanTune_Heartbeat");
    ASSERT_NE(mapping, nullptr);
    CloseHandle(mapping);

    nanoloop::HeartbeatReader reader;
    ASSERT_TRUE(reader.valid());
    writer.tick();
    EXPECT_EQ(reader.get_tick_count(), 1u);
}

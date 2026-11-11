#include <gtest/gtest.h>
#include "../src/shared/config.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

class ConfigurationCompatibility : public ::testing::Test {
  protected:
    std::filesystem::path directory;
    bool created = false;
    std::vector<std::string> previous_exclusions;

    void SetUp() override {
        previous_exclusions = vigilantune::config::EXCLUSION_LIST;
        directory =
            std::filesystem::current_path() / ("vigilantune-config-test-" + std::to_string(GetCurrentProcessId()));
        created = std::filesystem::create_directory(directory);
        ASSERT_TRUE(created);
    }

    void TearDown() override {
        vigilantune::config::EXCLUSION_LIST = previous_exclusions;
        if (created) {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
            EXPECT_FALSE(error);
        }
    }
};

TEST_F(ConfigurationCompatibility, LoadsLegacyFileWhenNewFileIsAbsent) {
    {
        std::ofstream file(directory / "nanoloop_config.ini");
        ASSERT_TRUE(file.is_open());
        file << "[general]\nlog_level = DEBUG\n";
    }

    vigilantune::ConfigLoader loader;
    ASSERT_TRUE(loader.load((directory / "vigilantune.ini").string()));
    EXPECT_EQ(loader.get_string("general.log_level"), "DEBUG");
}

TEST_F(ConfigurationCompatibility, PrefersNewFileWhenBothExist) {
    {
        std::ofstream legacy(directory / "nanoloop_config.ini");
        std::ofstream current(directory / "vigilantune.ini");
        ASSERT_TRUE(legacy.is_open());
        ASSERT_TRUE(current.is_open());
        legacy << "[general]\nlog_level = DEBUG\n";
        current << "[general]\nlog_level = WARN\n";
    }

    vigilantune::ConfigLoader loader;
    ASSERT_TRUE(loader.load((directory / "vigilantune.ini").string()));
    EXPECT_EQ(loader.get_string("general.log_level"), "WARN");
}

TEST_F(ConfigurationCompatibility, DoesNotUseLegacyFileForAnUnrelatedPath) {
    {
        std::ofstream legacy(directory / "nanoloop_config.ini");
        ASSERT_TRUE(legacy.is_open());
        legacy << "[general]\nlog_level = DEBUG\n";
    }

    vigilantune::ConfigLoader loader;
    EXPECT_FALSE(loader.load((directory / "other.ini").string()));
}

TEST_F(ConfigurationCompatibility, DefaultExclusionsIncludeRenamedProcesses) {
    auto exclusions = vigilantune::config::EXCLUSION_LIST;
    for (auto &name : exclusions) {
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    }
    EXPECT_NE(std::find(exclusions.begin(), exclusions.end(), "vigilantune.agent.exe"), exclusions.end());
    EXPECT_NE(std::find(exclusions.begin(), exclusions.end(), "vigilantune.watchdog.exe"), exclusions.end());
}

TEST_F(ConfigurationCompatibility, ExcludesRenamedProcessesWithLegacyExclusions) {
    const auto path = directory / "vigilantune.ini";
    {
        std::ofstream file(path);
        ASSERT_TRUE(file.is_open());
        file << "[governor]\nexclusion_list = explorer.exe,agent.exe,watchdog.exe\n";
    }

    ASSERT_TRUE(vigilantune::config::load_from_file(path.string()));
    auto exclusions = vigilantune::config::EXCLUSION_LIST;
    for (auto &name : exclusions) {
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    }
    EXPECT_NE(std::find(exclusions.begin(), exclusions.end(), "vigilantune.agent.exe"), exclusions.end());
    EXPECT_NE(std::find(exclusions.begin(), exclusions.end(), "vigilantune.watchdog.exe"), exclusions.end());
}

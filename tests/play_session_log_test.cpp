/*!
 *\file play_session_log_test.cpp
 *\brief Tests for JSONL play session logging.
 */

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

#include "simulation/economy.h"
#include "simulation/play_session_log.h"
#include "simulation/world.h"

namespace
{
   std::string ReadEntireFile(const std::filesystem::path& filePath)
   {
      std::ifstream stream(filePath);
      std::string contents;
      std::string line;
      while (std::getline(stream, line))
      {
         if (!contents.empty())
         {
            contents.push_back('\n');
         }
         contents += line;
      }
      return contents;
   }

   bool ContainsSubstring(std::string_view haystack, std::string_view needle)
   {
      return haystack.find(needle) != std::string_view::npos;
   }
} // namespace

TEST(PlaySessionLogTest, PlaySessionLogWritesSnapshots)
{
   const std::filesystem::path logsDirectory =
      std::filesystem::temp_directory_path() / "minidb_play_log_test";
   std::error_code errorCode;
   std::filesystem::remove_all(logsDirectory, errorCode);

   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   world.ConfigureEconomy(MiniDb::DefaultTrainCapacity, MiniDb::GameMode::Economic, MiniDb::NeverLose::No);

   MiniDb::PlaySessionLog playLog;
   MiniDb::PlaySessionSettings settings;
   settings.trainCapacity = MiniDb::DefaultTrainCapacity;
   ASSERT_TRUE(MiniDb::IsOk(playLog.BeginSession(logsDirectory.string(), settings, world.GetEconomy())));

   playLog.Tick(MiniDb::PlaySessionSnapshotRealSeconds + 1.0f, world, world.GetEconomy());
   playLog.EndSession("test", world, world.GetEconomy());

   bool foundFile = false;
   for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(logsDirectory))
   {
      if (!entry.is_regular_file())
      {
         continue;
      }

      const std::string fileContents = ReadEntireFile(entry.path());
      foundFile = true;
      EXPECT_TRUE(ContainsSubstring(fileContents, "\"type\":\"session_start\""));
      EXPECT_TRUE(ContainsSubstring(fileContents, "\"type\":\"snapshot\""));
      EXPECT_TRUE(ContainsSubstring(fileContents, "\"type\":\"session_end\""));
   }

   EXPECT_TRUE(foundFile);
   std::filesystem::remove_all(logsDirectory, errorCode);
}

TEST(PlaySessionLogTest, PlaySessionLogSkipsSandbox)
{
   MiniDb::PlaySessionLog playLog;
   EXPECT_FALSE(playLog.IsActive());
}

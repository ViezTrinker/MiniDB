/*!
 *\file play_session_log.cpp
 *\brief JSONL play session logging for economy tuning.
 */

#include "simulation/play_session_log.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>

#include "core/constants.h"
#include "simulation/track_inventory.h"

namespace MiniDb
{
   namespace
   {
      std::string EscapeJsonString(std::string_view text)
      {
         std::string escaped;
         escaped.reserve(text.size());
         for (char character : text)
         {
            if (character == '"' || character == '\\')
            {
               escaped.push_back('\\');
            }
            escaped.push_back(character);
         }
         return escaped;
      }

      std::string CurrentTimestampIso(void)
      {
         const auto now = std::chrono::system_clock::now();
         const std::time_t timeValue = std::chrono::system_clock::to_time_t(now);
         std::tm localTime;
#if defined(_WIN32)
         localtime_s(&localTime, &timeValue);
#else
         localtime_r(&timeValue, &localTime);
#endif
         std::ostringstream stream;
         stream << std::put_time(&localTime, "%Y-%m-%dT%H:%M:%S");
         return stream.str();
      }

      std::string BuildSessionFileName(void)
      {
         const auto now = std::chrono::system_clock::now();
         const std::time_t timeValue = std::chrono::system_clock::to_time_t(now);
         std::tm localTime;
#if defined(_WIN32)
         localtime_s(&localTime, &timeValue);
#else
         localtime_r(&timeValue, &localTime);
#endif
         std::ostringstream stream;
         stream << "play_"
            << std::put_time(&localTime, "%Y%m%d_%H%M%S")
            << ".jsonl";
         return stream.str();
      }
   } // namespace

   PlaySessionLog::PlaySessionLog(void)
   {
   }

   Result PlaySessionLog::BeginSession(
      std::string_view logsDirectory,
      const PlaySessionSettings& settings,
      const Economy& economy)
   {
      if (_active)
      {
         _stream.close();
         _active = false;
         _buffer.clear();
         _bufferedLineCount = 0;
      }
      std::error_code errorCode;
      std::filesystem::create_directories(logsDirectory, errorCode);

      const std::filesystem::path filePath =
         std::filesystem::path(logsDirectory) / BuildSessionFileName();
      _stream.open(filePath, std::ios::out | std::ios::trunc);
      if (!_stream.is_open())
      {
         return Result::FileError;
      }

      _active = true;
      _buffer.clear();
      _bufferedLineCount = 0;
      _fareLogCounter = 0;
      _realTimeSeconds = 0.0f;
      _snapshotAccumulatorSeconds = 0.0f;

      std::ostringstream header;
      header << "{\"type\":\"session_start\""
         << ",\"timestamp\":\"" << EscapeJsonString(CurrentTimestampIso()) << "\""
         << ",\"stationCap\":" << settings.stationCap
         << ",\"trainCapacity\":" << settings.trainCapacity
         << ",\"gameSpeed\":" << settings.gameSpeed
         << ",\"randomPool\":" << (settings.randomPool == RandomPool::Yes ? "true" : "false")
         << ",\"randomOrder\":" << (settings.randomOrder == RandomOrder::Yes ? "true" : "false")
         << ",\"eventsEnabled\":" << (settings.eventsEnabled == EventsEnabled::Yes ? "true" : "false")
         << ",\"neverLose\":" << (settings.neverLose == NeverLose::Yes ? "true" : "false")
         << ",\"economyScale\":" << economy.GetEconomyScale()
         << ",\"startingBalance\":" << economy.GetBalance()
         << ",\"defaultStartingBalance\":" << DefaultStartingBalance
         << ",\"trackBuildCostPerKm\":" << TrackBuildCostPerKm
         << ",\"trackMaintenanceCostPerKmPerSecond\":" << TrackMaintenanceCostPerKmPerSecond
         << ",\"trainPurchaseCost\":" << TrainPurchaseCost
         << ",\"trainMaintenanceCostPerSecond\":" << TrainMaintenanceCostPerSecond
         << ",\"farePerPassengerKm\":" << FarePerPassengerKm
         << "}";
      WriteLine(header.str());
      return Result::Ok;
   }

   void PlaySessionLog::EndSession(std::string_view reason, const World& world, Economy& economy)
   {
      if (!_active)
      {
         return;
      }

      WriteSnapshot(world, economy);
      std::ostringstream footer;
      footer << "{\"type\":\"session_end\""
         << ",\"reason\":\"" << EscapeJsonString(reason) << "\""
         << ",\"simTimeSeconds\":" << world.GetSimulationTimeSeconds()
         << ",\"realTimeSeconds\":" << _realTimeSeconds
         << ",\"balance\":" << economy.GetBalance()
         << ",\"lifetimeFareTotal\":" << economy.GetLifetimeFareTotal()
         << "}";
      WriteLine(footer.str());
      FlushBuffer();
      _stream.close();
      _active = false;
   }

   bool PlaySessionLog::IsActive(void) const
   {
      return _active;
   }

   void PlaySessionLog::LogTrackBuild(float newKm, int64_t cost, int64_t balanceAfter, float totalUniqueKm)
   {
      if (!_active)
      {
         return;
      }

      std::ostringstream line;
      line << "{\"type\":\"track_build\""
         << ",\"km\":" << newKm
         << ",\"cost\":" << cost
         << ",\"balanceAfter\":" << balanceAfter
         << ",\"totalUniqueKm\":" << totalUniqueKm
         << "}";
      WriteLine(line.str());
   }

   void PlaySessionLog::LogTrackBuildSkipped(StationId stationA, StationId stationB, float km)
   {
      if (!_active)
      {
         return;
      }

      std::ostringstream line;
      line << "{\"type\":\"track_build_skipped\""
         << ",\"stationA\":" << stationA
         << ",\"stationB\":" << stationB
         << ",\"km\":" << km
         << "}";
      WriteLine(line.str());
   }

   void PlaySessionLog::LogTrainPurchase(LineId lineId, int64_t cost, int64_t balanceAfter, uint32_t trainCount)
   {
      if (!_active)
      {
         return;
      }

      std::ostringstream line;
      line << "{\"type\":\"train_purchase\""
         << ",\"lineId\":" << lineId
         << ",\"cost\":" << cost
         << ",\"balanceAfter\":" << balanceAfter
         << ",\"trainCount\":" << trainCount
         << "}";
      WriteLine(line.str());
   }

   void PlaySessionLog::LogPurchaseBlocked(std::string_view action, int64_t cost, int64_t balance)
   {
      if (!_active)
      {
         return;
      }

      std::ostringstream line;
      line << "{\"type\":\"purchase_blocked\""
         << ",\"action\":\"" << EscapeJsonString(action) << "\""
         << ",\"cost\":" << cost
         << ",\"balance\":" << balance
         << "}";
      WriteLine(line.str());
   }

   void PlaySessionLog::LogFare(
      StationId originId,
      StationId destinationId,
      float beelineKm,
      int64_t amount,
      int64_t balanceAfter)
   {
      if (!_active || !ShouldLogFare())
      {
         return;
      }

      std::ostringstream line;
      line << "{\"type\":\"fare\""
         << ",\"originId\":" << originId
         << ",\"destId\":" << destinationId
         << ",\"beelineKm\":" << beelineKm
         << ",\"amount\":" << amount
         << ",\"balanceAfter\":" << balanceAfter
         << "}";
      WriteLine(line.str());
   }

   void PlaySessionLog::LogCrowdingDwell(StationId stationId, uint32_t waiting, uint32_t cap, float extraSeconds)
   {
      if (!_active)
      {
         return;
      }

      std::ostringstream line;
      line << "{\"type\":\"crowding_dwell\""
         << ",\"stationId\":" << stationId
         << ",\"waiting\":" << waiting
         << ",\"cap\":" << cap
         << ",\"extraSeconds\":" << extraSeconds
         << "}";
      WriteLine(line.str());
   }

   void PlaySessionLog::LogGameOver(
      GameOverReason reason,
      float negativeBalanceRealSeconds,
      int64_t finalBalance)
   {
      if (!_active)
      {
         return;
      }

      const char* reasonText = "bankruptcy";
      if (reason == GameOverReason::PlatformWait)
      {
         reasonText = "platform_wait";
      }

      std::ostringstream line;
      line << "{\"type\":\"game_over\""
         << ",\"reason\":\"" << reasonText << "\""
         << ",\"negativeBalanceRealSeconds\":" << negativeBalanceRealSeconds
         << ",\"balance\":" << finalBalance
         << "}";
      WriteLine(line.str());
   }

   void PlaySessionLog::Tick(float realDeltaSeconds, const World& world, Economy& economy)
   {
      if (!_active || realDeltaSeconds <= 0.0f)
      {
         return;
      }

      _realTimeSeconds += realDeltaSeconds;
      _snapshotAccumulatorSeconds += realDeltaSeconds;
      if (_snapshotAccumulatorSeconds < PlaySessionSnapshotRealSeconds)
      {
         return;
      }

      _snapshotAccumulatorSeconds = 0.0f;
      WriteSnapshot(world, economy);
   }

   void PlaySessionLog::WriteLine(std::string_view jsonLine)
   {
      if (!_active)
      {
         return;
      }

      _buffer.append(jsonLine);
      _buffer.push_back('\n');
      ++_bufferedLineCount;
      FlushIfNeeded();
   }

   void PlaySessionLog::FlushIfNeeded(void)
   {
      if (!_active || _bufferedLineCount < PlaySessionLogFlushLineCount)
      {
         return;
      }

      FlushBuffer();
   }

   void PlaySessionLog::FlushBuffer(void)
   {
      if (!_active || _buffer.empty())
      {
         return;
      }

      _stream << _buffer;
      _stream.flush();
      _buffer.clear();
      _bufferedLineCount = 0;
   }

   void PlaySessionLog::WriteSnapshot(const World& world, Economy& economy)
   {
      const float uniqueTrackKm = TotalUniqueTrackKilometers(world.GetNetwork());
      const uint32_t trainCount = static_cast<uint32_t>(world.GetTrains().size());
      const float maintenancePerSecond = economy.GetMaintenancePerSecond(uniqueTrackKm, trainCount);
      const int64_t fareSinceSnapshot = economy.GetFareSinceLastSnapshot();
      const uint32_t arrivalsSinceSnapshot = economy.GetArrivalsSinceLastSnapshot();
      economy.ResetSnapshotCounters();

      std::ostringstream line;
      line << "{\"type\":\"snapshot\""
         << ",\"simTimeSeconds\":" << world.GetSimulationTimeSeconds()
         << ",\"realTimeSeconds\":" << _realTimeSeconds
         << ",\"balance\":" << economy.GetBalance()
         << ",\"negativeBalanceRealSeconds\":" << economy.GetNegativeBalanceRealSeconds()
         << ",\"uniqueTrackKm\":" << uniqueTrackKm
         << ",\"lineCount\":" << world.GetNetwork().GetLines().size()
         << ",\"trainCount\":" << trainCount
         << ",\"stationCount\":" << world.GetNetwork().GetStations().size()
         << ",\"waiting\":" << world.GetWaitingPassengerCount()
         << ",\"onboard\":" << world.GetOnboardPassengerCount()
         << ",\"arrived\":" << world.GetArrivedPassengerCount()
         << ",\"spawnRate\":" <<
            (PassengerSpawnPerSecondForCapacity(world.GetTrainCapacity()) *
               world.GetPassengerSpawnPressureMultiplier())
         << ",\"spawnPressure\":" << world.GetPassengerSpawnPressureMultiplier()
         << ",\"maintenancePerSecond\":" << maintenancePerSecond
         << ",\"fareSinceLastSnapshot\":" << fareSinceSnapshot
         << ",\"arrivalsSinceLastSnapshot\":" << arrivalsSinceSnapshot
         << "}";
      WriteLine(line.str());
   }

   bool PlaySessionLog::ShouldLogFare(void)
   {
      ++_fareLogCounter;
      if (_realTimeSeconds < PlaySessionFullFareLogRealSeconds)
      {
         return true;
      }

      return (_fareLogCounter % PlaySessionFareSampleInterval) == 0;
   }
} // namespace MiniDb

/*!
 *\file play_session_log.h
 *\brief JSONL play session logging for economy tuning.
 */

#ifndef PLAY_SESSION_LOG_H
#define PLAY_SESSION_LOG_H

#include <cstdint>
#include <fstream>
#include <string>

#include "simulation/economy.h"
#include "simulation/world.h"

namespace MiniDb
{
   struct PlaySessionSettings
   {
      uint32_t stationCap = DefaultMaxStationCount;
      uint32_t trainCapacity = DefaultTrainCapacity;
      float gameSpeed = DefaultTimeScale;
      RandomPool randomPool = RandomPool::No;
      RandomOrder randomOrder = RandomOrder::No;
      EventsEnabled eventsEnabled = EventsEnabled::No;
      NeverLose neverLose = NeverLose::No;
   };

   class PlaySessionLog
   {
   public:
      PlaySessionLog(void);

      /*!
       *\brief Opens a new log file for an economic session.
       *
       *\param[in] logsDirectory Directory that will receive the JSONL file.
       *\param[in] settings Settings used for this run.
       *\param[in] economy Economy state at session start.
       */
      Result BeginSession(
         std::string_view logsDirectory,
         const PlaySessionSettings& settings,
         const Economy& economy);

      /*!
       *\brief Closes the active session and writes a final snapshot.
       *
       *\param[in] reason Why the session ended.
       *\param[in] world World state at session end.
       *\param[in] economy Economy state at session end.
       */
      void EndSession(std::string_view reason, const World& world, Economy& economy);

      /*!
       *\brief Returns true when a session file is open.
       */
      bool IsActive(void) const;

      void LogTrackBuild(float newKm, int64_t cost, int64_t balanceAfter, float totalUniqueKm);
      void LogTrackBuildSkipped(StationId stationA, StationId stationB, float km);
      void LogTrainPurchase(LineId lineId, int64_t cost, int64_t balanceAfter, uint32_t trainCount);
      void LogPurchaseBlocked(std::string_view action, int64_t cost, int64_t balance);
      void LogFare(
         StationId originId,
         StationId destinationId,
         float beelineKm,
         int64_t amount,
         int64_t balanceAfter);
      void LogCrowdingDwell(StationId stationId, uint32_t waiting, uint32_t cap, float extraSeconds);
      void LogGameOver(float negativeBalanceRealSeconds, int64_t finalBalance);

      /*!
       *\brief Writes periodic snapshots and flushes buffered lines.
       *
       *\param[in] realDeltaSeconds Wall-clock seconds since last frame.
       *\param[in] world Current world state.
       *\param[in] economy Current economy state.
       */
      void Tick(float realDeltaSeconds, const World& world, Economy& economy);

   private:
      void WriteLine(std::string_view jsonLine);
      void FlushIfNeeded(void);
      void FlushBuffer(void);
      void WriteSnapshot(const World& world, Economy& economy);
      bool ShouldLogFare(void);

      std::ofstream _stream;
      std::string _buffer;
      uint32_t _bufferedLineCount = 0;
      uint32_t _fareLogCounter = 0;
      float _realTimeSeconds = 0.0f;
      float _snapshotAccumulatorSeconds = 0.0f;
      bool _active = false;
   };
} // namespace MiniDb

#endif // PLAY_SESSION_LOG_H

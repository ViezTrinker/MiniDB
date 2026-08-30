/*!
 *\file ai_actions.h
 *\brief Discrete actions the spectator AI can apply to World.
 */

#ifndef AI_ACTIONS_H
#define AI_ACTIONS_H

#include <cstdint>
#include <vector>

#include "core/types.h"
#include "simulation/network.h"

namespace MiniDb
{
   enum class AiActionType : uint8_t
   {
      Noop = 0,
      AddLine = 1,
      ExtendLine = 2,
      InsertStation = 3,
      AddTrain = 4,
      RemoveLine = 5
   };

   enum class AllowAiReserveSpend : bool
   {
      No = false,
      Yes = true
   };

   struct AiAction
   {
      AiActionType type = AiActionType::Noop;
      StationIdList stationIds;
      LineId lineId = InvalidLineId;
      LineEnd lineEnd = LineEnd::Back;
      uint32_t segmentIndex = InvalidIndex;
      StationId stationId = InvalidStationId;
      float score = 0.0f;
   };

   using AiActionList = std::vector<AiAction>;
} // namespace MiniDb

#endif // AI_ACTIONS_H

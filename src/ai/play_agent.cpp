/*!
 *\file play_agent.cpp
 *\brief Spectator AI that drives World build actions.
 */

#include "ai/play_agent.h"

#include "ai/ai_observation.h"
#include "ai/ai_planner.h"
#include "core/constants.h"

namespace MiniDb
{
   PlayAgent::PlayAgent(void)
   {
   }

   void PlayAgent::Reset(void)
   {
      _decisionAccumulatorSeconds = 0.0f;
      _lastActionType = AiActionType::Noop;
   }

   Result PlayAgent::Step(World& world, float simulationDeltaSeconds)
   {
      if (simulationDeltaSeconds <= 0.0f)
      {
         return Result::Ok;
      }

      _decisionAccumulatorSeconds += simulationDeltaSeconds;
      if (_decisionAccumulatorSeconds < AiDecisionIntervalSeconds)
      {
         return Result::Ok;
      }

      _decisionAccumulatorSeconds = 0.0f;

      AiAction action;
      PlanAiAction(world, action);
      _lastActionType = action.type;
      if (action.type == AiActionType::Noop)
      {
         return Result::Ok;
      }

      return ApplyAiAction(world, action);
   }

   AiActionType PlayAgent::GetLastActionType(void) const
   {
      return _lastActionType;
   }
} // namespace MiniDb

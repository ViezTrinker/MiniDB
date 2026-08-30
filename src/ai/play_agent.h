/*!
 *\file play_agent.h
 *\brief Spectator AI that drives World build actions.
 */

#ifndef PLAY_AGENT_H
#define PLAY_AGENT_H

#include "ai/ai_actions.h"
#include "core/result.h"
#include "simulation/world.h"

namespace MiniDb
{
   enum class AiPlay : bool
   {
      No = false,
      Yes = true
   };

   class PlayAgent
   {
   public:
      PlayAgent(void);

      /*!
       *\brief Clears decision timing state for a new game.
       */
      void Reset(void);

      /*!
       *\brief Advances the agent clock and may apply one World action.
       *
       *\param[in,out] world Simulation owned by the game.
       *\param[in] simulationDeltaSeconds Sim seconds about to be ticked.
       */
      Result Step(World& world, float simulationDeltaSeconds);

      /*!
       *\brief Returns the last action type applied (or Noop).
       */
      AiActionType GetLastActionType(void) const;

   private:
      float _decisionAccumulatorSeconds = 0.0f;
      AiActionType _lastActionType = AiActionType::Noop;
   };
} // namespace MiniDb

#endif // PLAY_AGENT_H

/*!
 *\file ai_planner.h
 *\brief Candidate generation and greedy selection for the spectator AI.
 */

#ifndef AI_PLANNER_H
#define AI_PLANNER_H

#include "ai/ai_actions.h"
#include "ai/ai_observation.h"
#include "simulation/world.h"

namespace MiniDb
{
   /*!
    *\brief Picks the highest-scoring affordable action for the current world.
    *
    *\param[in] world Current simulation.
    *\param[out] action Chosen action (Noop if nothing useful).
    */
   void PlanAiAction(const World& world, AiAction& action);
} // namespace MiniDb

#endif // AI_PLANNER_H

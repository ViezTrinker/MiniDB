/*!
 *\file ai_scorer.h
 *\brief Heuristic scores for spectator AI candidate actions.
 */

#ifndef AI_SCORER_H
#define AI_SCORER_H

#include "ai/ai_actions.h"
#include "ai/ai_observation.h"
#include "simulation/world.h"

namespace MiniDb
{
   /*!
    *\brief Scores a candidate action given the current observation.
    *
    *\param[in] world Live world (for positions / gravity).
    *\param[in] observation Snapshot used for demand and patience.
    *\param[in,out] action Action whose score field is filled.
    */
   void ScoreAiAction(const World& world, const AiObservation& observation, AiAction& action);
} // namespace MiniDb

#endif // AI_SCORER_H

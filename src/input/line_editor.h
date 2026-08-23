/*!
 *\file line_editor.h
 *\brief Mouse-driven line drafting.
 */

#ifndef LINE_EDITOR_H
#define LINE_EDITOR_H

#include "core/result.h"
#include "core/types.h"
#include "simulation/world.h"

namespace MiniDb
{
   class LineEditor
   {
   public:
      /*!
       *\brief Handles a left click on a station.
       *
       *\param[in,out] world Simulation to mutate.
       *\param[in] stationId Clicked station.
       */
      Result OnStationClicked(World& world, StationId stationId);

      /*!
       *\brief Finishes the current draft as a new line.
       *
       *\param[in,out] world Simulation to mutate.
       */
      Result Confirm(World& world);

      /*!
       *\brief Cancels the current draft.
       */
      Result Cancel(void);

      /*!
       *\brief Clears draft and selection state.
       */
      void Reset(void);

      /*!
       *\brief Adds another train to the last selected line.
       *
       *\param[in,out] world Simulation to mutate.
       */
      Result AddTrainToSelectedLine(World& world);

      /*!
       *\brief Marks a finished line as selected.
       *
       *\param[in] lineId Line to select, or InvalidLineId.
       */
      void SelectLine(LineId lineId);

      /*!
       *\brief Deletes the selected finished line.
       *
       *\param[in,out] world Simulation to mutate.
       */
      Result DeleteSelectedLine(World& world);

      /*!
       *\brief Removes the last drafted station.
       */
      Result UndoDraft(void);

      /*!
       *\brief Restores the last undone draft station.
       */
      Result RedoDraft(void);

      /*!
       *\brief Returns true while a line is being drafted.
       */
      bool IsDrafting(void) const;

      const StationIdList& GetDraftStationIds(void) const;
      LineId GetSelectedLineId(void) const;

   private:
      void ClearRedo(void);
      void AppendDraftStation(StationId stationId);

      StationIdList _draftStationIds;
      StationIdList _redoStationIds;
      LineId _selectedLineId = InvalidLineId;
   };
} // namespace MiniDb

#endif // LINE_EDITOR_H

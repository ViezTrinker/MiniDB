/*!
 *\file line_editor.cpp
 *\brief Mouse-driven line drafting.
 */

#include "input/line_editor.h"

#include "core/constants.h"

namespace MiniDb
{
   Result LineEditor::OnStationClicked(World& world, StationId stationId)
   {
      if (world.GetNetwork().FindStation(stationId) == nullptr)
      {
         return Result::StationNotFound;
      }

      if (!_draftStationIds.empty())
      {
         if (_draftStationIds.back() == stationId)
         {
            return Result::Ok;
         }

         if (_draftStationIds.front() == stationId && _draftStationIds.size() >= MinimumLoopStations)
         {
            AppendDraftStation(stationId);
            return Confirm(world);
         }

         for (StationId existingId : _draftStationIds)
         {
            if (existingId == stationId)
            {
               return Result::DuplicateStation;
            }
         }

         AppendDraftStation(stationId);
         return Result::Ok;
      }

      _draftStationIds.clear();
      AppendDraftStation(stationId);
      return Result::Ok;
   }

   Result LineEditor::Confirm(World& world)
   {
      if (_draftStationIds.size() < MinimumLineStations)
      {
         Cancel();
         return Result::LineTooShort;
      }

      LineId lineId = InvalidLineId;
      const Result result = world.AddLine(_draftStationIds, lineId);
      if (IsErr(result))
      {
         return result;
      }

      _selectedLineId = lineId;
      _draftStationIds.clear();
      ClearRedo();
      return Result::Ok;
   }

   Result LineEditor::Cancel(void)
   {
      _draftStationIds.clear();
      ClearRedo();
      return Result::Ok;
   }

   void LineEditor::Reset(void)
   {
      _draftStationIds.clear();
      _selectedLineId = InvalidLineId;
      ClearRedo();
   }

   Result LineEditor::AddTrainToSelectedLine(World& world)
   {
      if (_selectedLineId == InvalidLineId)
      {
         return Result::InvalidArgument;
      }

      return world.AddTrainToLine(_selectedLineId);
   }

   void LineEditor::SelectLine(LineId lineId)
   {
      _selectedLineId = lineId;
   }

   Result LineEditor::DeleteSelectedLine(World& world)
   {
      if (_selectedLineId == InvalidLineId)
      {
         return Result::InvalidArgument;
      }

      const LineId lineId = _selectedLineId;
      Cancel();
      const Result result = world.RemoveLine(lineId);
      _selectedLineId = InvalidLineId;
      return result;
   }

   void LineEditor::ClearRedo(void)
   {
      _redoStationIds.clear();
   }

   void LineEditor::AppendDraftStation(StationId stationId)
   {
      _draftStationIds.push_back(stationId);
      ClearRedo();
   }

   Result LineEditor::UndoDraft(void)
   {
      if (_draftStationIds.empty())
      {
         return Result::InvalidArgument;
      }

      _redoStationIds.push_back(_draftStationIds.back());
      _draftStationIds.pop_back();
      return Result::Ok;
   }

   Result LineEditor::RedoDraft(void)
   {
      if (_redoStationIds.empty())
      {
         return Result::InvalidArgument;
      }

      _draftStationIds.push_back(_redoStationIds.back());
      _redoStationIds.pop_back();
      return Result::Ok;
   }

   bool LineEditor::IsDrafting(void) const
   {
      return !_draftStationIds.empty();
   }

   const StationIdList& LineEditor::GetDraftStationIds(void) const
   {
      return _draftStationIds;
   }

   LineId LineEditor::GetSelectedLineId(void) const
   {
      return _selectedLineId;
   }
} // namespace MiniDb

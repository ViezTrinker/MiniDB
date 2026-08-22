/*!
 *\file line_editor.cpp
 *\brief Mouse-driven line drafting and extension.
 */

#include "input/line_editor.h"

#include "core/constants.h"

namespace MiniDb
{
   bool LineEditor::IsTerminalOfLine(const World& world, LineId lineId, StationId stationId) const
   {
      const Line* pLine = world.GetNetwork().FindLine(lineId);
      if (pLine == nullptr || pLine->stationIds.size() < MinimumLineStations)
      {
         return false;
      }
      if (pLine->loop == LineLoop::Yes)
      {
         return false;
      }

      return pLine->stationIds.front() == stationId || pLine->stationIds.back() == stationId;
   }

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

         if (_extendLineId != InvalidLineId)
         {
            const Line* pLine = world.GetNetwork().FindLine(_extendLineId);
            if (pLine != nullptr &&
               pLine->loop == LineLoop::No &&
               stationId == pLine->stationIds.front() &&
               pLine->stationIds.size() >= MinimumLoopStations)
            {
               AppendDraftStation(stationId);
               return Confirm(world);
            }
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

      if (_selectedLineId != InvalidLineId && IsTerminalOfLine(world, _selectedLineId, stationId))
      {
         const Line* pLine = world.GetNetwork().FindLine(_selectedLineId);
         if (pLine != nullptr && pLine->stationIds.back() == stationId)
         {
            _extendLineId = _selectedLineId;
            AppendDraftStation(stationId);
            return Result::Ok;
         }
      }

      _extendLineId = InvalidLineId;
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

      if (_extendLineId != InvalidLineId)
      {
         for (uint32_t index = 1; index < _draftStationIds.size(); ++index)
         {
            const Result result = world.ExtendLine(_extendLineId, _draftStationIds[index]);
            if (IsErr(result))
            {
               return result;
            }
         }

         _selectedLineId = _extendLineId;
         _extendLineId = InvalidLineId;
         _draftStationIds.clear();
         ClearRedo();
         return Result::Ok;
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
      _extendLineId = InvalidLineId;
      ClearRedo();
      return Result::Ok;
   }

   void LineEditor::Reset(void)
   {
      _draftStationIds.clear();
      _extendLineId = InvalidLineId;
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
      _extendLineId = InvalidLineId;
      return result;
   }

   void LineEditor::ClearRedo(void)
   {
      _redoStationIds.clear();
      _redoExtendLineId = InvalidLineId;
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

      if (_draftStationIds.size() == 1)
      {
         _redoExtendLineId = _extendLineId;
         _extendLineId = InvalidLineId;
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

      if (_draftStationIds.empty())
      {
         _extendLineId = _redoExtendLineId;
         _redoExtendLineId = InvalidLineId;
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

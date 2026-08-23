/*!
 *\file renderer.cpp
 *\brief SFML drawing of the Germany map, network and HUD.
 */

#include "rendering/renderer.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <string>

#include "core/constants.h"
#include "geo/geojson_outline.h"
#include "geo/projection.h"
#include "rendering/utf8_text.h"
#include "simulation/train.h"

namespace MiniDb
{
   namespace
   {
      using LineColorPalette = std::array<sf::Color, LineColorCount>;

      const LineColorPalette LineColors = {
         sf::Color(220, 50, 47),
         sf::Color(38, 139, 210),
         sf::Color(133, 153, 0),
         sf::Color(181, 137, 0),
         sf::Color(211, 54, 130),
         sf::Color(42, 161, 152),
         sf::Color(203, 75, 22),
         sf::Color(108, 113, 196)
      };

      const sf::Color InspectedStationColor(30, 90, 160);
      const sf::Color InspectedTrainOutlineColor(255, 255, 255);

      std::string StationCityName(const World& world, StationId stationId)
      {
         const StationRecord* pStation = world.GetNetwork().FindStation(stationId);
         if (pStation == nullptr)
         {
            return std::string("?");
         }

         return pStation->cityName;
      }

      std::string LineInspectorTitle(const World& world, const Line& line)
      {
         if (line.stationIds.size() < MinimumLineStations)
         {
            return std::string("Line");
         }

         std::string title = StationCityName(world, line.stationIds.front());
         title += " - ";
         title += StationCityName(world, line.stationIds.back());
         return title;
      }

      uint32_t ClampedInspectorRowCount(uint32_t rowCount)
      {
         if (rowCount == 0)
         {
            return 1;
         }
         if (rowCount > InspectorMaxRows)
         {
            return InspectorMaxRows;
         }

         return rowCount;
      }

      uint32_t ClampedCrowdedStationRowCount(uint32_t rowCount)
      {
         if (rowCount == 0)
         {
            return 1;
         }
         if (rowCount > CrowdedStationMaxRows)
         {
            return CrowdedStationMaxRows;
         }

         return rowCount;
      }

      enum class BottomHudControl : uint32_t
      {
         Help = 0,
         Train = 1,
         SlowDown = 2,
         SpeedLabel = 3,
         SpeedUp = 4,
         Pause = 5,
         Resume = 6,
         Menu = 7
      };

      float BottomHudControlWidth(BottomHudControl control)
      {
         if (control == BottomHudControl::Train)
         {
            return TrainTokenWidthPixels;
         }
         if (control == BottomHudControl::SpeedLabel)
         {
            return HudSpeedLabelWidthPixels;
         }
         if (control == BottomHudControl::Pause ||
            control == BottomHudControl::Resume ||
            control == BottomHudControl::Menu)
         {
            return HudTextButtonWidthPixels;
         }

         return HudButtonSizePixels;
      }

      void DrawHudButtonFrame(
         sf::RenderWindow& window,
         const sf::FloatRect& bounds,
         const sf::Color& fillColor)
      {
         sf::RectangleShape button(bounds.size);
         button.setPosition(bounds.position);
         button.setFillColor(fillColor);
         button.setOutlineColor(sf::Color(30, 30, 30));
         button.setOutlineThickness(1.0f);
         window.draw(button);
      }

      void DrawCenteredHudLabel(
         sf::RenderWindow& window,
         const sf::Font& font,
         const sf::FloatRect& bounds,
         std::string_view label,
         unsigned int characterSize,
         const sf::Color& color)
      {
         sf::Text text(font, Utf8SfString(label), characterSize);
         text.setFillColor(color);
         const sf::FloatRect textBounds = text.getLocalBounds();
         text.setPosition({
            bounds.position.x + ((bounds.size.x - textBounds.size.x) * 0.5f) - textBounds.position.x,
            bounds.position.y + ((bounds.size.y - textBounds.size.y) * 0.5f) - textBounds.position.y
         });
         window.draw(text);
      }

      sf::Color ColorForLine(uint32_t colorIndex)
      {
         return LineColors[colorIndex % LineColorCount];
      }

      float StationRadiusPixels(uint32_t population)
      {
         float radiusPixels = StationRadiusBasePixels;
         if (population > MinimumCityPopulation)
         {
            const float populationRatio =
               static_cast<float>(population) / static_cast<float>(MinimumCityPopulation);
            radiusPixels += 1.6f * std::log10(populationRatio);
         }
         if (radiusPixels > StationRadiusMaxPixels)
         {
            radiusPixels = StationRadiusMaxPixels;
         }
         return radiusPixels;
      }

      void CanonicalStationPair(StationId leftId, StationId rightId, StationId& lowId, StationId& highId)
      {
         if (leftId < rightId)
         {
            lowId = leftId;
            highId = rightId;
            return;
         }

         lowId = rightId;
         highId = leftId;
      }

      bool LineHasUndirectedSegment(const Line& line, StationId lowId, StationId highId)
      {
         const uint32_t segmentCount = LineSegmentCount(line);
         for (uint32_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
         {
            StationId fromId = InvalidStationId;
            StationId toId = InvalidStationId;
            const Result endpointResult = LineSegmentEndpoints(line, segmentIndex, fromId, toId);
            if (IsErr(endpointResult))
            {
               continue;
            }

            StationId pairLow = InvalidStationId;
            StationId pairHigh = InvalidStationId;
            CanonicalStationPair(fromId, toId, pairLow, pairHigh);
            if (pairLow == lowId && pairHigh == highId)
            {
               return true;
            }
         }

         return false;
      }

      uint32_t SharedLineCountOnPair(const LineList& lines, StationId lowId, StationId highId)
      {
         uint32_t count = 0;
         for (const Line& line : lines)
         {
            if (LineHasUndirectedSegment(line, lowId, highId))
            {
               ++count;
            }
         }

         return count;
      }

      uint32_t SharedLineSlotOnPair(const LineList& lines, LineId lineId, StationId lowId, StationId highId)
      {
         uint32_t slotIndex = 0;
         for (const Line& line : lines)
         {
            if (line.id >= lineId)
            {
               continue;
            }
            if (LineHasUndirectedSegment(line, lowId, highId))
            {
               ++slotIndex;
            }
         }

         return slotIndex;
      }

      MapPoint OffsetAlongPerpendicular(MapPoint point, MapPoint from, MapPoint to, float offsetKm)
      {
         const float deltaX = to.xKm - from.xKm;
         const float deltaY = to.yKm - from.yKm;
         const float lengthKm = DistanceKm(from, to);
         if (lengthKm <= 0.05f)
         {
            return point;
         }

         MapPoint offsetPoint;
         offsetPoint.xKm = point.xKm + ((-deltaY) / lengthKm) * offsetKm;
         offsetPoint.yKm = point.yKm + (deltaX / lengthKm) * offsetKm;
         return offsetPoint;
      }

      void ClampViewSize(sf::View& mapView)
      {
         sf::Vector2f size = mapView.getSize();
         const float minimumWidth = 80.0f;
         const float maximumWidth = 2200.0f;
         if (size.x < minimumWidth)
         {
            const float scale = minimumWidth / size.x;
            size.x *= scale;
            size.y *= scale;
         }
         if (size.x > maximumWidth)
         {
            const float scale = maximumWidth / size.x;
            size.x *= scale;
            size.y *= scale;
         }
         mapView.setSize(size);
      }
   } // namespace

   Result Renderer::Initialize(sf::RenderWindow* pWindow, sf::Font* pFont)
   {
      if (pWindow == nullptr || pFont == nullptr)
      {
         return Result::InvalidArgument;
      }

      _pWindow = pWindow;
      _pFont = pFont;
      FitGermany();
      return Result::Ok;
   }

   Result Renderer::LoadOutline(std::string_view filePath)
   {
      const Result result = LoadOutlineFromFile(filePath, _outlinePolygons);
      if (IsErr(result))
      {
         return result;
      }

      RebuildOutlineVertexArrays();
      return Result::Ok;
   }

   float Renderer::SidebarWidthPixels(void) const
   {
      if (_pWindow == nullptr || _mapSidebar == MapSidebar::Hidden)
      {
         return 0.0f;
      }

      const float windowWidth = static_cast<float>(_pWindow->getSize().x);
      float panelWidth = UnconnectedPanelWidthPixels;
      if (windowWidth <= (panelWidth + 320.0f))
      {
         panelWidth = windowWidth * 0.28f;
      }

      return panelWidth;
   }

   float Renderer::MapWidthPixels(void) const
   {
      if (_pWindow == nullptr)
      {
         return 0.0f;
      }

      const float windowWidth = static_cast<float>(_pWindow->getSize().x);
      const float mapWidth = windowWidth - SidebarWidthPixels();
      if (mapWidth < 1.0f)
      {
         return 1.0f;
      }

      return mapWidth;
   }

   void Renderer::ApplyMapViewport(void)
   {
      if (_pWindow == nullptr)
      {
         return;
      }

      const float windowWidth = static_cast<float>(_pWindow->getSize().x);
      if (windowWidth <= 0.0f)
      {
         return;
      }

      const float viewportWidth = MapWidthPixels() / windowWidth;
      _mapView.setViewport(sf::FloatRect({0.0f, 0.0f}, {viewportWidth, 1.0f}));
   }

   void Renderer::ApplyDefaultView(void)
   {
      if (_pWindow == nullptr)
      {
         return;
      }

      const sf::Vector2u size = _pWindow->getSize();
      if (size.x == 0 || size.y == 0)
      {
         return;
      }

      sf::View defaultView;
      defaultView.setSize({static_cast<float>(size.x), static_cast<float>(size.y)});
      defaultView.setCenter({static_cast<float>(size.x) * 0.5f, static_cast<float>(size.y) * 0.5f});
      _pWindow->setView(defaultView);
   }

   void Renderer::SyncWindowViews(void)
   {
      ApplyDefaultView();
      ApplyMapViewport();
   }

   void Renderer::SetMapSidebar(MapSidebar sidebar)
   {
      _mapSidebar = sidebar;
      FitGermany();
   }

   void Renderer::CenterOn(MapPoint point)
   {
      _mapView.setCenter({point.xKm, point.yKm});
   }

   void Renderer::FitGermany(void)
   {
      if (_pWindow == nullptr)
      {
         return;
      }

      const float mapWidth = MapWidthKm() + (2.0f * MapViewMarginKm);
      const float mapHeight = MapHeightKm() + (2.0f * MapViewMarginKm);
      const auto windowSize = _pWindow->getSize();
      const float mapPixelWidth = MapWidthPixels();
      const float windowHeight = static_cast<float>(windowSize.y);
      if (mapPixelWidth <= 0.0f || windowHeight <= 0.0f)
      {
         return;
      }

      const float windowAspect = mapPixelWidth / windowHeight;
      const float mapAspect = mapWidth / mapHeight;
      sf::Vector2f viewSize;
      if (windowAspect > mapAspect)
      {
         viewSize.x = mapHeight * windowAspect;
         viewSize.y = mapHeight;
      }
      else
      {
         viewSize.x = mapWidth;
         viewSize.y = mapWidth / windowAspect;
      }

      _mapView.setSize(viewSize);
      _mapView.setCenter({MapWidthKm() * 0.5f, MapHeightKm() * 0.5f});
      ApplyMapViewport();
   }

   void Renderer::HandleResize(uint32_t width, uint32_t height)
   {
      if (_pWindow == nullptr || width == 0 || height == 0)
      {
         return;
      }

      ApplyDefaultView();
      FitGermany();
   }

   void Renderer::ZoomAt(sf::Vector2i pixel, float factor)
   {
      if (_pWindow == nullptr)
      {
         return;
      }

      const sf::Vector2f before = _pWindow->mapPixelToCoords(pixel, _mapView);
      _mapView.zoom(factor);
      ClampViewSize(_mapView);
      const sf::Vector2f after = _pWindow->mapPixelToCoords(pixel, _mapView);
      _mapView.move(before - after);
   }

   void Renderer::Pan(sf::Vector2f deltaKm)
   {
      _mapView.move(deltaKm);
   }

   MapPoint Renderer::ScreenToMap(sf::Vector2i pixel) const
   {
      MapPoint point;
      point.xKm = 0.0f;
      point.yKm = 0.0f;
      if (_pWindow == nullptr)
      {
         return point;
      }

      const sf::Vector2f world = _pWindow->mapPixelToCoords(pixel, _mapView);
      point.xKm = world.x;
      point.yKm = world.y;
      return point;
   }

   sf::Vector2f Renderer::PixelDeltaToMapDelta(sf::Vector2i pixelDelta) const
   {
      if (_pWindow == nullptr)
      {
         return {0.0f, 0.0f};
      }

      const sf::Vector2f worldZero = _pWindow->mapPixelToCoords({0, 0}, _mapView);
      const sf::Vector2f worldMoved = _pWindow->mapPixelToCoords(pixelDelta, _mapView);
      return worldMoved - worldZero;
   }

   float Renderer::PixelsToKm(float pixels) const
   {
      if (_pWindow == nullptr)
      {
         return pixels;
      }

      const float mapPixelWidth = MapWidthPixels();
      if (mapPixelWidth <= 0.0f)
      {
         return pixels;
      }

      return pixels * (_mapView.getSize().x / mapPixelWidth);
   }

   float Renderer::HitRadiusKm(void) const
   {
      return PixelsToKm(StationHitRadiusPixels);
   }

   sf::Vector2i Renderer::MapViewCenterPixel(void) const
   {
      if (_pWindow == nullptr)
      {
         return {0, 0};
      }

      const auto windowHeight = static_cast<int32_t>(_pWindow->getSize().y);
      return {
         static_cast<int32_t>(MapWidthPixels() * 0.5f),
         windowHeight / 2
      };
   }

   void Renderer::DrawScaledText(std::string_view text, MapPoint position, unsigned int characterSize, sf::Color color)
   {
      if (_pWindow == nullptr || _pFont == nullptr)
      {
         return;
      }

      sf::Text label(*_pFont, Utf8SfString(text), characterSize);
      const float kmPerPixel = PixelsToKm(1.0f);
      label.setScale({kmPerPixel, kmPerPixel});
      label.setFillColor(color);
      label.setPosition({position.xKm, position.yKm});
      _pWindow->draw(label);
   }

   void Renderer::DrawMenuBackdrop(void)
   {
      if (_pWindow == nullptr)
      {
         return;
      }

      SyncWindowViews();
      _pWindow->clear(sf::Color(236, 230, 218));
      _pWindow->setView(_mapView);
      DrawOutline();
      ApplyDefaultView();

      const auto windowWidth = static_cast<float>(_pWindow->getSize().x);
      const auto windowHeight = static_cast<float>(_pWindow->getSize().y);
      sf::RectangleShape dim({windowWidth, windowHeight});
      dim.setFillColor(sf::Color(236, 230, 218, 170));
      _pWindow->draw(dim);
   }

   void Renderer::Draw(
         const World& world,
         const StationIdList& draftStationIds,
         StationId hoveredStationId,
         StationId inspectedStationId,
         TrainId inspectedTrainId,
         LineId inspectedLineId,
         LineId highlightLineId,
         std::string_view statusText,
         HelpVisible helpVisible,
         SimulationPause pause,
         float timeScale,
         TrainDrag trainDrag,
         LineDrag lineDrag,
         const LineDragPreview& lineDragPreview,
         AnchorDrag anchorDrag,
         const TerminusAnchorPreview& anchorDragPreview,
         float unconnectedScrollPixels,
         sf::Vector2i cursorPixel)
   {
      if (_pWindow == nullptr)
      {
         return;
      }

      SyncWindowViews();
      _pWindow->clear(sf::Color(236, 230, 218));
      _pWindow->setView(_mapView);
      DrawOutline();
      DrawLines(world, highlightLineId, lineDrag, lineDragPreview);
      if (lineDrag == LineDrag::Yes)
      {
         DrawLineInsertPreview(world, lineDragPreview, cursorPixel);
      }
      if (anchorDrag == AnchorDrag::Yes)
      {
         DrawTerminusExtendPreview(world, anchorDragPreview, cursorPixel);
      }
      DrawTerminusAnchors(world, highlightLineId, anchorDrag, anchorDragPreview);
      DrawDraft(world, draftStationIds);
      DrawStations(world, hoveredStationId, inspectedStationId);
      DrawTrains(world, inspectedTrainId);
      if (trainDrag == TrainDrag::Yes)
      {
         DrawDraggedTrain(cursorPixel);
      }
      DrawHud(statusText);
      DrawHelpButton(helpVisible);
      DrawTrainToken(trainDrag);
      DrawPlaybackControls(pause, timeScale);
      if (helpVisible == HelpVisible::Yes)
      {
         DrawHelpPopup();
      }
      DrawSidebar(
         world,
         inspectedStationId,
         inspectedTrainId,
         inspectedLineId,
         unconnectedScrollPixels,
         cursorPixel);
   }

   void Renderer::DrawOutline(void)
   {
      if (_pWindow == nullptr)
      {
         return;
      }

      for (const sf::VertexArray& vertices : _outlineVertexArrays)
      {
         _pWindow->draw(vertices);
      }
   }

   void Renderer::RebuildOutlineVertexArrays(void)
   {
      _outlineVertexArrays.clear();
      for (const MapPolygon& polygon : _outlinePolygons)
      {
         if (polygon.size() < 2)
         {
            continue;
         }

         sf::VertexArray vertices(sf::PrimitiveType::LineStrip, polygon.size());
         for (uint32_t index = 0; index < polygon.size(); ++index)
         {
            vertices[index].position = {polygon[index].xKm, polygon[index].yKm};
            vertices[index].color = sf::Color(150, 140, 128);
         }
         _outlineVertexArrays.push_back(vertices);
      }
   }

   namespace
   {
      uint64_t SegmentOffsetKey(LineId lineId, uint32_t segmentIndex)
      {
         return (static_cast<uint64_t>(lineId) << 32) |
            static_cast<uint64_t>(segmentIndex);
      }
   } // namespace

   void Renderer::RebuildParallelOffsetCache(const World& world)
   {
      const uint64_t revision = world.GetNetwork().GetRevision();
      if (_parallelOffsetNetworkRevision == revision)
      {
         return;
      }

      _parallelOffsetKmBySegment.clear();
      const LineList& lines = world.GetNetwork().GetLines();
      for (const Line& line : lines)
      {
         const uint32_t segmentCount = LineSegmentCount(line);
         for (uint32_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
         {
            StationId fromId = InvalidStationId;
            StationId toId = InvalidStationId;
            const Result endpointResult = LineSegmentEndpoints(line, segmentIndex, fromId, toId);
            if (IsErr(endpointResult))
            {
               continue;
            }

            StationId pairLow = InvalidStationId;
            StationId pairHigh = InvalidStationId;
            CanonicalStationPair(fromId, toId, pairLow, pairHigh);
            const uint32_t sharedCount = SharedLineCountOnPair(lines, pairLow, pairHigh);
            const uint32_t slotIndex = SharedLineSlotOnPair(lines, line.id, pairLow, pairHigh);
            float offsetKm = 0.0f;
            if (sharedCount > 1)
            {
               const auto centeredSlot =
                  static_cast<float>(slotIndex) - (static_cast<float>(sharedCount - 1) * 0.5f);
               offsetKm = centeredSlot * PixelsToKm(ParallelLineOffsetPixels);
            }

            _parallelOffsetKmBySegment[SegmentOffsetKey(line.id, segmentIndex)] = offsetKm;
         }
      }

      _parallelOffsetNetworkRevision = revision;
   }

   float Renderer::ParallelOffsetKm(LineId lineId, uint32_t segmentIndex) const
   {
      const auto iterator = _parallelOffsetKmBySegment.find(SegmentOffsetKey(lineId, segmentIndex));
      if (iterator == _parallelOffsetKmBySegment.end())
      {
         return 0.0f;
      }

      return iterator->second;
   }

   void Renderer::DrawSegment(MapPoint from, MapPoint to, sf::Color color, float thicknessKm)
   {
      if (_pWindow == nullptr)
      {
         return;
      }

      const float lengthKm = DistanceKm(from, to);
      if (lengthKm <= 0.05f)
      {
         return;
      }

      sf::RectangleShape segment({lengthKm, thicknessKm});
      segment.setOrigin({0.0f, thicknessKm * 0.5f});
      segment.setPosition({from.xKm, from.yKm});
      const float angleRadians = std::atan2(to.yKm - from.yKm, to.xKm - from.xKm);
      const float angleDegrees = angleRadians * 180.0f / Pi;
      segment.setRotation(sf::degrees(angleDegrees));
      segment.setFillColor(color);
      _pWindow->draw(segment);
   }

   void Renderer::DrawLines(
      const World& world,
      LineId highlightLineId,
      LineDrag lineDrag,
      const LineDragPreview& lineDragPreview)
   {
      RebuildParallelOffsetCache(world);
      for (const Line& line : world.GetNetwork().GetLines())
      {
         const sf::Color color = ColorForLine(line.colorIndex);
         float thicknessPixels = LineThicknessPixels;
         if (line.id == highlightLineId)
         {
            thicknessPixels = SelectedLineThicknessPixels;
         }
         const float thicknessKm = PixelsToKm(thicknessPixels);
         const uint32_t segmentCount = LineSegmentCount(line);
         for (uint32_t segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
         {
            if (lineDrag == LineDrag::Yes &&
               line.id == lineDragPreview.lineId &&
               segmentIndex == lineDragPreview.segmentIndex)
            {
               continue;
            }

            StationId fromId = InvalidStationId;
            StationId toId = InvalidStationId;
            const Result endpointResult = LineSegmentEndpoints(line, segmentIndex, fromId, toId);
            if (IsErr(endpointResult))
            {
               continue;
            }

            const StationRecord* pFrom = world.GetNetwork().FindStation(fromId);
            const StationRecord* pTo = world.GetNetwork().FindStation(toId);
            if (pFrom == nullptr || pTo == nullptr)
            {
               continue;
            }

            const float offsetKm = ParallelOffsetKm(line.id, segmentIndex);

            const MapPoint fromPoint = OffsetAlongPerpendicular(
               pFrom->position,
               pFrom->position,
               pTo->position,
               offsetKm);
            const MapPoint toPoint = OffsetAlongPerpendicular(
               pTo->position,
               pFrom->position,
               pTo->position,
               offsetKm);
            DrawSegment(fromPoint, toPoint, color, thicknessKm);
         }
      }
   }

   void Renderer::DrawLineInsertPreview(
      const World& world,
      const LineDragPreview& lineDragPreview,
      sf::Vector2i cursorPixel)
   {
      const Line* pLine = world.GetNetwork().FindLine(lineDragPreview.lineId);
      if (pLine == nullptr)
      {
         return;
      }

      StationId fromId = InvalidStationId;
      StationId toId = InvalidStationId;
      const Result endpointResult = LineSegmentEndpoints(*pLine, lineDragPreview.segmentIndex, fromId, toId);
      if (IsErr(endpointResult))
      {
         return;
      }

      const StationRecord* pFrom = world.GetNetwork().FindStation(fromId);
      const StationRecord* pTo = world.GetNetwork().FindStation(toId);
      if (pFrom == nullptr || pTo == nullptr)
      {
         return;
      }

      MapPoint midPoint = ScreenToMap(cursorPixel);
      const StationRecord* pHover = world.GetNetwork().FindStation(lineDragPreview.hoverStationId);
      if (pHover != nullptr)
      {
         midPoint = pHover->position;
      }

      const sf::Color color = ColorForLine(pLine->colorIndex);
      const float thicknessKm = PixelsToKm(SelectedLineThicknessPixels);
      DrawSegment(pFrom->position, midPoint, color, thicknessKm);
      DrawSegment(midPoint, pTo->position, color, thicknessKm);
   }

   void Renderer::DrawTerminusAnchors(
      const World& world,
      LineId highlightLineId,
      AnchorDrag anchorDrag,
      const TerminusAnchorPreview& anchorDragPreview)
   {
      if (_pWindow == nullptr || highlightLineId == InvalidLineId)
      {
         return;
      }

      const Line* pLine = world.GetNetwork().FindLine(highlightLineId);
      if (pLine == nullptr || pLine->loop == LineLoop::Yes || pLine->stationIds.size() < MinimumLineStations)
      {
         return;
      }

      const sf::Color color = ColorForLine(pLine->colorIndex);
      const float offsetKm = PixelsToKm(TerminusAnchorOffsetPixels);
      const float radiusKm = PixelsToKm(TerminusAnchorRadiusPixels);
      const LineEnd ends[] = {LineEnd::Front, LineEnd::Back};

      for (LineEnd end : ends)
      {
         MapPoint anchorPoint;
         if (IsErr(world.GetTerminusAnchorPosition(highlightLineId, end, offsetKm, anchorPoint)))
         {
            continue;
         }

         float drawRadiusKm = radiusKm;
         if (anchorDrag == AnchorDrag::Yes &&
            anchorDragPreview.lineId == highlightLineId &&
            anchorDragPreview.end == end)
         {
            drawRadiusKm = PixelsToKm(TerminusAnchorDragRadiusPixels);
         }

         sf::CircleShape anchor(drawRadiusKm);
         anchor.setOrigin({drawRadiusKm, drawRadiusKm});
         anchor.setPosition({anchorPoint.xKm, anchorPoint.yKm});
         anchor.setFillColor(color);
         anchor.setOutlineColor(sf::Color(255, 255, 255, 220));
         anchor.setOutlineThickness(PixelsToKm(1.0f));
         _pWindow->draw(anchor);
      }
   }

   void Renderer::DrawTerminusExtendPreview(
      const World& world,
      const TerminusAnchorPreview& anchorDragPreview,
      sf::Vector2i cursorPixel)
   {
      const Line* pLine = world.GetNetwork().FindLine(anchorDragPreview.lineId);
      if (pLine == nullptr || pLine->loop == LineLoop::Yes || pLine->stationIds.size() < MinimumLineStations)
      {
         return;
      }

      StationId terminalId = InvalidStationId;
      if (anchorDragPreview.end == LineEnd::Front)
      {
         terminalId = pLine->stationIds.front();
      }
      else
      {
         terminalId = pLine->stationIds.back();
      }

      const StationRecord* pTerminal = world.GetNetwork().FindStation(terminalId);
      if (pTerminal == nullptr)
      {
         return;
      }

      MapPoint targetPoint = ScreenToMap(cursorPixel);
      const StationRecord* pHover = world.GetNetwork().FindStation(anchorDragPreview.hoverStationId);
      if (pHover != nullptr)
      {
         targetPoint = pHover->position;
      }

      const sf::Color color = ColorForLine(pLine->colorIndex);
      const float thicknessKm = PixelsToKm(SelectedLineThicknessPixels);
      DrawSegment(pTerminal->position, targetPoint, color, thicknessKm);
   }

   const StationRecord* Renderer::FindDraftStation(const World& world, StationId stationId) const
   {
      return world.GetNetwork().FindStation(stationId);
   }

   void Renderer::DrawDraft(const World& world, const StationIdList& draftStationIds)
   {
      if (draftStationIds.size() < 2)
      {
         return;
      }

      const sf::Color draftColor(40, 40, 40, 180);
      const float thicknessKm = PixelsToKm(DraftLineThicknessPixels);
      for (uint32_t index = 0; index + 1 < draftStationIds.size(); ++index)
      {
         const StationRecord* pFrom = FindDraftStation(world, draftStationIds[index]);
         const StationRecord* pTo = FindDraftStation(world, draftStationIds[index + 1]);
         if (pFrom == nullptr || pTo == nullptr)
         {
            continue;
         }

         DrawSegment(pFrom->position, pTo->position, draftColor, thicknessKm);
      }
   }

   void Renderer::DrawStations(const World& world, StationId hoveredStationId, StationId inspectedStationId)
   {
      if (_pWindow == nullptr || _pFont == nullptr)
      {
         return;
      }

      const StationRecordList& stations = world.GetNetwork().GetStations();
      const uint32_t stationCount = static_cast<uint32_t>(stations.size());
      for (const StationRecord& station : stations)
      {
         const float radiusKm = PixelsToKm(StationRadiusPixels(station.population));
         sf::CircleShape circle(radiusKm);
         circle.setOrigin({radiusKm, radiusKm});
         circle.setPosition({station.position.xKm, station.position.yKm});
         if (station.id == inspectedStationId)
         {
            circle.setFillColor(InspectedStationColor);
         }
         else if (station.id == hoveredStationId)
         {
            circle.setFillColor(sf::Color(80, 80, 80));
         }
         else
         {
            circle.setFillColor(sf::Color(45, 45, 45));
         }
         _pWindow->draw(circle);

         const uint32_t waitingCount = world.GetWaitingCountAt(station.id);
         const bool drawName = (stationCount <= 20) || (station.population >= 100000) ||
            (station.id == inspectedStationId);
         MapPoint labelPosition;
         labelPosition.xKm = station.position.xKm + radiusKm + PixelsToKm(2.0f);
         labelPosition.yKm = station.position.yKm - PixelsToKm(8.0f);
         if (drawName)
         {
            DrawScaledText(
               station.cityName,
               labelPosition,
               static_cast<unsigned int>(MapLabelCharacterPixels),
               sf::Color(30, 30, 30));
         }

         if (waitingCount > 0)
         {
            MapPoint waitingPosition;
            waitingPosition.xKm = station.position.xKm + radiusKm + PixelsToKm(2.0f);
            waitingPosition.yKm = station.position.yKm + PixelsToKm(4.0f);
            DrawScaledText(
               std::to_string(waitingCount),
               waitingPosition,
               static_cast<unsigned int>(WaitingLabelCharacterPixels),
               sf::Color(180, 40, 40));
         }
      }
   }

   void Renderer::DrawTrains(const World& world, TrainId inspectedTrainId)
   {
      if (_pWindow == nullptr)
      {
         return;
      }

      const float trainLengthKm = PixelsToKm(TrainLengthPixels);
      const float trainWidthKm = PixelsToKm(TrainWidthPixels);
      const float outlineKm = PixelsToKm(TrainOutlinePixels);
      const float inspectedOutlineKm = PixelsToKm(TrainOutlinePixels + 1.5f);
      for (const Train& train : world.GetTrains())
      {
         const Line* pLine = world.GetNetwork().FindLine(train.lineId);
         if (pLine == nullptr)
         {
            continue;
         }

         const MapPoint position = TrainMapPosition(train, *pLine, world.GetNetwork());
         const StationId fromId = CurrentStationOnLine(train, *pLine);
         StationId toId = NextStationOnLine(train, *pLine);
         if (toId == InvalidStationId)
         {
            toId = fromId;
         }

         const StationRecord* pFrom = world.GetNetwork().FindStation(fromId);
         const StationRecord* pTo = world.GetNetwork().FindStation(toId);
         float angleDegrees = 0.0f;
         if (pFrom != nullptr && pTo != nullptr)
         {
            const float angleRadians = std::atan2(
               pTo->position.yKm - pFrom->position.yKm,
               pTo->position.xKm - pFrom->position.xKm);
            angleDegrees = angleRadians * 180.0f / Pi;
         }

         sf::RectangleShape trainShape({trainLengthKm, trainWidthKm});
         trainShape.setOrigin({trainLengthKm * 0.5f, trainWidthKm * 0.5f});
         trainShape.setPosition({position.xKm, position.yKm});
         trainShape.setRotation(sf::degrees(angleDegrees));
         trainShape.setFillColor(ColorForLine(pLine->colorIndex));
         if (train.id == inspectedTrainId)
         {
            trainShape.setOutlineColor(InspectedTrainOutlineColor);
            trainShape.setOutlineThickness(inspectedOutlineKm);
         }
         else
         {
            trainShape.setOutlineColor(sf::Color(20, 20, 20));
            trainShape.setOutlineThickness(outlineKm);
         }
         _pWindow->draw(trainShape);
      }
   }

   void Renderer::DrawHud(std::string_view statusText)
   {
      if (_pWindow == nullptr || _pFont == nullptr)
      {
         return;
      }

      ApplyDefaultView();
      sf::Text text(*_pFont, Utf8SfString(statusText), 16);
      text.setFillColor(sf::Color(25, 25, 25));
      text.setPosition({12.0f, 10.0f});
      _pWindow->draw(text);
   }

   float Renderer::BottomHudBarTop(void) const
   {
      if (_pWindow == nullptr)
      {
         return 0.0f;
      }

      const float windowHeight = static_cast<float>(_pWindow->getSize().y);
      return windowHeight - HudButtonMarginPixels - HudButtonSizePixels;
   }

   float Renderer::BottomHudControlLeft(uint32_t controlIndex) const
   {
      float left = HudButtonMarginPixels;
      for (uint32_t index = 0; index < controlIndex; ++index)
      {
         left += BottomHudControlWidth(static_cast<BottomHudControl>(index));
         left += HudControlGapPixels;
      }

      return left;
   }

   sf::FloatRect Renderer::HelpButtonBounds(void) const
   {
      if (_pWindow == nullptr)
      {
         return sf::FloatRect({0.0f, 0.0f}, {0.0f, 0.0f});
      }

      return sf::FloatRect(
         {BottomHudControlLeft(static_cast<uint32_t>(BottomHudControl::Help)), BottomHudBarTop()},
         {BottomHudControlWidth(BottomHudControl::Help), HudButtonSizePixels});
   }

   sf::FloatRect Renderer::TrainTokenBounds(void) const
   {
      if (_pWindow == nullptr)
      {
         return sf::FloatRect({0.0f, 0.0f}, {0.0f, 0.0f});
      }

      return sf::FloatRect(
         {BottomHudControlLeft(static_cast<uint32_t>(BottomHudControl::Train)), BottomHudBarTop()},
         {BottomHudControlWidth(BottomHudControl::Train), HudButtonSizePixels});
   }

   sf::FloatRect Renderer::SlowDownButtonBounds(void) const
   {
      if (_pWindow == nullptr)
      {
         return sf::FloatRect({0.0f, 0.0f}, {0.0f, 0.0f});
      }

      return sf::FloatRect(
         {BottomHudControlLeft(static_cast<uint32_t>(BottomHudControl::SlowDown)), BottomHudBarTop()},
         {BottomHudControlWidth(BottomHudControl::SlowDown), HudButtonSizePixels});
   }

   sf::FloatRect Renderer::SpeedLabelBounds(void) const
   {
      if (_pWindow == nullptr)
      {
         return sf::FloatRect({0.0f, 0.0f}, {0.0f, 0.0f});
      }

      return sf::FloatRect(
         {BottomHudControlLeft(static_cast<uint32_t>(BottomHudControl::SpeedLabel)), BottomHudBarTop()},
         {BottomHudControlWidth(BottomHudControl::SpeedLabel), HudButtonSizePixels});
   }

   sf::FloatRect Renderer::SpeedUpButtonBounds(void) const
   {
      if (_pWindow == nullptr)
      {
         return sf::FloatRect({0.0f, 0.0f}, {0.0f, 0.0f});
      }

      return sf::FloatRect(
         {BottomHudControlLeft(static_cast<uint32_t>(BottomHudControl::SpeedUp)), BottomHudBarTop()},
         {BottomHudControlWidth(BottomHudControl::SpeedUp), HudButtonSizePixels});
   }

   sf::FloatRect Renderer::PauseButtonBounds(void) const
   {
      if (_pWindow == nullptr)
      {
         return sf::FloatRect({0.0f, 0.0f}, {0.0f, 0.0f});
      }

      return sf::FloatRect(
         {BottomHudControlLeft(static_cast<uint32_t>(BottomHudControl::Pause)), BottomHudBarTop()},
         {BottomHudControlWidth(BottomHudControl::Pause), HudButtonSizePixels});
   }

   sf::FloatRect Renderer::ResumeButtonBounds(void) const
   {
      if (_pWindow == nullptr)
      {
         return sf::FloatRect({0.0f, 0.0f}, {0.0f, 0.0f});
      }

      return sf::FloatRect(
         {BottomHudControlLeft(static_cast<uint32_t>(BottomHudControl::Resume)), BottomHudBarTop()},
         {BottomHudControlWidth(BottomHudControl::Resume), HudButtonSizePixels});
   }

   sf::FloatRect Renderer::MenuButtonBounds(void) const
   {
      if (_pWindow == nullptr)
      {
         return sf::FloatRect({0.0f, 0.0f}, {0.0f, 0.0f});
      }

      return sf::FloatRect(
         {BottomHudControlLeft(static_cast<uint32_t>(BottomHudControl::Menu)), BottomHudBarTop()},
         {BottomHudControlWidth(BottomHudControl::Menu), HudButtonSizePixels});
   }

   bool Renderer::ContainsPixel(const sf::FloatRect& bounds, sf::Vector2i pixel) const
   {
      const sf::Vector2f point(static_cast<float>(pixel.x), static_cast<float>(pixel.y));
      return bounds.contains(point);
   }

   bool Renderer::IsHelpButtonHit(sf::Vector2i pixel) const
   {
      return ContainsPixel(HelpButtonBounds(), pixel);
   }

   bool Renderer::IsTrainTokenHit(sf::Vector2i pixel) const
   {
      return ContainsPixel(TrainTokenBounds(), pixel);
   }

   bool Renderer::IsSlowDownButtonHit(sf::Vector2i pixel) const
   {
      return ContainsPixel(SlowDownButtonBounds(), pixel);
   }

   bool Renderer::IsSpeedUpButtonHit(sf::Vector2i pixel) const
   {
      return ContainsPixel(SpeedUpButtonBounds(), pixel);
   }

   bool Renderer::IsPauseButtonHit(sf::Vector2i pixel) const
   {
      return ContainsPixel(PauseButtonBounds(), pixel);
   }

   bool Renderer::IsResumeButtonHit(sf::Vector2i pixel) const
   {
      return ContainsPixel(ResumeButtonBounds(), pixel);
   }

   bool Renderer::IsMenuButtonHit(sf::Vector2i pixel) const
   {
      return ContainsPixel(MenuButtonBounds(), pixel);
   }

   void Renderer::DrawHelpButton(HelpVisible helpVisible)
   {
      if (_pWindow == nullptr || _pFont == nullptr)
      {
         return;
      }

      const sf::FloatRect bounds = HelpButtonBounds();
      if (helpVisible == HelpVisible::Yes)
      {
         DrawHudButtonFrame(*_pWindow, bounds, sf::Color(45, 45, 45));
      }
      else
      {
         DrawHudButtonFrame(*_pWindow, bounds, sf::Color(70, 70, 70));
      }
      DrawCenteredHudLabel(*_pWindow, *_pFont, bounds, "?", 20, sf::Color(250, 250, 250));
   }

   void Renderer::DrawTrainToken(TrainDrag trainDrag)
   {
      if (_pWindow == nullptr || _pFont == nullptr)
      {
         return;
      }

      const sf::FloatRect bounds = TrainTokenBounds();
      if (trainDrag == TrainDrag::Yes)
      {
         DrawHudButtonFrame(*_pWindow, bounds, sf::Color(200, 200, 200, 120));
      }
      else
      {
         DrawHudButtonFrame(*_pWindow, bounds, sf::Color(50, 50, 50));
      }

      sf::RectangleShape trainShape({28.0f, 12.0f});
      trainShape.setOrigin({14.0f, 6.0f});
      trainShape.setPosition({
         bounds.position.x + (bounds.size.x * 0.5f),
         bounds.position.y + (bounds.size.y * 0.5f)
      });
      trainShape.setFillColor(sf::Color(220, 50, 47));
      _pWindow->draw(trainShape);
   }

   void Renderer::DrawPlaybackControls(SimulationPause pause, float timeScale)
   {
      if (_pWindow == nullptr || _pFont == nullptr)
      {
         return;
      }

      DrawHudButtonFrame(*_pWindow, SlowDownButtonBounds(), sf::Color(70, 70, 70));
      DrawCenteredHudLabel(
         *_pWindow,
         *_pFont,
         SlowDownButtonBounds(),
         "<",
         20,
         sf::Color(250, 250, 250));

      const sf::FloatRect speedBounds = SpeedLabelBounds();
      DrawHudButtonFrame(*_pWindow, speedBounds, sf::Color(236, 230, 218));
      std::string speedLabel = std::to_string(static_cast<int32_t>(timeScale));
      speedLabel += "x";
      DrawCenteredHudLabel(
         *_pWindow,
         *_pFont,
         speedBounds,
         speedLabel,
         14,
         sf::Color(35, 35, 35));

      DrawHudButtonFrame(*_pWindow, SpeedUpButtonBounds(), sf::Color(70, 70, 70));
      DrawCenteredHudLabel(
         *_pWindow,
         *_pFont,
         SpeedUpButtonBounds(),
         ">",
         20,
         sf::Color(250, 250, 250));

      if (pause == SimulationPause::Yes)
      {
         DrawHudButtonFrame(*_pWindow, PauseButtonBounds(), sf::Color(90, 90, 90));
         DrawHudButtonFrame(*_pWindow, ResumeButtonBounds(), sf::Color(45, 45, 45));
      }
      else
      {
         DrawHudButtonFrame(*_pWindow, PauseButtonBounds(), sf::Color(45, 45, 45));
         DrawHudButtonFrame(*_pWindow, ResumeButtonBounds(), sf::Color(90, 90, 90));
      }
      DrawCenteredHudLabel(
         *_pWindow,
         *_pFont,
         PauseButtonBounds(),
         "Pause",
         14,
         sf::Color(250, 250, 250));
      DrawCenteredHudLabel(
         *_pWindow,
         *_pFont,
         ResumeButtonBounds(),
         "Resume",
         14,
         sf::Color(250, 250, 250));

      DrawHudButtonFrame(*_pWindow, MenuButtonBounds(), sf::Color(70, 70, 70));
      DrawCenteredHudLabel(
         *_pWindow,
         *_pFont,
         MenuButtonBounds(),
         "Menu",
         14,
         sf::Color(250, 250, 250));
   }

   void Renderer::DrawHelpPopup(void)
   {
      if (_pWindow == nullptr || _pFont == nullptr)
      {
         return;
      }

      const sf::FloatRect helpButton = HelpButtonBounds();
      const float panelWidth = 340.0f;
      const float panelHeight = 284.0f;
      const float panelLeft = HudButtonMarginPixels;
      const float panelTop = helpButton.position.y - panelHeight - 8.0f;
      sf::RectangleShape panel({panelWidth, panelHeight});
      panel.setPosition({panelLeft, panelTop});
      panel.setFillColor(sf::Color(255, 252, 245, 240));
      panel.setOutlineColor(sf::Color(120, 110, 100));
      panel.setOutlineThickness(1.0f);
      _pWindow->draw(panel);

      const char helpText[] =
         "Click stations to draft a line.\n"
         "Click the first station again to close a loop.\n"
         "Drag a line onto a station to insert it.\n"
         "Drag a terminus anchor to extend a selected line.\n"
         "Enter or right-click to finish a line.\n"
         "Drag the train onto a line to place it there.\n"
         "Click a train, station, or line for details on the right.\n"
         "Bottom bar: speed, pause, resume, menu.\n"
         "Del deletes the selected line.\n"
         "Ctrl+Z / Ctrl+Y undo or redo a draft click.\n"
         "Wheel zoom, middle-drag pan. F11 fullscreen.\n"
         "Arrows pan. + / - zoom.\n"
         "[ / ] change station cap by 50.\n"
         "Space pause. 1 / 2 / 4 / 8 speed.\n"
         "Esc returns to the menu.";
      sf::Text text(*_pFont, Utf8SfString(helpText), 14);
      text.setFillColor(sf::Color(35, 35, 35));
      text.setPosition({panelLeft + 12.0f, panelTop + 10.0f});
      _pWindow->draw(text);
   }

   void Renderer::DrawDraggedTrain(sf::Vector2i cursorPixel)
   {
      if (_pWindow == nullptr)
      {
         return;
      }

      const MapPoint mapPoint = ScreenToMap(cursorPixel);
      const float trainLengthKm = PixelsToKm(TrainLengthPixels);
      const float trainWidthKm = PixelsToKm(TrainWidthPixels);
      sf::RectangleShape trainShape({trainLengthKm, trainWidthKm});
      trainShape.setOrigin({trainLengthKm * 0.5f, trainWidthKm * 0.5f});
      trainShape.setPosition({mapPoint.xKm, mapPoint.yKm});
      trainShape.setFillColor(sf::Color(50, 50, 50, 200));
      trainShape.setOutlineColor(sf::Color(20, 20, 20));
      trainShape.setOutlineThickness(PixelsToKm(TrainOutlinePixels));
      _pWindow->draw(trainShape);
   }

   float Renderer::ComputeInspectorHeightPixels(
      const World& world,
      StationId inspectedStationId,
      TrainId inspectedTrainId,
      LineId inspectedLineId,
      const SidebarSnapshot& snapshot) const
   {
      if (_pWindow == nullptr)
      {
         return 72.0f;
      }

      const float windowHeight = static_cast<float>(_pWindow->getSize().y);
      float height = 72.0f;
      if (inspectedStationId != InvalidStationId)
      {
         const uint32_t rowCount = ClampedInspectorRowCount(static_cast<uint32_t>(snapshot.stationDemand.size()));
         height = 86.0f + (static_cast<float>(rowCount) * 20.0f);
      }
      else if (inspectedTrainId != InvalidTrainId)
      {
         const uint32_t rowCount = ClampedInspectorRowCount(static_cast<uint32_t>(snapshot.onboardDemand.size()));
         height = 108.0f + (static_cast<float>(rowCount) * 36.0f);
      }
      else if (inspectedLineId != InvalidLineId)
      {
         const uint32_t trainRows = ClampedInspectorRowCount(static_cast<uint32_t>(snapshot.lineOccupancy.size()));
         const uint32_t destRows = ClampedInspectorRowCount(static_cast<uint32_t>(snapshot.lineDemand.size()));
         height = 86.0f + (static_cast<float>(trainRows) * 20.0f) + 24.0f +
            (static_cast<float>(destRows) * 20.0f);
      }
      else
      {
         const uint32_t destRows = ClampedInspectorRowCount(static_cast<uint32_t>(snapshot.globalDemand.size()));
         const uint32_t crowdedRows = ClampedCrowdedStationRowCount(static_cast<uint32_t>(snapshot.crowdedStations.size()));
         height = 86.0f + (static_cast<float>(destRows) * 20.0f) + 24.0f +
            (static_cast<float>(crowdedRows) * 20.0f);
      }

      const float maximumHeight = windowHeight * 0.45f;
      if (height > maximumHeight)
      {
         height = maximumHeight;
      }
      if (height < 72.0f)
      {
         height = 72.0f;
      }

      return height;
   }

   SidebarSnapshot Renderer::BuildSidebarSnapshot(
      const World& world,
      StationId inspectedStationId,
      TrainId inspectedTrainId,
      LineId inspectedLineId) const
   {
      SidebarSnapshot snapshot;
      world.CollectUnconnectedStations(snapshot.unconnectedIds);

      if (inspectedStationId != InvalidStationId)
      {
         world.CollectWaitingDemand(inspectedStationId, snapshot.stationDemand);
      }
      else if (inspectedTrainId != InvalidTrainId)
      {
         world.CollectOnboardDemand(inspectedTrainId, snapshot.onboardDemand);
      }
      else if (inspectedLineId != InvalidLineId)
      {
         world.CollectTrainsOnLine(inspectedLineId, snapshot.lineOccupancy);
         world.CollectLineDemand(inspectedLineId, snapshot.lineDemand);
      }
      else
      {
         world.CollectGlobalWaitingDemand(snapshot.globalDemand);
         world.CollectCrowdedStations(snapshot.crowdedStations);
      }

      snapshot.inspectorHeightPixels = ComputeInspectorHeightPixels(
         world,
         inspectedStationId,
         inspectedTrainId,
         inspectedLineId,
         snapshot);
      snapshot.unconnectedListTopPixels = snapshot.inspectorHeightPixels + 8.0f;
      return snapshot;
   }

   float Renderer::InspectorHeightPixels(
      const World& world,
      StationId inspectedStationId,
      TrainId inspectedTrainId,
      LineId inspectedLineId) const
   {
      const SidebarSnapshot snapshot = BuildSidebarSnapshot(
         world,
         inspectedStationId,
         inspectedTrainId,
         inspectedLineId);
      return snapshot.inspectorHeightPixels;
   }

   float Renderer::UnconnectedListTopPixels(
      const World& world,
      StationId inspectedStationId,
      TrainId inspectedTrainId,
      LineId inspectedLineId) const
   {
      const SidebarSnapshot snapshot = BuildSidebarSnapshot(
         world,
         inspectedStationId,
         inspectedTrainId,
         inspectedLineId);
      return snapshot.unconnectedListTopPixels;
   }

   sf::FloatRect Renderer::UnconnectedPanelBounds(void) const
   {
      if (_pWindow == nullptr || _mapSidebar == MapSidebar::Hidden)
      {
         return sf::FloatRect({0.0f, 0.0f}, {0.0f, 0.0f});
      }

      const float windowWidth = static_cast<float>(_pWindow->getSize().x);
      const float windowHeight = static_cast<float>(_pWindow->getSize().y);
      const float panelWidth = SidebarWidthPixels();
      return sf::FloatRect({windowWidth - panelWidth, 0.0f}, {panelWidth, windowHeight});
   }

   bool Renderer::IsPixelOnMap(sf::Vector2i pixel) const
   {
      if (_pWindow == nullptr)
      {
         return false;
      }

      if (pixel.x < 0 || pixel.y < 0)
      {
         return false;
      }

      return static_cast<float>(pixel.x) < MapWidthPixels();
   }

   bool Renderer::IsUnconnectedPanelHit(sf::Vector2i pixel) const
   {
      return ContainsPixel(UnconnectedPanelBounds(), pixel);
   }

   float Renderer::ClampUnconnectedScroll(
      const World& world,
      StationId inspectedStationId,
      TrainId inspectedTrainId,
      LineId inspectedLineId,
      float unconnectedScrollPixels) const
   {
      if (_pWindow == nullptr)
      {
         return 0.0f;
      }

      const SidebarSnapshot snapshot = BuildSidebarSnapshot(
         world,
         inspectedStationId,
         inspectedTrainId,
         inspectedLineId);
      const sf::FloatRect bounds = UnconnectedPanelBounds();
      const float listTop = snapshot.unconnectedListTopPixels;
      const float rowsTop = listTop + 24.0f;
      const float listHeight = bounds.size.y - rowsTop - 12.0f;
      const float contentHeight = static_cast<float>(snapshot.unconnectedIds.size()) * UnconnectedRowHeightPixels;
      float maxScroll = contentHeight - listHeight;
      if (maxScroll < 0.0f)
      {
         maxScroll = 0.0f;
      }

      float scroll = unconnectedScrollPixels;
      if (scroll < 0.0f)
      {
         scroll = 0.0f;
      }
      if (scroll > maxScroll)
      {
         scroll = maxScroll;
      }

      return scroll;
   }

   StationId Renderer::HitTestUnconnectedRow(
      const World& world,
      StationId inspectedStationId,
      TrainId inspectedTrainId,
      LineId inspectedLineId,
      sf::Vector2i pixel,
      float unconnectedScrollPixels) const
   {
      if (!IsUnconnectedPanelHit(pixel))
      {
         return InvalidStationId;
      }

      const SidebarSnapshot snapshot = BuildSidebarSnapshot(
         world,
         inspectedStationId,
         inspectedTrainId,
         inspectedLineId);
      const sf::FloatRect bounds = UnconnectedPanelBounds();
      const float listTop = snapshot.unconnectedListTopPixels;
      const float rowsTop = listTop + 24.0f;
      const float localY =
         static_cast<float>(pixel.y) - bounds.position.y - rowsTop + unconnectedScrollPixels;
      if (localY < 0.0f)
      {
         return InvalidStationId;
      }

      const auto rowIndex = static_cast<uint32_t>(localY / UnconnectedRowHeightPixels);
      if (rowIndex >= snapshot.unconnectedIds.size())
      {
         return InvalidStationId;
      }

      return snapshot.unconnectedIds[rowIndex];
   }

   void Renderer::DrawSidebarInspector(
      const World& world,
      StationId inspectedStationId,
      TrainId inspectedTrainId,
      LineId inspectedLineId,
      const sf::FloatRect& panelBounds,
      const SidebarSnapshot& snapshot)
   {
      const float inspectorHeight = snapshot.inspectorHeightPixels;
      sf::RectangleShape inspector({panelBounds.size.x, inspectorHeight});
      inspector.setPosition(panelBounds.position);
      inspector.setFillColor(sf::Color(255, 252, 245));
      _pWindow->draw(inspector);

      const float left = panelBounds.position.x + 12.0f;
      if (inspectedStationId != InvalidStationId)
      {
         const StationRecord* pStation = world.GetNetwork().FindStation(inspectedStationId);
         std::string title = "Station";
         if (pStation != nullptr)
         {
            title = pStation->cityName;
         }
         sf::Text titleText(*_pFont, Utf8SfString(title), 18);
         titleText.setFillColor(sf::Color(25, 25, 25));
         titleText.setPosition({left, 10.0f});
         _pWindow->draw(titleText);

         const uint32_t waitingTotal = world.GetWaitingCountAt(inspectedStationId);
         std::string waitingLine = "Waiting " + std::to_string(waitingTotal);
         sf::Text waitingText(*_pFont, Utf8SfString(waitingLine), 14);
         waitingText.setFillColor(sf::Color(180, 40, 40));
         waitingText.setPosition({left, 36.0f});
         _pWindow->draw(waitingText);

         DestinationDemandList demand = snapshot.stationDemand;
         if (demand.empty())
         {
            sf::Text emptyText(*_pFont, Utf8SfString("No destinations yet"), 13);
            emptyText.setFillColor(sf::Color(90, 85, 80));
            emptyText.setPosition({left, 60.0f});
            _pWindow->draw(emptyText);
            return;
         }

         uint32_t shownRows = 0;
         for (const DestinationDemand& entry : demand)
         {
            if (shownRows >= InspectorMaxRows)
            {
               break;
            }

            const float rowY = 60.0f + (static_cast<float>(shownRows) * 20.0f);
            if (rowY + 18.0f > inspectorHeight)
            {
               break;
            }

            std::string rowLine = StationCityName(world, entry.destinationId);
            rowLine += "  x";
            rowLine += std::to_string(entry.waitingCount);
            sf::Text rowText(*_pFont, Utf8SfString(rowLine), 13);
            rowText.setFillColor(sf::Color(40, 40, 40));
            rowText.setPosition({left, rowY});
            _pWindow->draw(rowText);
            ++shownRows;
         }
         return;
      }

      if (inspectedTrainId != InvalidTrainId)
      {
         const Train* pTrain = world.FindTrain(inspectedTrainId);
         sf::Text titleText(*_pFont, Utf8SfString("Train"), 18);
         titleText.setFillColor(sf::Color(25, 25, 25));
         titleText.setPosition({left, 10.0f});
         _pWindow->draw(titleText);

         uint32_t onboardCount = 0;
         if (pTrain != nullptr)
         {
            onboardCount = static_cast<uint32_t>(pTrain->passengerIds.size());
         }
         std::string onboardLine = "Onboard ";
         onboardLine += std::to_string(onboardCount);
         onboardLine += " / ";
         onboardLine += std::to_string(world.GetTrainCapacity());
         sf::Text onboardText(*_pFont, Utf8SfString(onboardLine), 14);
         onboardText.setFillColor(sf::Color(35, 35, 35));
         onboardText.setPosition({left, 36.0f});
         _pWindow->draw(onboardText);

         std::string nextLine = "Next  -";
         if (pTrain != nullptr)
         {
            const Line* pLine = world.GetNetwork().FindLine(pTrain->lineId);
            if (pLine != nullptr)
            {
               const StationId nextId = NextStationOnLine(*pTrain, *pLine);
               if (nextId != InvalidStationId)
               {
                  nextLine = "Next  " + StationCityName(world, nextId);
               }
            }
         }
         sf::Text nextText(*_pFont, Utf8SfString(nextLine), 13);
         nextText.setFillColor(sf::Color(80, 75, 70));
         nextText.setPosition({left, 56.0f});
         _pWindow->draw(nextText);

         OnboardDemandList demand = snapshot.onboardDemand;
         if (demand.empty())
         {
            sf::Text emptyText(*_pFont, Utf8SfString("No passengers onboard"), 13);
            emptyText.setFillColor(sf::Color(90, 85, 80));
            emptyText.setPosition({left, 80.0f});
            _pWindow->draw(emptyText);
            return;
         }

         uint32_t shownRows = 0;
         for (const OnboardDemand& entry : demand)
         {
            if (shownRows >= InspectorMaxRows)
            {
               break;
            }

            const float rowY = 80.0f + (static_cast<float>(shownRows) * 36.0f);
            if (rowY + 32.0f > inspectorHeight)
            {
               break;
            }

            std::string destLine = StationCityName(world, entry.destinationId);
            destLine += "  x";
            destLine += std::to_string(entry.passengerCount);
            sf::Text destText(*_pFont, Utf8SfString(destLine), 13);
            destText.setFillColor(sf::Color(40, 40, 40));
            destText.setPosition({left, rowY});
            _pWindow->draw(destText);

            std::string transferLine = "direct";
            if (entry.transferStationId != InvalidStationId)
            {
               transferLine = "transfer at ";
               transferLine += StationCityName(world, entry.transferStationId);
            }
            sf::Text transferText(*_pFont, Utf8SfString(transferLine), 12);
            transferText.setFillColor(sf::Color(110, 70, 40));
            transferText.setPosition({left, rowY + 16.0f});
            _pWindow->draw(transferText);
            ++shownRows;
         }
         return;
      }

      if (inspectedLineId != InvalidLineId)
      {
         const Line* pLine = world.GetNetwork().FindLine(inspectedLineId);
         std::string title = "Line";
         if (pLine != nullptr)
         {
            title = LineInspectorTitle(world, *pLine);
         }
         sf::Text titleText(*_pFont, Utf8SfString(title), 18);
         titleText.setFillColor(sf::Color(25, 25, 25));
         titleText.setPosition({left, 10.0f});
         _pWindow->draw(titleText);

         TrainOccupancyList occupancy = snapshot.lineOccupancy;
         std::string trainsLine = "Trains " + std::to_string(occupancy.size());
         sf::Text trainsText(*_pFont, Utf8SfString(trainsLine), 14);
         trainsText.setFillColor(sf::Color(35, 35, 35));
         trainsText.setPosition({left, 36.0f});
         _pWindow->draw(trainsText);

         const uint32_t trainRowSlots = ClampedInspectorRowCount(static_cast<uint32_t>(occupancy.size()));
         if (occupancy.empty())
         {
            sf::Text emptyTrainsText(*_pFont, Utf8SfString("No trains"), 13);
            emptyTrainsText.setFillColor(sf::Color(90, 85, 80));
            emptyTrainsText.setPosition({left, 56.0f});
            _pWindow->draw(emptyTrainsText);
         }
         else
         {
            uint32_t shownTrainRows = 0;
            for (const TrainOccupancy& entry : occupancy)
            {
               if (shownTrainRows >= InspectorMaxRows)
               {
                  break;
               }

               const float rowY = 56.0f + (static_cast<float>(shownTrainRows) * 20.0f);
               if (rowY + 18.0f > inspectorHeight)
               {
                  break;
               }

               std::string rowLine = std::to_string(entry.passengerCount);
               rowLine += " / ";
               rowLine += std::to_string(world.GetTrainCapacity());
               if (entry.nextStationId != InvalidStationId)
               {
                  rowLine += "  ";
                  rowLine += StationCityName(world, entry.nextStationId);
               }
               sf::Text rowText(*_pFont, Utf8SfString(rowLine), 13);
               rowText.setFillColor(sf::Color(40, 40, 40));
               rowText.setPosition({left, rowY});
               _pWindow->draw(rowText);
               ++shownTrainRows;
            }
         }

         const float destHeaderY = 56.0f + (static_cast<float>(trainRowSlots) * 20.0f) + 4.0f;
         sf::Text destHeaderText(*_pFont, Utf8SfString("Destinations"), 14);
         destHeaderText.setFillColor(sf::Color(35, 35, 35));
         destHeaderText.setPosition({left, destHeaderY});
         _pWindow->draw(destHeaderText);

         DestinationDemandList demand = snapshot.lineDemand;
         if (demand.empty())
         {
            sf::Text emptyDestText(*_pFont, Utf8SfString("No destinations yet"), 13);
            emptyDestText.setFillColor(sf::Color(90, 85, 80));
            emptyDestText.setPosition({left, destHeaderY + 20.0f});
            _pWindow->draw(emptyDestText);
            return;
         }

         uint32_t shownDestRows = 0;
         for (const DestinationDemand& entry : demand)
         {
            if (shownDestRows >= InspectorMaxRows)
            {
               break;
            }

            const float rowY = destHeaderY + 20.0f + (static_cast<float>(shownDestRows) * 20.0f);
            if (rowY + 18.0f > inspectorHeight)
            {
               break;
            }

            std::string rowLine = StationCityName(world, entry.destinationId);
            rowLine += "  x";
            rowLine += std::to_string(entry.waitingCount);
            sf::Text rowText(*_pFont, Utf8SfString(rowLine), 13);
            rowText.setFillColor(sf::Color(40, 40, 40));
            rowText.setPosition({left, rowY});
            _pWindow->draw(rowText);
            ++shownDestRows;
         }
         return;
      }

      DestinationDemandList demand = snapshot.globalDemand;
      StationCrowdingList crowded = snapshot.crowdedStations;

      sf::Text titleText(*_pFont, Utf8SfString("Overview"), 18);
      titleText.setFillColor(sf::Color(25, 25, 25));
      titleText.setPosition({left, 10.0f});
      _pWindow->draw(titleText);

      std::string waitingLine = "Waiting " + std::to_string(world.GetWaitingPassengerCount());
      sf::Text waitingText(*_pFont, Utf8SfString(waitingLine), 14);
      waitingText.setFillColor(sf::Color(180, 40, 40));
      waitingText.setPosition({left, 36.0f});
      _pWindow->draw(waitingText);

      const uint32_t destRowSlots = ClampedInspectorRowCount(static_cast<uint32_t>(demand.size()));
      if (demand.empty())
      {
         sf::Text emptyDestText(*_pFont, Utf8SfString("No destinations yet"), 13);
         emptyDestText.setFillColor(sf::Color(90, 85, 80));
         emptyDestText.setPosition({left, 56.0f});
         _pWindow->draw(emptyDestText);
      }
      else
      {
         uint32_t shownDestRows = 0;
         for (const DestinationDemand& entry : demand)
         {
            if (shownDestRows >= InspectorMaxRows)
            {
               break;
            }

            const float rowY = 56.0f + (static_cast<float>(shownDestRows) * 20.0f);
            if (rowY + 18.0f > inspectorHeight)
            {
               break;
            }

            std::string rowLine = StationCityName(world, entry.destinationId);
            rowLine += "  x";
            rowLine += std::to_string(entry.waitingCount);
            sf::Text rowText(*_pFont, Utf8SfString(rowLine), 13);
            rowText.setFillColor(sf::Color(40, 40, 40));
            rowText.setPosition({left, rowY});
            _pWindow->draw(rowText);
            ++shownDestRows;
         }
      }

      const float crowdedHeaderY = 56.0f + (static_cast<float>(destRowSlots) * 20.0f) + 4.0f;
      sf::Text crowdedHeaderText(*_pFont, Utf8SfString("Crowded stations"), 14);
      crowdedHeaderText.setFillColor(sf::Color(35, 35, 35));
      crowdedHeaderText.setPosition({left, crowdedHeaderY});
      _pWindow->draw(crowdedHeaderText);

      if (crowded.empty())
      {
         sf::Text emptyCrowdedText(*_pFont, Utf8SfString("No crowded stations"), 13);
         emptyCrowdedText.setFillColor(sf::Color(90, 85, 80));
         emptyCrowdedText.setPosition({left, crowdedHeaderY + 20.0f});
         _pWindow->draw(emptyCrowdedText);
         return;
      }

      uint32_t shownCrowdedRows = 0;
      for (const StationCrowding& entry : crowded)
      {
         if (shownCrowdedRows >= CrowdedStationMaxRows)
         {
            break;
         }

         const float rowY = crowdedHeaderY + 20.0f + (static_cast<float>(shownCrowdedRows) * 20.0f);
         if (rowY + 18.0f > inspectorHeight)
         {
            break;
         }

         std::string rowLine = StationCityName(world, entry.stationId);
         rowLine += "  x";
         rowLine += std::to_string(entry.waitingCount);
         sf::Text rowText(*_pFont, Utf8SfString(rowLine), 13);
         rowText.setFillColor(sf::Color(40, 40, 40));
         rowText.setPosition({left, rowY});
         _pWindow->draw(rowText);
         ++shownCrowdedRows;
      }
   }

   void Renderer::DrawSidebar(
      const World& world,
      StationId inspectedStationId,
      TrainId inspectedTrainId,
      LineId inspectedLineId,
      float unconnectedScrollPixels,
      sf::Vector2i cursorPixel)
   {
      if (_pWindow == nullptr || _pFont == nullptr || _mapSidebar == MapSidebar::Hidden)
      {
         return;
      }

      ApplyDefaultView();
      const sf::FloatRect bounds = UnconnectedPanelBounds();
      sf::RectangleShape panel(bounds.size);
      panel.setPosition(bounds.position);
      panel.setFillColor(sf::Color(248, 244, 236));
      panel.setOutlineColor(sf::Color(170, 160, 148));
      panel.setOutlineThickness(1.0f);
      _pWindow->draw(panel);

      const SidebarSnapshot snapshot = BuildSidebarSnapshot(
         world,
         inspectedStationId,
         inspectedTrainId,
         inspectedLineId);
      DrawSidebarInspector(
         world,
         inspectedStationId,
         inspectedTrainId,
         inspectedLineId,
         bounds,
         snapshot);

      const float listTop = snapshot.unconnectedListTopPixels;
      sf::RectangleShape divider({bounds.size.x - 16.0f, 1.0f});
      divider.setPosition({bounds.position.x + 8.0f, listTop - 6.0f});
      divider.setFillColor(sf::Color(190, 180, 168));
      _pWindow->draw(divider);

      std::string title = "Unconnected  " + std::to_string(snapshot.unconnectedIds.size());
      sf::Text titleText(*_pFont, Utf8SfString(title), 15);
      titleText.setFillColor(sf::Color(35, 35, 35));
      titleText.setPosition({bounds.position.x + 12.0f, listTop});
      _pWindow->draw(titleText);

      const float rowsTop = listTop + 24.0f;
      if (snapshot.unconnectedIds.empty())
      {
         sf::Text emptyText(*_pFont, Utf8SfString("All stations connected"), 13);
         emptyText.setFillColor(sf::Color(90, 85, 80));
         emptyText.setPosition({bounds.position.x + 12.0f, rowsTop});
         _pWindow->draw(emptyText);
         return;
      }

      const float listBottom = bounds.size.y - 8.0f;
      const StationId hoveredRowId = HitTestUnconnectedRow(
         world,
         inspectedStationId,
         inspectedTrainId,
         inspectedLineId,
         cursorPixel,
         unconnectedScrollPixels);
      for (uint32_t index = 0; index < snapshot.unconnectedIds.size(); ++index)
      {
         const float rowY = rowsTop + (static_cast<float>(index) * UnconnectedRowHeightPixels) - unconnectedScrollPixels;
         if ((rowY + UnconnectedRowHeightPixels) < rowsTop || rowY > listBottom)
         {
            continue;
         }

         const StationRecord* pStation = world.GetNetwork().FindStation(snapshot.unconnectedIds[index]);
         if (pStation == nullptr)
         {
            continue;
         }

         sf::Color rowColor(40, 40, 40);
         if (snapshot.unconnectedIds[index] == inspectedStationId || snapshot.unconnectedIds[index] == hoveredRowId)
         {
            sf::RectangleShape rowHighlight({bounds.size.x - 8.0f, UnconnectedRowHeightPixels});
            rowHighlight.setPosition({bounds.position.x + 4.0f, rowY});
            rowHighlight.setFillColor(sf::Color(220, 210, 195));
            _pWindow->draw(rowHighlight);
            rowColor = sf::Color(20, 20, 20);
         }

         sf::Text rowText(*_pFont, Utf8SfString(pStation->cityName), 14);
         rowText.setFillColor(rowColor);
         rowText.setPosition({bounds.position.x + 12.0f, rowY + 2.0f});
         _pWindow->draw(rowText);
      }
   }
} // namespace MiniDb

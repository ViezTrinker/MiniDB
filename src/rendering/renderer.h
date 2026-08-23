/*!
 *\file renderer.h
 *\brief SFML drawing of the Germany map, network and HUD.
 */

#ifndef RENDERER_H
#define RENDERER_H

#include <string_view>
#include <unordered_map>
#include <vector>

#include <SFML/Graphics.hpp>

#include "core/result.h"
#include "core/types.h"
#include "simulation/world.h"

namespace MiniDb
{
   enum class HelpVisible : bool
   {
      No = false,
      Yes = true
   };

   enum class SimulationPause : bool
   {
      No = false,
      Yes = true
   };

   enum class TrainDrag : bool
   {
      No = false,
      Yes = true
   };

   enum class LineDrag : bool
   {
      No = false,
      Yes = true
   };

   enum class AnchorDrag : bool
   {
      No = false,
      Yes = true
   };

   enum class MapSidebar : bool
   {
      Hidden = false,
      Visible = true
   };

   struct LineDragPreview
   {
      LineId lineId = InvalidLineId;
      uint32_t segmentIndex = InvalidIndex;
      StationId hoverStationId = InvalidStationId;
   };

   struct TerminusAnchorPreview
   {
      LineId lineId = InvalidLineId;
      LineEnd end = LineEnd::Front;
      StationId hoverStationId = InvalidStationId;
   };

   struct SidebarSnapshot
   {
      StationIdList unconnectedIds;
      DestinationDemandList stationDemand;
      OnboardDemandList onboardDemand;
      TrainOccupancyList lineOccupancy;
      DestinationDemandList lineDemand;
      DestinationDemandList globalDemand;
      StationCrowdingList crowdedStations;
      StationEventList activeEvents;
      float inspectorHeightPixels = 72.0f;
      float unconnectedListTopPixels = 80.0f;
      float eventsSectionHeightPixels = 0.0f;
   };

   class Renderer
   {
   public:
      /*!
       *\brief Binds the renderer to a window and font.
       *
       *\param[in] pWindow Target window.
       *\param[in] pFont Font used for labels and HUD.
       */
      Result Initialize(sf::RenderWindow* pWindow, sf::Font* pFont);

      /*!
       *\brief Loads the Germany outline from GeoJSON.
       *
       *\param[in] filePath Path to germany.geojson.
       */
      Result LoadOutline(std::string_view filePath);

      /*!
       *\brief Fits the camera to the full Germany map.
       */
      void FitGermany(void);

      /*!
       *\brief Shows or hides the unconnected-station sidebar and refits the map.
       *
       *\param[in] sidebar Whether the right-hand list is visible.
       */
      void SetMapSidebar(MapSidebar sidebar);

      /*!
       *\brief Centers the map camera on a point.
       *
       *\param[in] point Map location in kilometres.
       */
      void CenterOn(MapPoint point);

      /*!
       *\brief Updates the view after a window resize.
       *
       *\param[in] width New width in pixels.
       *\param[in] height New height in pixels.
       */
      void HandleResize(uint32_t width, uint32_t height);

      /*!
       *\brief Aligns the HUD view and map viewport with the current window size.
       */
      void SyncWindowViews(void);

      /*!
       *\brief Zooms the map camera around a screen pixel.
       *
       *\param[in] pixel Screen location under the cursor.
       *\param[in] factor Zoom factor. Values below 1 zoom in.
       */
      void ZoomAt(sf::Vector2i pixel, float factor);

      /*!
       *\brief Pans the map camera.
       *
       *\param[in] deltaKm Pan offset in map kilometres.
       */
      void Pan(sf::Vector2f deltaKm);

      /*!
       *\brief Converts a screen pixel to map kilometres.
       *
       *\param[in] pixel Screen location.
       */
      MapPoint ScreenToMap(sf::Vector2i pixel) const;

      /*!
       *\brief Converts a pixel drag into a map pan delta.
       *
       *\param[in] pixelDelta Mouse movement in pixels.
       */
      sf::Vector2f PixelDeltaToMapDelta(sf::Vector2i pixelDelta) const;

      /*!
       *\brief Converts a length in screen pixels to map kilometres at the current zoom.
       *
       *\param[in] pixels Length in pixels.
       */
      float PixelsToKm(float pixels) const;

      /*!
       *\brief Station hit radius in map kilometres at the current zoom.
       */
      float HitRadiusKm(void) const;

      /*!
       *\brief Draws the Germany outline behind the start menu.
       */
      void DrawMenuBackdrop(void);

      /*!
       *\brief Draws the current simulation state.
       *
       *\param[in] world Simulation snapshot.
       *\param[in] draftStationIds Line currently being drawn.
       *\param[in] hoveredStationId Station under the cursor, or InvalidStationId.
       *\param[in] inspectedStationId Station shown in the right inspector.
       *\param[in] inspectedTrainId Train shown in the right inspector.
       *\param[in] inspectedLineId Line shown in the right inspector.
       *\param[in] highlightLineId Line to emphasize (selected or drop target).
       *\param[in] statusText Compact status overlay.
       *\param[in] helpVisible Whether the help popup is open.
       *\param[in] pause Whether the simulation is paused.
       *\param[in] timeScale Current simulation speed multiplier.
       *\param[in] trainDrag Whether a train token is being dragged.
       *\param[in] lineDrag Whether a line segment is being dragged.
       *\param[in] lineDragPreview Line insert preview while dragging.
       *\param[in] anchorDrag Whether a terminus anchor is being dragged.
       *\param[in] anchorDragPreview Terminus extension preview while dragging.
       *\param[in] unconnectedScrollPixels Vertical list scroll in pixels.
       *\param[in] cursorPixel Current mouse position in screen pixels.
       */
      void Draw(
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
         sf::Vector2i cursorPixel);

      /*!
       *\brief Screen pixel at the center of the map viewport.
       */
      sf::Vector2i MapViewCenterPixel(void) const;

      /*!
       *\brief Returns true when the pixel is on the help button.
       *
       *\param[in] pixel Screen location.
       */
      bool IsHelpButtonHit(sf::Vector2i pixel) const;

      /*!
       *\brief Returns true when the pixel is on the train token.
       *
       *\param[in] pixel Screen location.
       */
      bool IsTrainTokenHit(sf::Vector2i pixel) const;

      /*!
       *\brief Returns true when the pixel is on the slow-down control.
       *
       *\param[in] pixel Screen location.
       */
      bool IsSlowDownButtonHit(sf::Vector2i pixel) const;

      /*!
       *\brief Returns true when the pixel is on the speed-up control.
       *
       *\param[in] pixel Screen location.
       */
      bool IsSpeedUpButtonHit(sf::Vector2i pixel) const;

      /*!
       *\brief Returns true when the pixel is on the pause control.
       *
       *\param[in] pixel Screen location.
       */
      bool IsPauseButtonHit(sf::Vector2i pixel) const;

      /*!
       *\brief Returns true when the pixel is on the resume control.
       *
       *\param[in] pixel Screen location.
       */
      bool IsResumeButtonHit(sf::Vector2i pixel) const;

      /*!
       *\brief Returns true when the pixel is on the menu control.
       *
       *\param[in] pixel Screen location.
       */
      bool IsMenuButtonHit(sf::Vector2i pixel) const;

      /*!
       *\brief Returns true when the pixel is on the map area, not the sidebar.
       *
       *\param[in] pixel Screen location.
       */
      bool IsPixelOnMap(sf::Vector2i pixel) const;

      /*!
       *\brief Returns true when the pixel is on the unconnected-station panel.
       *
       *\param[in] pixel Screen location.
       */
      bool IsUnconnectedPanelHit(sf::Vector2i pixel) const;

      /*!
       *\brief Station listed under the cursor in the sidebar, or InvalidStationId.
       *
       *\param[in] world Simulation snapshot.
       *\param[in] inspectedStationId Station shown in the inspector.
       *\param[in] inspectedTrainId Train shown in the inspector.
       *\param[in] inspectedLineId Line shown in the inspector.
       *\param[in] pixel Screen location.
       *\param[in] unconnectedScrollPixels Vertical list scroll in pixels.
       */
      StationId HitTestUnconnectedRow(
         const World& world,
         StationId inspectedStationId,
         TrainId inspectedTrainId,
         LineId inspectedLineId,
         sf::Vector2i pixel,
         float unconnectedScrollPixels) const;

      /*!
       *\brief Clamps sidebar scroll to the current unconnected list.
       *
       *\param[in] world Simulation snapshot.
       *\param[in] inspectedStationId Station shown in the inspector.
       *\param[in] inspectedTrainId Train shown in the inspector.
       *\param[in] inspectedLineId Line shown in the inspector.
       *\param[in] unconnectedScrollPixels Requested scroll.
       */
      float ClampUnconnectedScroll(
         const World& world,
         StationId inspectedStationId,
         TrainId inspectedTrainId,
         LineId inspectedLineId,
         float unconnectedScrollPixels) const;

   private:
      void DrawOutline(void);
      void DrawLines(
         const World& world,
         LineId highlightLineId,
         LineDrag lineDrag,
         const LineDragPreview& lineDragPreview);
      void DrawLineInsertPreview(
         const World& world,
         const LineDragPreview& lineDragPreview,
         sf::Vector2i cursorPixel);
      void DrawTerminusAnchors(
         const World& world,
         LineId highlightLineId,
         AnchorDrag anchorDrag,
         const TerminusAnchorPreview& anchorDragPreview);
      void DrawTerminusExtendPreview(
         const World& world,
         const TerminusAnchorPreview& anchorDragPreview,
         sf::Vector2i cursorPixel);
      void DrawDraft(const World& world, const StationIdList& draftStationIds);
      void DrawStations(const World& world, StationId hoveredStationId, StationId inspectedStationId);
      void DrawTrains(const World& world, TrainId inspectedTrainId);
      void DrawDraggedTrain(sf::Vector2i cursorPixel);
      void DrawHud(std::string_view statusText);
      void DrawHelpButton(HelpVisible helpVisible);
      void DrawTrainToken(TrainDrag trainDrag);
      void DrawPlaybackControls(SimulationPause pause, float timeScale);
      void DrawHelpPopup(void);
      void DrawSidebar(
         const World& world,
         StationId inspectedStationId,
         TrainId inspectedTrainId,
         LineId inspectedLineId,
         float unconnectedScrollPixels,
         sf::Vector2i cursorPixel);
      SidebarSnapshot BuildSidebarSnapshot(
         const World& world,
         StationId inspectedStationId,
         TrainId inspectedTrainId,
         LineId inspectedLineId) const;
      float ComputeInspectorHeightPixels(
         StationId inspectedStationId,
         TrainId inspectedTrainId,
         LineId inspectedLineId,
         const SidebarSnapshot& snapshot) const;
      void DrawSidebarInspector(
         const World& world,
         StationId inspectedStationId,
         TrainId inspectedTrainId,
         LineId inspectedLineId,
         const sf::FloatRect& panelBounds,
         const SidebarSnapshot& snapshot);
      void DrawSidebarEvents(
         const World& world,
         const sf::FloatRect& panelBounds,
         const SidebarSnapshot& snapshot);
      float EventsSectionHeightPixels(const StationEventList& activeEvents) const;
      float InspectorHeightPixels(
         const World& world,
         StationId inspectedStationId,
         TrainId inspectedTrainId,
         LineId inspectedLineId) const;
      float UnconnectedListTopPixels(
         const World& world,
         StationId inspectedStationId,
         TrainId inspectedTrainId,
         LineId inspectedLineId) const;
      void DrawSegment(MapPoint from, MapPoint to, sf::Color color, float thicknessKm);
      void DrawScaledText(std::string_view text, MapPoint position, unsigned int characterSize, sf::Color color);
      void RebuildParallelOffsetCache(const World& world);
      void RebuildOutlineVertexArrays(void);
      float ParallelOffsetKm(LineId lineId, uint32_t segmentIndex) const;
      const StationRecord* FindDraftStation(const World& world, StationId stationId) const;
      sf::FloatRect HelpButtonBounds(void) const;
      sf::FloatRect TrainTokenBounds(void) const;
      sf::FloatRect SlowDownButtonBounds(void) const;
      sf::FloatRect SpeedLabelBounds(void) const;
      sf::FloatRect SpeedUpButtonBounds(void) const;
      sf::FloatRect PauseButtonBounds(void) const;
      sf::FloatRect ResumeButtonBounds(void) const;
      sf::FloatRect MenuButtonBounds(void) const;
      float BottomHudBarTop(void) const;
      float BottomHudControlLeft(uint32_t controlIndex) const;
      sf::FloatRect UnconnectedPanelBounds(void) const;
      float SidebarWidthPixels(void) const;
      float MapWidthPixels(void) const;
      void ApplyMapViewport(void);
      void ApplyDefaultView(void);
      bool ContainsPixel(const sf::FloatRect& bounds, sf::Vector2i pixel) const;

      sf::RenderWindow* _pWindow = nullptr;
      sf::Font* _pFont = nullptr;
      sf::View _mapView;
      MapPolygonList _outlinePolygons;
      std::vector<sf::VertexArray> _outlineVertexArrays;
      std::unordered_map<uint64_t, float> _parallelOffsetKmBySegment;
      uint64_t _parallelOffsetNetworkRevision = 0;
      MapSidebar _mapSidebar = MapSidebar::Hidden;
   };
} // namespace MiniDb

#endif // RENDERER_H

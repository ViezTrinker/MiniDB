/*!
 *\file constants.h
 *\brief Shared simulation and map constants.
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <cstdint>

namespace MiniDb
{
   inline constexpr float GermanyLatMin = 47.270f;
   inline constexpr float GermanyLatMax = 55.058f;
   inline constexpr float GermanyLonMin = 5.866f;
   inline constexpr float GermanyLonMax = 15.042f;
   inline constexpr float ProjectionReferenceLatitudeDegrees = 51.0f;
   inline constexpr float KilometersPerDegreeLatitude = 111.32f;
   inline constexpr float Pi = 3.14159265358979323846f;

   inline constexpr uint32_t MinimumCityPopulation = 10000;
   inline constexpr uint32_t MinimumStationCap = 2;
   inline constexpr uint32_t InitialStationCount = 6;
   inline constexpr uint32_t DefaultMaxStationCount = 100;
   inline constexpr uint32_t StationCapStep = 50;
   inline constexpr uint32_t UnlimitedStationCount = 0xFFFFFFFFu;
   inline constexpr float StationSpawnIntervalSeconds = 5.0f;
   inline constexpr float GlobalPassengerSpawnPerSecond = 1.5f;
   inline constexpr float TrainSpeedKmPerHour = 12000.0f;
   inline constexpr float SecondsPerHour = 3600.0f;
   inline constexpr float TrainDwellSeconds = 0.5f;
   inline constexpr uint32_t TrainCapacity = 32;
   inline constexpr float ExpectedWaitHeadwayFraction = 0.5f;
   inline constexpr float GravityAlpha = 1.0f;
   inline constexpr float GravityGamma = 1.6f;
   inline constexpr float GravityDistanceOffsetKm = 20.0f;
   inline constexpr uint32_t MinimumLineStations = 2;
   inline constexpr uint32_t MinimumLoopStations = 3;
   inline constexpr uint32_t LineColorCount = 8;
   inline constexpr float MapViewMarginKm = 40.0f;
   inline constexpr uint32_t DefaultWindowWidth = 1280;
   inline constexpr uint32_t DefaultWindowHeight = 720;
   inline constexpr float DefaultTimeScale = 1.0f;

   inline constexpr float StationRadiusBasePixels = 3.0f;
   inline constexpr float StationRadiusMaxPixels = 5.0f;
   inline constexpr float StationHitRadiusPixels = 12.0f;
   inline constexpr float LineThicknessPixels = 3.0f;
   inline constexpr float ParallelLineOffsetPixels = 3.0f;
   inline constexpr float SelectedLineThicknessPixels = 4.5f;
   inline constexpr float DraftLineThicknessPixels = 2.5f;
   inline constexpr float TrainLengthPixels = 14.0f;
   inline constexpr float TrainWidthPixels = 6.0f;
   inline constexpr float TrainOutlinePixels = 1.0f;
   inline constexpr float MapLabelCharacterPixels = 14.0f;
   inline constexpr float WaitingLabelCharacterPixels = 12.0f;
   inline constexpr uint32_t DemandPanelMaxRows = 6;
   inline constexpr uint32_t InspectorMaxRows = 8;
   inline constexpr uint32_t CrowdedStationMaxRows = 10;
   inline constexpr float DemandPanelWidthPixels = 220.0f;
   inline constexpr float TrainHitRadiusPixels = 14.0f;
   inline constexpr float HudButtonSizePixels = 36.0f;
   inline constexpr float TrainTokenWidthPixels = 56.0f;
   inline constexpr float HudTextButtonWidthPixels = 78.0f;
   inline constexpr float HudSpeedLabelWidthPixels = 40.0f;
   inline constexpr float HudControlGapPixels = 8.0f;
   inline constexpr float HudButtonMarginPixels = 12.0f;
   inline constexpr float TimeScaleSlow = 1.0f;
   inline constexpr float TimeScaleMedium = 2.0f;
   inline constexpr float TimeScaleFast = 4.0f;
   inline constexpr float TimeScaleVeryFast = 8.0f;
   inline constexpr float LineDropHitPixels = 16.0f;
   inline constexpr float LineDragStartPixels = 8.0f;
   inline constexpr float KeyboardPanSpeedPixelsPerSecond = 480.0f;
   inline constexpr float KeyboardZoomInFactor = 0.9f;
   inline constexpr float KeyboardZoomOutFactor = 1.1f;
   inline constexpr float TerminusAnchorRadiusPixels = 7.0f;
   inline constexpr float TerminusAnchorOffsetPixels = 14.0f;
   inline constexpr float TerminusAnchorDragRadiusPixels = 10.0f;
   inline constexpr float UnconnectedPanelWidthPixels = 280.0f;
   inline constexpr float UnconnectedRowHeightPixels = 22.0f;
} // namespace MiniDb

#endif // CONSTANTS_H

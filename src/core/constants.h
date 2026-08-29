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
   inline constexpr uint32_t EconomicInitialStationCount = 3;
   inline constexpr uint32_t DefaultMaxStationCount = 100;
   inline constexpr uint32_t UnlimitedStationCount = 0xFFFFFFFFu;
   inline constexpr float StationSpawnIntervalSeconds = 5.0f;
   inline constexpr float EconomicStationSpawnIntervalSeconds = 45.0f;
   inline constexpr uint32_t DefaultTrainCapacity = 160;
   inline constexpr uint32_t MinimumTrainCapacity = 1;
   inline constexpr float PassengerSpawnPerSecondAtDefaultCapacity = 7.5f;
   inline constexpr float PassengerSpawnPressurePerUnlock = 1.01f;
   inline constexpr float TrainSpeedKmPerHour = 12000.0f;
   inline constexpr float SecondsPerHour = 3600.0f;
   inline constexpr float TrainDwellSeconds = 0.5f;
   inline constexpr float ExpectedWaitHeadwayFraction = 0.5f;
   inline constexpr float GravityAlpha = 1.0f;
   inline constexpr float GravityGamma = 1.6f;
   inline constexpr float GravityDistanceOffsetKm = 20.0f;
   inline constexpr float EventCheckIntervalSeconds = 90.0f;
   inline constexpr float EventDurationSeconds = 60.0f;
   inline constexpr float EventStationFraction = 0.05f;
   inline constexpr float EventDestinationWeightMultiplier = 10.0f;
   inline constexpr uint32_t EventStationMaxRows = 10;
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
   inline constexpr float LineThicknessPixels = 5.0f;
   inline constexpr float ParallelLineOffsetPixels = 6.0f;
   inline constexpr float SelectedLineThicknessPixels = 7.0f;
   inline constexpr float DraftLineThicknessPixels = 4.0f;
   inline constexpr float TrainLengthPixels = 14.0f;
   inline constexpr float TrainWidthPixels = 6.0f;
   inline constexpr float TrainOutlinePixels = 1.0f;
   inline constexpr float MapLabelCharacterPixels = 14.0f;
   inline constexpr float WaitingLabelCharacterPixels = 12.0f;
   inline constexpr uint32_t DemandPanelMaxRows = 6;
   inline constexpr uint32_t InspectorMaxRows = 8;
   inline constexpr uint32_t StationDemandMaxRows = 25;
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
   inline constexpr float TimeScaleUltraFast = 16.0f;
   inline constexpr float LineDropHitPixels = 20.0f;
   inline constexpr float LineDragStartPixels = 8.0f;
   inline constexpr float KeyboardPanSpeedPixelsPerSecond = 480.0f;
   inline constexpr float KeyboardZoomInFactor = 0.9f;
   inline constexpr float KeyboardZoomOutFactor = 1.1f;
   inline constexpr float TerminusAnchorRadiusPixels = 7.0f;
   inline constexpr float TerminusAnchorOffsetPixels = 14.0f;
   inline constexpr float TerminusAnchorDragRadiusPixels = 10.0f;
   inline constexpr float UnconnectedPanelWidthPixels = 280.0f;
   inline constexpr float UnconnectedRowHeightPixels = 22.0f;

   inline constexpr int64_t DefaultStartingBalance = 500000;
   inline constexpr float TrackBuildCostPerKm = 400.0f;
   inline constexpr float TrackMaintenanceCostPerKmPerSecond = 0.12f;
   inline constexpr int64_t TrainPurchaseCost = 35000;
   inline constexpr float TrainMaintenanceCostPerSecond = 6.0f;
   inline constexpr float FarePerPassengerKm = 1.20f;
   inline constexpr uint32_t StationWaitingCapacityPopulationFactor = 800;
   inline constexpr float StationCrowdingDwellSeconds = 4.0f;
   inline constexpr float NegativeBalanceGameOverRealSeconds = 300.0f;
   inline constexpr float BankruptcyHudWarningSeconds = 60.0f;
   inline constexpr float MaxPassengerPlatformWaitSeconds = 120.0f;
   inline constexpr float PassengerPlatformWaitWarningSeconds = 90.0f;
   inline constexpr float MaxUnconnectedPassengerPlatformWaitSeconds = 300.0f;
   inline constexpr float UnconnectedPassengerPlatformWaitWarningSeconds = 270.0f;
   inline constexpr float PlatformPatienceGraceSeconds = 600.0f;
   inline constexpr float PlatformWaitBlinkPeriodSeconds = 0.4f;
   inline constexpr float PlatformWaitWarningLeadSeconds = 30.0f;
   inline constexpr float PlaySessionSnapshotRealSeconds = 10.0f;
   inline constexpr uint32_t PlaySessionLogFlushLineCount = 50;
   inline constexpr uint32_t PlaySessionFareSampleInterval = 10;
   inline constexpr float PlaySessionFullFareLogRealSeconds = 300.0f;
   inline constexpr float InsufficientFundsToastSeconds = 3.0f;

   /*!
    *\brief Economy scale for a given train capacity setting.
    *
    *\param[in] trainCapacity Passengers per train.
    */
   inline float EconomyScaleForCapacity(uint32_t trainCapacity)
   {
      return static_cast<float>(trainCapacity) / static_cast<float>(DefaultTrainCapacity);
   }

   /*!
    *\brief Passenger spawn rate for a given train capacity.
    *
    * Scales linearly from `PassengerSpawnPerSecondAtDefaultCapacity` at
    * `DefaultTrainCapacity` (160 capacity -> 7.5 / s, 320 capacity -> 15 / s).
    *
    *\param[in] trainCapacity Passengers per train.
    */
   inline float PassengerSpawnPerSecondForCapacity(uint32_t trainCapacity)
   {
      return (static_cast<float>(trainCapacity) / static_cast<float>(DefaultTrainCapacity)) *
         PassengerSpawnPerSecondAtDefaultCapacity;
   }
} // namespace MiniDb

#endif // CONSTANTS_H

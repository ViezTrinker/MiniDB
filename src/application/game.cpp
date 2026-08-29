/*!
 *\file game.cpp
 *\brief Window, input and real-time game loop.
 */

#include "application/game.h"

#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <array>

#include "core/constants.h"

namespace MiniDb
{
   namespace
   {
      using PathCandidateList = std::array<std::string, 4>;

      std::string DirectoryFromPath(std::string_view path)
      {
         const std::string text(path);
         const size_t slashIndex = text.find_last_of("/\\");
         if (slashIndex == std::string::npos)
         {
            return std::string(".");
         }

         return text.substr(0, slashIndex);
      }

      std::string FormatTime(float seconds)
      {
         if (seconds < 0.0f)
         {
            seconds = 0.0f;
         }

         const auto totalSeconds = static_cast<uint32_t>(seconds);
         const uint32_t minutes = totalSeconds / 60u;
         const uint32_t restSeconds = totalSeconds % 60u;
         std::ostringstream stream;
         if (minutes < 10u)
         {
            stream << "0";
         }
         stream << minutes << ":";
         if (restSeconds < 10u)
         {
            stream << "0";
         }
         stream << restSeconds;
         return stream.str();
      }

      std::string FormatEuro(int64_t amount)
      {
         const bool negative = amount < 0;
         uint64_t absolute = static_cast<uint64_t>(negative ? -amount : amount);
         std::string digits = std::to_string(absolute);
         std::string formatted;
         formatted.reserve(digits.size() + 8);
         if (negative)
         {
            formatted.push_back('-');
         }
         formatted.push_back('\xE2');
         formatted.push_back('\x82');
         formatted.push_back('\xAC');

         const size_t firstGroup = digits.size() % 3;
         size_t index = 0;
         if (firstGroup > 0)
         {
            formatted.append(digits, 0, firstGroup);
            index = firstGroup;
            if (index < digits.size())
            {
               formatted.push_back(',');
            }
         }

         while (index < digits.size())
         {
            formatted.append(digits, index, 3);
            index += 3;
            if (index < digits.size())
            {
               formatted.push_back(',');
            }
         }

         return formatted;
      }

      float PixelDistanceSquared(sf::Vector2i left, sf::Vector2i right)
      {
         const auto deltaX = static_cast<float>(left.x - right.x);
         const auto deltaY = static_cast<float>(left.y - right.y);
         return (deltaX * deltaX) + (deltaY * deltaY);
      }

      bool IsValidInsertStation(const World& world, LineId lineId, StationId stationId)
      {
         if (stationId == InvalidStationId)
         {
            return false;
         }

         const Line* pLine = world.GetNetwork().FindLine(lineId);
         if (pLine == nullptr)
         {
            return false;
         }

         return world.GetNetwork().StationIndexOnLine(*pLine, stationId) == InvalidIndex;
      }

      bool IsValidAnchorTarget(const World& world, LineId lineId, LineEnd end, StationId stationId)
      {
         if (stationId == InvalidStationId)
         {
            return false;
         }

         const Line* pLine = world.GetNetwork().FindLine(lineId);
         if (pLine == nullptr || pLine->loop == LineLoop::Yes)
         {
            return false;
         }
         if (world.GetNetwork().FindStation(stationId) == nullptr)
         {
            return false;
         }

         if (world.GetNetwork().StationIndexOnLine(*pLine, stationId) == InvalidIndex)
         {
            return true;
         }

         if (pLine->stationIds.size() < MinimumLoopStations)
         {
            return false;
         }

         if (end == LineEnd::Back && stationId == pLine->stationIds.front())
         {
            return true;
         }
         if (end == LineEnd::Front && stationId == pLine->stationIds.back())
         {
            return true;
         }

         return false;
      }
   } // namespace

   Game::Game(void) :
      _world(static_cast<uint32_t>(std::time(nullptr))),
      _timeScale(DefaultTimeScale)
   {
   }

   bool Game::FileExists(std::string_view filePath) const
   {
      const std::string pathText(filePath);
      std::ifstream file;
      file.open(pathText);
      return file.is_open();
   }

   std::string Game::ResolveDataFile(std::string_view executablePath, std::string_view fileName) const
   {
      const std::string executableDirectory = DirectoryFromPath(executablePath);
      const PathCandidateList candidates = {
         executableDirectory + "/data/" + std::string(fileName),
         std::string("data/") + std::string(fileName),
         std::string("../data/") + std::string(fileName),
         std::string("../../data/") + std::string(fileName)
      };

      for (const std::string& candidate : candidates)
      {
         if (FileExists(candidate))
         {
            return candidate;
         }
      }

      return candidates[0];
   }

   Result Game::LoadFont(void)
   {
      const PathCandidateList fontPaths = {
         std::string("C:/Windows/Fonts/segoeui.ttf"),
         std::string("C:/Windows/Fonts/arial.ttf"),
         std::string("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"),
         std::string("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf")
      };

      for (const std::string& fontPath : fontPaths)
      {
         if (_font.openFromFile(fontPath))
         {
            return Result::Ok;
         }
      }

      return Result::FileError;
   }

   Result Game::LoadSimulationData(std::string_view executablePath)
   {
      const std::string stationsPath = ResolveDataFile(executablePath, "stations.json");
      const Result catalogResult = _world.LoadCatalogFromFile(stationsPath);
      if (IsErr(catalogResult))
      {
         return catalogResult;
      }

      const std::string outlinePath = ResolveDataFile(executablePath, "germany.geojson");
      const Result outlineResult = _renderer.LoadOutline(outlinePath);
      if (IsErr(outlineResult))
      {
         return outlineResult;
      }

      return Result::Ok;
   }

   Result Game::Initialize(std::string_view executablePath)
   {
      const sf::VideoMode videoMode({DefaultWindowWidth, DefaultWindowHeight});
      _window.create(videoMode, "MiniDB");
      ConfigureWindow();

      const Result fontResult = LoadFont();
      if (IsErr(fontResult))
      {
         return fontResult;
      }

      const Result rendererResult = _renderer.Initialize(&_window, &_font);
      if (IsErr(rendererResult))
      {
         return rendererResult;
      }

      _mainMenu.Initialize(&_window, &_font);
      _logsDirectory = DirectoryFromPath(executablePath) + "/logs";
      _world.SetPlaySessionLog(&_playSessionLog);

      const Result dataResult = LoadSimulationData(executablePath);
      if (IsErr(dataResult))
      {
         return dataResult;
      }

      _clock.restart();
      return Result::Ok;
   }

   int32_t Game::Run(void)
   {
      while (_window.isOpen())
      {
         ProcessEvents();
         const float deltaSeconds = _clock.restart().asSeconds();
         Update(deltaSeconds);
         Render();
      }

      return 0;
   }

   void Game::ProcessEvents(void)
   {
      _renderer.SyncWindowViews();
      while (const std::optional<sf::Event> event = _window.pollEvent())
      {
         if (event->is<sf::Event::Closed>())
         {
            _window.close();
            continue;
         }

         if (const sf::Event::Resized* pResized = event->getIf<sf::Event::Resized>())
         {
            _renderer.HandleResize(pResized->size.x, pResized->size.y);
            continue;
         }

         if (const sf::Event::TextEntered* pTextEntered = event->getIf<sf::Event::TextEntered>())
         {
            HandleTextEntered(*pTextEntered);
            continue;
         }

         if (const sf::Event::KeyPressed* pKeyPressed = event->getIf<sf::Event::KeyPressed>())
         {
            HandleKeyPressed(*pKeyPressed);
            continue;
         }

         if (const sf::Event::MouseButtonPressed* pMousePressed = event->getIf<sf::Event::MouseButtonPressed>())
         {
            HandleMousePressed(*pMousePressed);
            continue;
         }

         if (const sf::Event::MouseButtonReleased* pMouseReleased = event->getIf<sf::Event::MouseButtonReleased>())
         {
            HandleMouseReleased(*pMouseReleased);
            continue;
         }

         if (const sf::Event::MouseMoved* pMouseMoved = event->getIf<sf::Event::MouseMoved>())
         {
            HandleMouseMoved(*pMouseMoved);
            continue;
         }

         if (const sf::Event::MouseWheelScrolled* pMouseWheel = event->getIf<sf::Event::MouseWheelScrolled>())
         {
            HandleMouseWheel(*pMouseWheel);
         }
      }
   }

   void Game::ApplyMenuAction(MenuAction action)
   {
      if (action == MenuAction::Start)
      {
         StartNewGame();
         return;
      }
      if (action == MenuAction::Resume)
      {
         _mainMenu.SetSelectedMaxStationCount(
            _world.GetMaxStationCount(),
            _world.GetCatalogStationCount());
         _mainMenu.SetTrainCapacity(_world.GetTrainCapacity());
         _mainMenu.SetGameSpeed(_timeScale);
         _renderer.SetMapSidebar(MapSidebar::Visible);
         _appScreen = AppScreen::Playing;
         return;
      }
      if (action == MenuAction::Quit)
      {
         EndPlaySessionLog("quit");
         _window.close();
      }
   }

   void Game::ResetPlayInput(void)
   {
      _lineEditor.Reset();
      _pause = SimulationPause::No;
      _timeScale = DefaultTimeScale;
      _panState = PanState::No;
      _helpVisible = HelpVisible::No;
      _trainDrag = TrainDrag::No;
      _lineDrag = LineDrag::No;
      _lineGrabPending = LineGrabPending::No;
      _anchorGrabPending = AnchorGrabPending::No;
      _anchorDrag = AnchorDrag::No;
      _dropTargetLineId = InvalidLineId;
      _lineDragLineId = InvalidLineId;
      _anchorDragLineId = InvalidLineId;
      _lineDragSegmentIndex = InvalidIndex;
      _lineDragHoverStationId = InvalidStationId;
      _anchorDragHoverStationId = InvalidStationId;
      _unconnectedScrollPixels = 0.0f;
      _inspectedStationId = InvalidStationId;
      _inspectedTrainId = InvalidTrainId;
      _inspectedLineId = InvalidLineId;
   }

   void Game::SlowDownSimulation(void)
   {
      if (_timeScale >= TimeScaleUltraFast)
      {
         _timeScale = TimeScaleVeryFast;
         return;
      }
      if (_timeScale >= TimeScaleVeryFast)
      {
         _timeScale = TimeScaleFast;
         return;
      }
      if (_timeScale >= TimeScaleFast)
      {
         _timeScale = TimeScaleMedium;
         return;
      }
      if (_timeScale >= TimeScaleMedium)
      {
         _timeScale = TimeScaleSlow;
         return;
      }

      _timeScale = TimeScaleSlow;
   }

   void Game::SpeedUpSimulation(void)
   {
      if (_timeScale <= TimeScaleSlow)
      {
         _timeScale = TimeScaleMedium;
         return;
      }
      if (_timeScale <= TimeScaleMedium)
      {
         _timeScale = TimeScaleFast;
         return;
      }
      if (_timeScale <= TimeScaleFast)
      {
         _timeScale = TimeScaleVeryFast;
         return;
      }
      if (_timeScale <= TimeScaleVeryFast)
      {
         _timeScale = TimeScaleUltraFast;
         return;
      }

      _timeScale = TimeScaleUltraFast;
   }

   void Game::StartNewGame(void)
   {
      _world.ResetSimulation();
      const GameMode gameMode = _mainMenu.GetGameMode();
      uint32_t stationCap = _mainMenu.GetSelectedMaxStationCount();
      if (gameMode == GameMode::Sandbox)
      {
         stationCap = UnlimitedStationCount;
      }

      _world.SetMaxStationCount(stationCap);
      _world.SetTrainCapacity(_mainMenu.GetTrainCapacity());
      _world.ConfigureEconomy(
         _mainMenu.GetTrainCapacity(),
         gameMode,
         _mainMenu.GetNeverLoseSetting());
      _world.ConfigureNewGame(
         _mainMenu.GetRandomPool(),
         _mainMenu.GetRandomOrder(),
         _mainMenu.GetEventsEnabled());
      const Result spawnResult = _world.SpawnInitialStations();
      if (IsErr(spawnResult))
      {
         return;
      }

      ResetPlayInput();
      _timeScale = _mainMenu.GetGameSpeed();
      _statusMessage.clear();
      _statusMessageSeconds = 0.0f;
      _menuBannerMessage.clear();
      BeginPlaySessionLog();
      _renderer.SetMapSidebar(MapSidebar::Visible);
      _hasActiveGame = HasActiveGame::Yes;
      _appScreen = AppScreen::Playing;
   }

   void Game::ReturnToMenu(std::string_view sessionEndReason)
   {
      EndPlaySessionLog(sessionEndReason);
      _trainDrag = TrainDrag::No;
      _lineDrag = LineDrag::No;
      _lineGrabPending = LineGrabPending::No;
      _anchorGrabPending = AnchorGrabPending::No;
      _anchorDrag = AnchorDrag::No;
      _dropTargetLineId = InvalidLineId;
      _lineDragHoverStationId = InvalidStationId;
      _anchorDragHoverStationId = InvalidStationId;
      _panState = PanState::No;
      _helpVisible = HelpVisible::No;
      _mainMenu.SetSelectedMaxStationCount(
         _world.GetMaxStationCount(),
         _world.GetCatalogStationCount());
      _mainMenu.SetTrainCapacity(_world.GetTrainCapacity());
      _mainMenu.SetGameSpeed(_timeScale);
      _mainMenu.ShowRootPage();
      _renderer.SetMapSidebar(MapSidebar::Hidden);
      _appScreen = AppScreen::Menu;
   }

   void Game::ConfigureWindow(void)
   {
      _window.setFramerateLimit(60);
      _window.setKeyRepeatEnabled(false);
      const sf::Vector2u size = _window.getSize();
      _renderer.HandleResize(size.x, size.y);
   }

   void Game::ToggleFullscreen(void)
   {
      if (_fullscreenMode == FullscreenMode::No)
      {
         _windowedSize = _window.getSize();
         const sf::VideoMode desktopMode = sf::VideoMode::getDesktopMode();
         _window.create(desktopMode, "MiniDB", sf::State::Fullscreen);
         _fullscreenMode = FullscreenMode::Yes;
      }
      else
      {
         const sf::VideoMode windowedMode(_windowedSize);
         _window.create(windowedMode, "MiniDB", sf::State::Windowed);
         _fullscreenMode = FullscreenMode::No;
      }

      ConfigureWindow();
      if (_appScreen == AppScreen::Playing)
      {
         _renderer.SetMapSidebar(MapSidebar::Visible);
      }
      else
      {
         _renderer.SetMapSidebar(MapSidebar::Hidden);
      }
   }

   void Game::HandleKeyPressed(const sf::Event::KeyPressed& keyPressed)
   {
      if (keyPressed.code == sf::Keyboard::Key::F11)
      {
         ToggleFullscreen();
         return;
      }

      if (_appScreen == AppScreen::Menu)
      {
         ApplyMenuAction(_mainMenu.HandleKeyPressed(
            keyPressed,
            _hasActiveGame,
            _world.GetCatalogStationCount()));
         return;
      }

      if (keyPressed.code == sf::Keyboard::Key::Escape)
      {
         if (_lineDrag == LineDrag::Yes || _lineGrabPending == LineGrabPending::Yes)
         {
            _lineDrag = LineDrag::No;
            _lineGrabPending = LineGrabPending::No;
            _lineDragHoverStationId = InvalidStationId;
            return;
         }
         if (_anchorDrag == AnchorDrag::Yes || _anchorGrabPending == AnchorGrabPending::Yes)
         {
            _anchorDrag = AnchorDrag::No;
            _anchorGrabPending = AnchorGrabPending::No;
            _anchorDragHoverStationId = InvalidStationId;
            return;
         }
         if (_lineEditor.IsDrafting())
         {
            _lineEditor.Cancel();
            return;
         }

         ReturnToMenu();
         return;
      }
      if (keyPressed.control)
      {
         if (keyPressed.code == sf::Keyboard::Key::Z)
         {
            _lineEditor.UndoDraft();
            return;
         }
         if (keyPressed.code == sf::Keyboard::Key::Y)
         {
            _lineEditor.RedoDraft();
            return;
         }
      }
      if (keyPressed.code == sf::Keyboard::Key::Enter)
      {
         HandleActionResult(_lineEditor.Confirm(_world));
         return;
      }
      if (keyPressed.code == sf::Keyboard::Key::T)
      {
         HandleActionResult(_lineEditor.AddTrainToSelectedLine(_world));
         return;
      }
      if (keyPressed.code == sf::Keyboard::Key::Delete)
      {
         if (_lineDrag == LineDrag::Yes || _lineGrabPending == LineGrabPending::Yes)
         {
            _lineEditor.SelectLine(_lineDragLineId);
            _lineDrag = LineDrag::No;
            _lineGrabPending = LineGrabPending::No;
            _lineDragHoverStationId = InvalidStationId;
         }
         if (_anchorDrag == AnchorDrag::Yes || _anchorGrabPending == AnchorGrabPending::Yes)
         {
            _anchorDrag = AnchorDrag::No;
            _anchorGrabPending = AnchorGrabPending::No;
            _anchorDragHoverStationId = InvalidStationId;
         }

         LineId lineId = _lineEditor.GetSelectedLineId();
         if (lineId == InvalidLineId && _inspectedTrainId != InvalidTrainId)
         {
            const Train* pTrain = _world.FindTrain(_inspectedTrainId);
            if (pTrain != nullptr)
            {
               lineId = pTrain->lineId;
               _lineEditor.SelectLine(lineId);
            }
         }

         const Result deleteResult = _lineEditor.DeleteSelectedLine(_world);
         if (IsOk(deleteResult))
         {
            if (_world.FindTrain(_inspectedTrainId) == nullptr)
            {
               _inspectedTrainId = InvalidTrainId;
            }
            if (_world.GetNetwork().FindLine(_inspectedLineId) == nullptr)
            {
               _inspectedLineId = InvalidLineId;
            }
         }
         return;
      }
      if (keyPressed.code == sf::Keyboard::Key::Space)
      {
         if (_pause == SimulationPause::No)
         {
            _pause = SimulationPause::Yes;
         }
         else
         {
            _pause = SimulationPause::No;
         }
         return;
      }
      if (keyPressed.code == sf::Keyboard::Key::Num1)
      {
         if (!keyPressed.alt)
         {
            _timeScale = TimeScaleSlow;
         }
         return;
      }
      if (keyPressed.code == sf::Keyboard::Key::Num2)
      {
         if (!keyPressed.alt)
         {
            _timeScale = TimeScaleMedium;
         }
         return;
      }
      if (keyPressed.code == sf::Keyboard::Key::Num4)
      {
         if (!keyPressed.alt)
         {
            _timeScale = TimeScaleFast;
         }
         return;
      }
      if (keyPressed.code == sf::Keyboard::Key::Num8)
      {
         if (!keyPressed.alt)
         {
            _timeScale = TimeScaleVeryFast;
         }
         return;
      }
      if (keyPressed.code == sf::Keyboard::Key::Equal ||
         keyPressed.code == sf::Keyboard::Key::Add)
      {
         _renderer.ZoomAt(_renderer.MapViewCenterPixel(), KeyboardZoomInFactor);
         return;
      }
      if (keyPressed.code == sf::Keyboard::Key::Hyphen ||
         keyPressed.code == sf::Keyboard::Key::Subtract)
      {
         _renderer.ZoomAt(_renderer.MapViewCenterPixel(), KeyboardZoomOutFactor);
         return;
      }
   }

   void Game::HandleTextEntered(const sf::Event::TextEntered& textEntered)
   {
      if (_appScreen == AppScreen::Menu)
      {
         _mainMenu.HandleTextEntered(textEntered.unicode);
      }
   }

   void Game::HandleMousePressed(const sf::Event::MouseButtonPressed& mousePressed)
   {
      _lastMousePixel = mousePressed.position;
      if (_appScreen == AppScreen::Menu)
      {
         if (mousePressed.button == sf::Mouse::Button::Left)
         {
            ApplyMenuAction(_mainMenu.HandleClick(
               mousePressed.position,
               _hasActiveGame,
               _world.GetCatalogStationCount()));
         }
         return;
      }

      if (mousePressed.button == sf::Mouse::Button::Left)
      {
         if (_renderer.IsHelpButtonHit(mousePressed.position))
         {
            if (_helpVisible == HelpVisible::No)
            {
               _helpVisible = HelpVisible::Yes;
            }
            else
            {
               _helpVisible = HelpVisible::No;
            }
            return;
         }

         if (_renderer.IsTrainTokenHit(mousePressed.position))
         {
            _trainDrag = TrainDrag::Yes;
            _dropTargetLineId = InvalidLineId;
            _helpVisible = HelpVisible::No;
            _lineDrag = LineDrag::No;
            _lineGrabPending = LineGrabPending::No;
            _anchorGrabPending = AnchorGrabPending::No;
            _anchorDrag = AnchorDrag::No;
            return;
         }

         if (_renderer.IsSlowDownButtonHit(mousePressed.position))
         {
            SlowDownSimulation();
            _helpVisible = HelpVisible::No;
            return;
         }

         if (_renderer.IsSpeedUpButtonHit(mousePressed.position))
         {
            SpeedUpSimulation();
            _helpVisible = HelpVisible::No;
            return;
         }

         if (_renderer.IsPauseButtonHit(mousePressed.position))
         {
            _pause = SimulationPause::Yes;
            _helpVisible = HelpVisible::No;
            return;
         }

         if (_renderer.IsResumeButtonHit(mousePressed.position))
         {
            _pause = SimulationPause::No;
            _helpVisible = HelpVisible::No;
            return;
         }

         if (_renderer.IsMenuButtonHit(mousePressed.position))
         {
            ReturnToMenu();
            return;
         }

         if (_renderer.IsUnconnectedPanelHit(mousePressed.position))
         {
            const StationId listedId = _renderer.HitTestUnconnectedRow(
               _world,
               _inspectedStationId,
               _inspectedTrainId,
               _inspectedLineId,
               mousePressed.position,
               _unconnectedScrollPixels);
            if (listedId != InvalidStationId)
            {
               _inspectedStationId = listedId;
               _inspectedTrainId = InvalidTrainId;
               _inspectedLineId = InvalidLineId;
               const StationRecord* pStation = _world.GetNetwork().FindStation(listedId);
               if (pStation != nullptr)
               {
                  _renderer.CenterOn(pStation->position);
               }
            }
            return;
         }

         if (!_renderer.IsPixelOnMap(mousePressed.position))
         {
            return;
         }

         const MapPoint mapPoint = _renderer.ScreenToMap(mousePressed.position);
         if (!_lineEditor.IsDrafting())
         {
            const LineId selectedLineId = _lineEditor.GetSelectedLineId();
            if (selectedLineId != InvalidLineId)
            {
               LineEnd anchorEnd = LineEnd::Front;
               if (_world.HitTestTerminusAnchor(
                  selectedLineId,
                  mapPoint,
                  _renderer.PixelsToKm(TerminusAnchorDragRadiusPixels),
                  _renderer.PixelsToKm(TerminusAnchorOffsetPixels),
                  anchorEnd))
               {
                  _anchorGrabPending = AnchorGrabPending::Yes;
                  _anchorDragLineId = selectedLineId;
                  _anchorDragEnd = anchorEnd;
                  _anchorDragStartPixel = mousePressed.position;
                  _anchorDragHoverStationId = InvalidStationId;
                  _lineDrag = LineDrag::No;
                  _lineGrabPending = LineGrabPending::No;
                  _helpVisible = HelpVisible::No;
                  return;
               }
            }
         }

         const StationId stationId = _world.HitTestStation(mapPoint, _renderer.HitRadiusKm());
         if (stationId != InvalidStationId)
         {
            _inspectedStationId = stationId;
            _inspectedTrainId = InvalidTrainId;
            _inspectedLineId = InvalidLineId;
            _lineEditor.OnStationClicked(_world, stationId);
            return;
         }

         const TrainId trainId = _world.HitTestTrain(mapPoint, _renderer.PixelsToKm(TrainHitRadiusPixels));
         if (trainId != InvalidTrainId)
         {
            _inspectedTrainId = trainId;
            _inspectedStationId = InvalidStationId;
            _inspectedLineId = InvalidLineId;
            const Train* pTrain = _world.FindTrain(trainId);
            if (pTrain != nullptr)
            {
               _lineEditor.SelectLine(pTrain->lineId);
            }
            return;
         }

         _inspectedStationId = InvalidStationId;
         _inspectedTrainId = InvalidTrainId;
         _helpVisible = HelpVisible::No;
         _inspectedLineId = InvalidLineId;
         if (!_lineEditor.IsDrafting())
         {
            const LineSegmentHit hit = _world.FindNearestLineSegment(
               mapPoint,
               _renderer.PixelsToKm(LineDropHitPixels));
            if (hit.lineId != InvalidLineId)
            {
               _inspectedLineId = hit.lineId;
               _lineGrabPending = LineGrabPending::Yes;
               _lineDragLineId = hit.lineId;
               _lineDragSegmentIndex = hit.segmentIndex;
               _lineDragStartPixel = mousePressed.position;
               _lineDragHoverStationId = InvalidStationId;
            }
         }
         return;
      }

      if (mousePressed.button == sf::Mouse::Button::Right)
      {
         HandleActionResult(_lineEditor.Confirm(_world));
         return;
      }

      if (mousePressed.button == sf::Mouse::Button::Middle)
      {
         _panState = PanState::Yes;
      }
   }

   void Game::HandleMouseReleased(const sf::Event::MouseButtonReleased& mouseReleased)
   {
      if (_appScreen == AppScreen::Menu)
      {
         return;
      }

      if (mouseReleased.button == sf::Mouse::Button::Left && _trainDrag == TrainDrag::Yes)
      {
         const MapPoint mapPoint = _renderer.ScreenToMap(mouseReleased.position);
         const LineId lineId = _world.FindNearestLine(mapPoint, _renderer.PixelsToKm(LineDropHitPixels));
         if (lineId != InvalidLineId)
         {
            HandleActionResult(_world.AddTrainToLineAt(lineId, mapPoint));
         }
         _trainDrag = TrainDrag::No;
         _dropTargetLineId = InvalidLineId;
      }

      if (mouseReleased.button == sf::Mouse::Button::Left &&
         (_anchorDrag == AnchorDrag::Yes || _anchorGrabPending == AnchorGrabPending::Yes))
      {
         if (_anchorDrag == AnchorDrag::Yes &&
            IsValidAnchorTarget(_world, _anchorDragLineId, _anchorDragEnd, _anchorDragHoverStationId))
         {
            const Result extendResult = _world.ExtendLineAt(
               _anchorDragLineId,
               _anchorDragEnd,
               _anchorDragHoverStationId);
            HandleActionResult(extendResult);
            if (IsOk(extendResult))
            {
               _lineEditor.SelectLine(_anchorDragLineId);
               _inspectedStationId = _anchorDragHoverStationId;
               _inspectedTrainId = InvalidTrainId;
               _inspectedLineId = InvalidLineId;
            }
         }

         _anchorDrag = AnchorDrag::No;
         _anchorGrabPending = AnchorGrabPending::No;
         _anchorDragHoverStationId = InvalidStationId;
      }

      if (mouseReleased.button == sf::Mouse::Button::Left &&
         (_lineDrag == LineDrag::Yes || _lineGrabPending == LineGrabPending::Yes))
      {
         if (_lineDrag == LineDrag::Yes &&
            IsValidInsertStation(_world, _lineDragLineId, _lineDragHoverStationId))
         {
            const Result insertResult = _world.InsertStationOnLine(
               _lineDragLineId,
               _lineDragSegmentIndex,
               _lineDragHoverStationId);
            HandleActionResult(insertResult);
            if (IsOk(insertResult))
            {
               _lineEditor.SelectLine(_lineDragLineId);
               _inspectedStationId = _lineDragHoverStationId;
               _inspectedTrainId = InvalidTrainId;
               _inspectedLineId = InvalidLineId;
            }
         }
         else if (_lineGrabPending == LineGrabPending::Yes && _lineDrag == LineDrag::No)
         {
            _lineEditor.SelectLine(_lineDragLineId);
         }

         _lineDrag = LineDrag::No;
         _lineGrabPending = LineGrabPending::No;
         _lineDragHoverStationId = InvalidStationId;
      }

      if (mouseReleased.button == sf::Mouse::Button::Middle)
      {
         _panState = PanState::No;
      }
   }

   void Game::HandleMouseMoved(const sf::Event::MouseMoved& mouseMoved)
   {
      if (_appScreen == AppScreen::Menu)
      {
         _lastMousePixel = mouseMoved.position;
         return;
      }

      if (_trainDrag == TrainDrag::Yes)
      {
         const MapPoint mapPoint = _renderer.ScreenToMap(mouseMoved.position);
         _dropTargetLineId = _world.FindNearestLine(mapPoint, _renderer.PixelsToKm(LineDropHitPixels));
      }
      else if (_anchorGrabPending == AnchorGrabPending::Yes || _anchorDrag == AnchorDrag::Yes)
      {
         const float startDistanceSquared = PixelDistanceSquared(_anchorDragStartPixel, mouseMoved.position);
         const float startThreshold = LineDragStartPixels * LineDragStartPixels;
         if (_anchorDrag == AnchorDrag::No && startDistanceSquared >= startThreshold)
         {
            _anchorDrag = AnchorDrag::Yes;
            _lineEditor.Cancel();
            _lineEditor.SelectLine(_anchorDragLineId);
         }

         const MapPoint mapPoint = _renderer.ScreenToMap(mouseMoved.position);
         const StationId stationId = _world.HitTestStation(mapPoint, _renderer.HitRadiusKm());
         if (IsValidAnchorTarget(_world, _anchorDragLineId, _anchorDragEnd, stationId))
         {
            _anchorDragHoverStationId = stationId;
         }
         else
         {
            _anchorDragHoverStationId = InvalidStationId;
         }
      }
      else if (_lineGrabPending == LineGrabPending::Yes || _lineDrag == LineDrag::Yes)
      {
         const float startDistanceSquared = PixelDistanceSquared(_lineDragStartPixel, mouseMoved.position);
         const float startThreshold = LineDragStartPixels * LineDragStartPixels;
         if (_lineDrag == LineDrag::No && startDistanceSquared >= startThreshold)
         {
            _lineDrag = LineDrag::Yes;
            _lineEditor.Cancel();
            _lineEditor.SelectLine(_lineDragLineId);
         }

         const MapPoint mapPoint = _renderer.ScreenToMap(mouseMoved.position);
         const StationId stationId = _world.HitTestStation(mapPoint, _renderer.HitRadiusKm());
         if (IsValidInsertStation(_world, _lineDragLineId, stationId))
         {
            _lineDragHoverStationId = stationId;
         }
         else
         {
            _lineDragHoverStationId = InvalidStationId;
         }
      }
      else if (_panState == PanState::Yes && _renderer.IsPixelOnMap(mouseMoved.position))
      {
         const sf::Vector2i pixelDelta = _lastMousePixel - mouseMoved.position;
         const sf::Vector2f mapDelta = _renderer.PixelDeltaToMapDelta(pixelDelta);
         _renderer.Pan(mapDelta);
      }

      _lastMousePixel = mouseMoved.position;
   }

   void Game::HandleMouseWheel(const sf::Event::MouseWheelScrolled& mouseWheel)
   {
      if (_appScreen == AppScreen::Menu)
      {
         return;
      }

      if (_renderer.IsUnconnectedPanelHit(mouseWheel.position))
      {
         _unconnectedScrollPixels -= mouseWheel.delta * UnconnectedRowHeightPixels * 2.0f;
         _unconnectedScrollPixels = _renderer.ClampUnconnectedScroll(
            _world,
            _inspectedStationId,
            _inspectedTrainId,
            _inspectedLineId,
            _unconnectedScrollPixels);
         return;
      }

      if (!_renderer.IsPixelOnMap(mouseWheel.position))
      {
         return;
      }

      float factor = 1.0f;
      if (mouseWheel.delta > 0.0f)
      {
         factor = 0.9f;
      }
      else if (mouseWheel.delta < 0.0f)
      {
         factor = 1.1f;
      }

      _renderer.ZoomAt(mouseWheel.position, factor);
   }

   void Game::Update(float deltaSeconds)
   {
      if (_appScreen == AppScreen::Menu)
      {
         return;
      }

      UpdateKeyboardCamera(deltaSeconds);

      float clampedDelta = deltaSeconds;
      if (clampedDelta > 0.05f)
      {
         clampedDelta = 0.05f;
      }

      if (_statusMessageSeconds > 0.0f)
      {
         _statusMessageSeconds -= clampedDelta;
         if (_statusMessageSeconds <= 0.0f)
         {
            _statusMessageSeconds = 0.0f;
            _statusMessage.clear();
         }
      }

      if (_world.GetGameMode() == GameMode::Economic)
      {
         const bool pauseActive = _pause == SimulationPause::Yes;
         const BankruptcyTickResult bankruptcyResult =
            _world.TickBankruptcy(clampedDelta, pauseActive);
         if (bankruptcyResult == BankruptcyTickResult::GameOver)
         {
            HandleGameOver(GameOverReason::Bankruptcy);
            return;
         }

         if (_playSessionLog.IsActive())
         {
            if (!pauseActive)
            {
               _playSessionLog.Tick(clampedDelta, _world, _world.GetEconomy());
            }
         }
      }

      float simulationDelta = 0.0f;
      if (_pause == SimulationPause::No)
      {
         simulationDelta = clampedDelta * _timeScale;
      }

      _world.Tick(simulationDelta);
      if (_world.IsPlatformPatienceGameOver())
      {
         HandleGameOver(GameOverReason::PlatformWait);
      }
   }

   void Game::UpdateKeyboardCamera(float deltaSeconds)
   {
      if (deltaSeconds <= 0.0f)
      {
         return;
      }

      const auto stepPixels = static_cast<int32_t>(KeyboardPanSpeedPixelsPerSecond * deltaSeconds);
      if (stepPixels <= 0)
      {
         return;
      }

      int32_t deltaX = 0;
      int32_t deltaY = 0;
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))
      {
         deltaX -= stepPixels;
      }
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right))
      {
         deltaX += stepPixels;
      }
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))
      {
         deltaY -= stepPixels;
      }
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))
      {
         deltaY += stepPixels;
      }

      if (deltaX == 0 && deltaY == 0)
      {
         return;
      }

      const sf::Vector2f mapDelta = _renderer.PixelDeltaToMapDelta({deltaX, deltaY});
      _renderer.Pan(mapDelta);
   }

   std::string Game::BuildHudText(void) const
   {
      std::ostringstream stream;
      stream << "Time " << FormatTime(_world.GetSimulationTimeSeconds());
      if (_world.GetGameMode() == GameMode::Economic)
      {
         stream << "   Balance " << FormatEuro(_world.GetBalance());
         const Economy& economy = _world.GetEconomy();
         if (economy.GetBalance() < 0 && economy.GetNeverLose() == NeverLose::No)
         {
            stream << "   Bankrupt in " << FormatTime(economy.GetSecondsUntilGameOver());
         }
         const float patienceRemaining = _world.GetWorstPlatformWaitRemainingSeconds();
         if (economy.GetNeverLose() == NeverLose::No &&
            patienceRemaining <= PlatformWaitWarningLeadSeconds)
         {
            stream << "   Patience " << FormatTime(patienceRemaining);
         }
      }
      stream << "   Stations " << _world.GetNetwork().GetStations().size();
      stream << "/" << _world.GetStationCap();
      stream << "   Waiting " << _world.GetWaitingPassengerCount();
      stream << "   Onboard " << _world.GetOnboardPassengerCount();
      stream << "   Arrived " << _world.GetArrivedPassengerCount();
      if (_pause == SimulationPause::Yes)
      {
         stream << "   PAUSED";
      }
      else
      {
         stream << "   " << static_cast<int32_t>(_timeScale) << "x";
      }
      if (!_statusMessage.empty())
      {
         stream << "   " << _statusMessage;
      }
      return stream.str();
   }

   void Game::HandleActionResult(Result result)
   {
      if (result != Result::InsufficientFunds)
      {
         return;
      }

      _statusMessage = "Insufficient funds";
      _statusMessageSeconds = InsufficientFundsToastSeconds;
   }

   void Game::HandleGameOver(GameOverReason reason)
   {
      if (_playSessionLog.IsActive())
      {
         const Economy& economy = _world.GetEconomy();
         _playSessionLog.LogGameOver(
            reason,
            economy.GetNegativeBalanceRealSeconds(),
            economy.GetBalance());
      }

      if (reason == GameOverReason::PlatformWait)
      {
         _menuBannerMessage = "Game over - passenger waited too long";
      }
      else
      {
         _menuBannerMessage = "Bankrupt - 5 minutes in the red";
      }
      ReturnToMenu("game_over");
   }

   void Game::BeginPlaySessionLog(void)
   {
      if (_world.GetGameMode() != GameMode::Economic)
      {
         return;
      }

      PlaySessionSettings settings;
      settings.stationCap = _world.GetMaxStationCount();
      settings.trainCapacity = _world.GetTrainCapacity();
      settings.gameSpeed = _timeScale;
      settings.randomPool = _mainMenu.GetRandomPool();
      settings.randomOrder = _mainMenu.GetRandomOrder();
      settings.eventsEnabled = _mainMenu.GetEventsEnabled();
      settings.neverLose = _mainMenu.GetNeverLoseSetting();
      const Result beginResult = _playSessionLog.BeginSession(
         _logsDirectory,
         settings,
         _world.GetEconomy());
      if (IsErr(beginResult))
      {
         return;
      }
   }

   void Game::EndPlaySessionLog(std::string_view reason)
   {
      if (!_playSessionLog.IsActive())
      {
         return;
      }

      _playSessionLog.EndSession(reason, _world, _world.GetEconomy());
   }

   void Game::Render(void)
   {
      if (_appScreen == AppScreen::Menu)
      {
         _renderer.DrawMenuBackdrop();
         _mainMenu.Draw(_hasActiveGame, _world.GetCatalogStationCount(), _lastMousePixel);
         if (!_menuBannerMessage.empty())
         {
            sf::Text bannerText(_font, _menuBannerMessage, 18);
            bannerText.setFillColor(sf::Color(180, 40, 40));
            const sf::Vector2u windowSize = _window.getSize();
            const sf::FloatRect textBounds = bannerText.getLocalBounds();
            bannerText.setPosition({
               (static_cast<float>(windowSize.x) - textBounds.size.x) * 0.5f,
               static_cast<float>(windowSize.y) - 48.0f});
            _window.draw(bannerText);
         }
         _window.display();
         return;
      }

      const MapPoint hoverPoint = _renderer.ScreenToMap(_lastMousePixel);
      StationId hoveredStationId = _world.HitTestStation(hoverPoint, _renderer.HitRadiusKm());
      if (_lineDragHoverStationId != InvalidStationId)
      {
         hoveredStationId = _lineDragHoverStationId;
      }
      if (_anchorDragHoverStationId != InvalidStationId)
      {
         hoveredStationId = _anchorDragHoverStationId;
      }
      const std::string statusText = BuildHudText();
      LineId highlightLineId = _lineEditor.GetSelectedLineId();
      if (_trainDrag == TrainDrag::Yes)
      {
         highlightLineId = _dropTargetLineId;
      }
      else if (_lineDrag == LineDrag::Yes)
      {
         highlightLineId = _lineDragLineId;
      }
      else if (_anchorDrag == AnchorDrag::Yes)
      {
         highlightLineId = _anchorDragLineId;
      }

      LineDragPreview lineDragPreview;
      lineDragPreview.lineId = _lineDragLineId;
      lineDragPreview.segmentIndex = _lineDragSegmentIndex;
      lineDragPreview.hoverStationId = _lineDragHoverStationId;
      TerminusAnchorPreview anchorDragPreview;
      anchorDragPreview.lineId = _anchorDragLineId;
      anchorDragPreview.end = _anchorDragEnd;
      anchorDragPreview.hoverStationId = _anchorDragHoverStationId;
      _unconnectedScrollPixels = _renderer.ClampUnconnectedScroll(
         _world,
         _inspectedStationId,
         _inspectedTrainId,
         _inspectedLineId,
         _unconnectedScrollPixels);
      _renderer.Draw(
         _world,
         _lineEditor.GetDraftStationIds(),
         hoveredStationId,
         _inspectedStationId,
         _inspectedTrainId,
         _inspectedLineId,
         highlightLineId,
         statusText,
         _helpVisible,
         _pause,
         _timeScale,
         _trainDrag,
         _lineDrag,
         lineDragPreview,
         _anchorDrag,
         anchorDragPreview,
         _unconnectedScrollPixels,
         _lastMousePixel);
      _window.display();
   }
} // namespace MiniDb

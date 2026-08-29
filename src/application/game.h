/*!
 *\file game.h
 *\brief Window, input and real-time game loop.
 */

#ifndef GAME_H
#define GAME_H

#include <string>
#include <string_view>

#include <SFML/Graphics.hpp>

#include "application/main_menu.h"
#include "core/constants.h"
#include "core/result.h"
#include "core/types.h"
#include "input/line_editor.h"
#include "rendering/renderer.h"
#include "simulation/play_session_log.h"
#include "simulation/world.h"

namespace MiniDb
{
   enum class PanState : bool
   {
      No = false,
      Yes = true
   };

   enum class LineGrabPending : bool
   {
      No = false,
      Yes = true
   };

   enum class AnchorGrabPending : bool
   {
      No = false,
      Yes = true
   };

   enum class FullscreenMode : bool
   {
      No = false,
      Yes = true
   };

   class Game
   {
   public:
      Game(void);

      /*!
       *\brief Creates the window, loads data and prepares the simulation.
       *
       *\param[in] executablePath Path of the running executable, used to find data files.
       */
      Result Initialize(std::string_view executablePath);

      /*!
       *\brief Runs the main loop until the window is closed.
       */
      int32_t Run(void);

   private:
      Result LoadFont(void);
      Result LoadSimulationData(std::string_view executablePath);
      std::string ResolveDataFile(std::string_view executablePath, std::string_view fileName) const;
      bool FileExists(std::string_view filePath) const;
      void ProcessEvents(void);
      void HandleKeyPressed(const sf::Event::KeyPressed& keyPressed);
      void HandleTextEntered(const sf::Event::TextEntered& textEntered);
      void HandleMousePressed(const sf::Event::MouseButtonPressed& mousePressed);
      void HandleMouseReleased(const sf::Event::MouseButtonReleased& mouseReleased);
      void HandleMouseMoved(const sf::Event::MouseMoved& mouseMoved);
      void HandleMouseWheel(const sf::Event::MouseWheelScrolled& mouseWheel);
      void ApplyMenuAction(MenuAction action);
      void StartNewGame(void);
      void ReturnToMenu(std::string_view sessionEndReason = "menu");
      void ResetPlayInput(void);
      void SlowDownSimulation(void);
      void SpeedUpSimulation(void);
      void ToggleFullscreen(void);
      void ConfigureWindow(void);
      void Update(float deltaSeconds);
      void UpdateKeyboardCamera(float deltaSeconds);
      void Render(void);
      std::string BuildHudText(void) const;
      void HandleActionResult(Result result);
      void HandleGameOver(void);
      void BeginPlaySessionLog(void);
      void EndPlaySessionLog(std::string_view reason);

      sf::RenderWindow _window;
      sf::Font _font;
      sf::Clock _clock;
      World _world;
      Renderer _renderer;
      MainMenu _mainMenu;
      LineEditor _lineEditor;
      PlaySessionLog _playSessionLog;
      std::string _logsDirectory;
      std::string _statusMessage;
      std::string _menuBannerMessage;
      float _statusMessageSeconds = 0.0f;
      AppScreen _appScreen = AppScreen::Menu;
      HasActiveGame _hasActiveGame = HasActiveGame::No;
      SimulationPause _pause = SimulationPause::No;
      float _timeScale = DefaultTimeScale;
      PanState _panState = PanState::No;
      HelpVisible _helpVisible = HelpVisible::No;
      TrainDrag _trainDrag = TrainDrag::No;
      LineDrag _lineDrag = LineDrag::No;
      LineGrabPending _lineGrabPending = LineGrabPending::No;
      AnchorGrabPending _anchorGrabPending = AnchorGrabPending::No;
      AnchorDrag _anchorDrag = AnchorDrag::No;
      LineId _dropTargetLineId = InvalidLineId;
      LineId _lineDragLineId = InvalidLineId;
      LineId _anchorDragLineId = InvalidLineId;
      LineEnd _anchorDragEnd = LineEnd::Front;
      uint32_t _lineDragSegmentIndex = InvalidIndex;
      StationId _lineDragHoverStationId = InvalidStationId;
      StationId _anchorDragHoverStationId = InvalidStationId;
      sf::Vector2i _lineDragStartPixel;
      sf::Vector2i _anchorDragStartPixel;
      float _unconnectedScrollPixels = 0.0f;
      sf::Vector2i _lastMousePixel;
      StationId _inspectedStationId = InvalidStationId;
      TrainId _inspectedTrainId = InvalidTrainId;
      LineId _inspectedLineId = InvalidLineId;
      FullscreenMode _fullscreenMode = FullscreenMode::No;
      sf::Vector2u _windowedSize = {DefaultWindowWidth, DefaultWindowHeight};
   };
} // namespace MiniDb

#endif // GAME_H

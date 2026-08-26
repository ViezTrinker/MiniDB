/*!
 *\file main_menu.h
 *\brief Start screen, settings, and new-game options.
 */

#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include <cstdint>
#include <string>

#include <SFML/Graphics.hpp>

#include "core/constants.h"
#include "simulation/world.h"

namespace MiniDb
{
   enum class AppScreen : uint8_t
   {
      Menu = 0,
      Playing = 1
   };

   enum class HasActiveGame : bool
   {
      No = false,
      Yes = true
   };

   enum class MenuAction : int8_t
   {
      None = 0,
      Start = 1,
      Resume = 2,
      Quit = 3
   };

   enum class SettingsNumberFocus : uint8_t
   {
      None = 0,
      StationCount = 1,
      TrainCapacity = 2
   };

   enum class MenuPage : uint8_t
   {
      Root = 0,
      Settings = 1
   };

   class MainMenu
   {
   public:
      MainMenu(void);

      /*!
       *\brief Binds the menu to a window and font.
       *
       *\param[in] pWindow Target window.
       *\param[in] pFont Font used for labels and buttons.
       */
      void Initialize(sf::RenderWindow* pWindow, sf::Font* pFont);

      /*!
       *\brief Draws the start menu or settings over the current default view.
       *
       *\param[in] hasActiveGame Whether Resume should be offered.
       *\param[in] catalogStationCount Cities available in the loaded catalog.
       *\param[in] cursorPixel Current mouse position in screen pixels.
       */
      void Draw(HasActiveGame hasActiveGame, uint32_t catalogStationCount, sf::Vector2i cursorPixel);

      /*!
       *\brief Handles a left click on the menu.
       *
       *\param[in] pixel Screen location.
       *\param[in] hasActiveGame Whether Resume is available.
       *\param[in] catalogStationCount Cities available in the loaded catalog.
       */
      MenuAction HandleClick(
         sf::Vector2i pixel,
         HasActiveGame hasActiveGame,
         uint32_t catalogStationCount);

      /*!
       *\brief Handles a key press while the menu is open.
       *
       *\param[in] keyPressed Key event.
       *\param[in] hasActiveGame Whether Resume is available.
       *\param[in] catalogStationCount Cities available in the loaded catalog.
       */
      MenuAction HandleKeyPressed(
         const sf::Event::KeyPressed& keyPressed,
         HasActiveGame hasActiveGame,
         uint32_t catalogStationCount);

      /*!
       *\brief Handles typed characters for focused number fields.
       *
       *\param[in] unicode UTF-32 code point.
       */
      void HandleTextEntered(char32_t unicode);

      /*!
       *\brief Station cap chosen for the next new game.
       */
      uint32_t GetSelectedMaxStationCount(void) const;

      /*!
       *\brief Sets the station-cap field shown in settings.
       *
       *\param[in] maxStationCount Cap to display and commit.
       *\param[in] catalogStationCount Catalog size used for clamping.
       */
      void SetSelectedMaxStationCount(uint32_t maxStationCount, uint32_t catalogStationCount);

      /*!
       *\brief Train capacity chosen for the next new game.
       */
      uint32_t GetTrainCapacity(void) const;

      /*!
       *\brief Sets the train-capacity field shown in settings.
       *
       *\param[in] trainCapacity Passengers per train.
       */
      void SetTrainCapacity(uint32_t trainCapacity);

      /*!
       *\brief Simulation speed multiplier chosen for the next new game.
       */
      float GetGameSpeed(void) const;

      /*!
       *\brief Sets the game-speed control shown in settings.
       *
       *\param[in] gameSpeed Speed multiplier (1 / 2 / 4 / 8 / 16).
       */
      void SetGameSpeed(float gameSpeed);

      /*!
       *\brief Whether the next new game uses a random station pool.
       */
      RandomPool GetRandomPool(void) const;

      /*!
       *\brief Whether the next new game shuffles spawn order.
       */
      RandomOrder GetRandomOrder(void) const;

      /*!
       *\brief Whether the next new game enables destination events.
       */
      EventsEnabled GetEventsEnabled(void) const;

      /*!
       *\brief Returns to the root menu page.
       */
      void ShowRootPage(void);

   private:
      struct RootBounds
      {
         sf::FloatRect panel;
         sf::FloatRect start;
         sf::FloatRect resume;
         sf::FloatRect settings;
         sf::FloatRect quit;
      };

      struct SettingsBounds
      {
         sf::FloatRect panel;
         sf::FloatRect value;
         sf::FloatRect trainCapacityValue;
         sf::FloatRect gameSpeed;
         sf::FloatRect randomPool;
         sf::FloatRect randomOrder;
         sf::FloatRect events;
         sf::FloatRect back;
      };

      RootBounds ComputeRootBounds(HasActiveGame hasActiveGame) const;
      SettingsBounds ComputeSettingsBounds(void) const;
      void CommitFocusedNumber(uint32_t catalogStationCount);
      void CancelFocusedNumberEdit(void);
      void CommitStationCount(uint32_t catalogStationCount);
      void CommitTrainCapacity(void);
      void CancelStationCountEdit(void);
      void CancelTrainCapacityEdit(void);
      void BeginStationCountEdit(void);
      void BeginTrainCapacityEdit(uint32_t catalogStationCount);
      void CycleGameSpeed(void);
      float SnapGameSpeed(float gameSpeed) const;
      uint32_t ParsedStationCount(void) const;
      uint32_t ParsedTrainCapacity(void) const;
      std::string FormatStationCountField(void) const;
      std::string FormatTrainCapacityField(void) const;
      std::string FormatGameSpeedLabel(void) const;
      bool ContainsPixel(const sf::FloatRect& bounds, sf::Vector2i pixel) const;
      void DrawRoot(HasActiveGame hasActiveGame, sf::Vector2i cursorPixel);
      void DrawSettings(uint32_t catalogStationCount, sf::Vector2i cursorPixel);
      MenuAction HandleRootClick(sf::Vector2i pixel, HasActiveGame hasActiveGame);
      MenuAction HandleSettingsClick(sf::Vector2i pixel, uint32_t catalogStationCount);
      MenuAction HandleRootKeyPressed(
         const sf::Event::KeyPressed& keyPressed,
         HasActiveGame hasActiveGame);
      MenuAction HandleSettingsKeyPressed(
         const sf::Event::KeyPressed& keyPressed,
         uint32_t catalogStationCount);

      sf::RenderWindow* _pWindow = nullptr;
      sf::Font* _pFont = nullptr;
      MenuPage _page = MenuPage::Root;
      uint32_t _committedStationCount = DefaultMaxStationCount;
      std::string _stationCountText;
      uint32_t _committedTrainCapacity = DefaultTrainCapacity;
      std::string _trainCapacityText;
      SettingsNumberFocus _numberFocus = SettingsNumberFocus::None;
      float _gameSpeed = DefaultTimeScale;
      RandomPool _randomPool = RandomPool::No;
      RandomOrder _randomOrder = RandomOrder::No;
      EventsEnabled _eventsEnabled = EventsEnabled::No;
   };
} // namespace MiniDb

#endif // MAIN_MENU_H

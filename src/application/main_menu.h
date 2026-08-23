/*!
 *\file main_menu.h
 *\brief Start screen for launching a game and choosing the station cap.
 */

#ifndef MAIN_MENU_H
#define MAIN_MENU_H

#include <cstdint>
#include <string>

#include <SFML/Graphics.hpp>

#include "core/constants.h"

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

   enum class StationLimitStep : int8_t
   {
      Decrease = -1,
      Increase = 1
   };

   enum class StationCountFocus : bool
   {
      No = false,
      Yes = true
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
       *\brief Draws the start menu over the current default view.
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
       *\brief Handles typed characters for the station-count field.
       *
       *\param[in] unicode UTF-32 code point.
       */
      void HandleTextEntered(char32_t unicode);

      /*!
       *\brief Station cap chosen for the next new game, or applied on Resume.
       */
      uint32_t GetSelectedMaxStationCount(void) const;

      /*!
       *\brief Sets the station-cap field shown on the menu.
       *
       *\param[in] maxStationCount Cap to display and commit.
       *\param[in] catalogStationCount Catalog size used for clamping.
       */
      void SetSelectedMaxStationCount(uint32_t maxStationCount, uint32_t catalogStationCount);

   private:
      struct MenuBounds
      {
         sf::FloatRect panel;
         sf::FloatRect decrease;
         sf::FloatRect value;
         sf::FloatRect increase;
         sf::FloatRect start;
         sf::FloatRect resume;
         sf::FloatRect quit;
      };

      MenuBounds ComputeBounds(HasActiveGame hasActiveGame) const;
      void CommitStationCount(uint32_t catalogStationCount);
      void CancelStationCountEdit(void);
      void AdjustStationCount(StationLimitStep step, uint32_t catalogStationCount);
      void BeginStationCountEdit(void);
      uint32_t ParsedStationCount(void) const;
      std::string FormatStationCountField(void) const;
      bool ContainsPixel(const sf::FloatRect& bounds, sf::Vector2i pixel) const;

      sf::RenderWindow* _pWindow = nullptr;
      sf::Font* _pFont = nullptr;
      uint32_t _committedStationCount = DefaultMaxStationCount;
      std::string _stationCountText;
      StationCountFocus _stationCountFocus = StationCountFocus::No;
   };
} // namespace MiniDb

#endif // MAIN_MENU_H

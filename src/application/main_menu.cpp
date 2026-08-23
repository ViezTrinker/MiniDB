/*!
 *\file main_menu.cpp
 *\brief Start screen, settings, and new-game options.
 */

#include "application/main_menu.h"

#include <sstream>
#include <string>
#include <string_view>

#include "core/constants.h"
#include "rendering/utf8_text.h"

namespace MiniDb
{
   namespace
   {
      constexpr float MenuPanelWidth = 440.0f;
      constexpr float MenuPanelPaddingBottom = 28.0f;
      constexpr float MenuTitleTop = 24.0f;
      constexpr float MenuSubtitleTop = 78.0f;
      constexpr float MenuRootFirstButtonTop = 130.0f;
      constexpr float MenuSettingsTitleTop = 24.0f;
      constexpr float MenuStationsLabelTop = 72.0f;
      constexpr float MenuStepperTop = 102.0f;
      constexpr float MenuHintTop = 152.0f;
      constexpr float MenuToggleFirstTop = 184.0f;
      constexpr float MenuToggleGap = 12.0f;
      constexpr float MenuBackTopExtra = 16.0f;
      constexpr float MenuButtonWidth = 280.0f;
      constexpr float MenuButtonHeight = 44.0f;
      constexpr float MenuButtonGap = 12.0f;
      constexpr float MenuStepSize = 40.0f;
      constexpr float MenuValueWidth = 148.0f;
      constexpr float MenuStepperGap = 8.0f;
      constexpr uint32_t MenuStationCountMaxDigits = 5;
      constexpr uint32_t MenuSettingsToggleCount = 3;

      enum class ButtonHover : bool
      {
         No = false,
         Yes = true
      };

      uint32_t ClampStationCount(uint32_t stationCount, uint32_t catalogStationCount)
      {
         uint32_t clamped = stationCount;
         if (clamped < MinimumStationCap)
         {
            clamped = MinimumStationCap;
         }
         if (catalogStationCount > 0 && clamped > catalogStationCount)
         {
            clamped = catalogStationCount;
         }

         return clamped;
      }

      uint32_t ParseDigitText(std::string_view text, uint32_t fallback)
      {
         if (text.empty())
         {
            return fallback;
         }

         uint32_t value = 0;
         for (char character : text)
         {
            if (character < '0' || character > '9')
            {
               return fallback;
            }

            const auto digit = static_cast<uint32_t>(character - '0');
            if (value > ((UnlimitedStationCount - digit) / 10u))
            {
               return fallback;
            }

            value = (value * 10u) + digit;
         }

         if (value == 0)
         {
            return fallback;
         }

         return value;
      }

      void DrawCenteredText(
         sf::RenderWindow& window,
         sf::Font& font,
         std::string_view text,
         float centerX,
         float top,
         unsigned int characterSize,
         sf::Color color)
      {
         sf::Text label(font, Utf8SfString(text), characterSize);
         const sf::FloatRect localBounds = label.getLocalBounds();
         label.setFillColor(color);
         label.setPosition({
            centerX - (localBounds.size.x * 0.5f) - localBounds.position.x,
            top
         });
         window.draw(label);
      }

      void DrawPanelButton(
         sf::RenderWindow& window,
         sf::Font& font,
         const sf::FloatRect& bounds,
         std::string_view label,
         ButtonHover hover)
      {
         sf::RectangleShape shape(bounds.size);
         shape.setPosition(bounds.position);
         if (hover == ButtonHover::Yes)
         {
            shape.setFillColor(sf::Color(50, 50, 50));
         }
         else
         {
            shape.setFillColor(sf::Color(35, 35, 35));
         }
         shape.setOutlineColor(sf::Color(20, 20, 20));
         shape.setOutlineThickness(1.0f);
         window.draw(shape);

         sf::Text text(font, Utf8SfString(label), 20);
         const sf::FloatRect textBounds = text.getLocalBounds();
         text.setFillColor(sf::Color(245, 245, 245));
         text.setPosition({
            bounds.position.x + ((bounds.size.x - textBounds.size.x) * 0.5f) - textBounds.position.x,
            bounds.position.y + ((bounds.size.y - textBounds.size.y) * 0.5f) - textBounds.position.y
         });
         window.draw(text);
      }

      std::string ToggleLabel(std::string_view name, bool enabled)
      {
         std::string label(name);
         label += ": ";
         if (enabled)
         {
            label += "On";
         }
         else
         {
            label += "Off";
         }

         return label;
      }
   } // namespace

   MainMenu::MainMenu(void) :
      _committedStationCount(DefaultMaxStationCount),
      _stationCountText(std::to_string(DefaultMaxStationCount))
   {
   }

   void MainMenu::Initialize(sf::RenderWindow* pWindow, sf::Font* pFont)
   {
      _pWindow = pWindow;
      _pFont = pFont;
   }

   MainMenu::RootBounds MainMenu::ComputeRootBounds(HasActiveGame hasActiveGame) const
   {
      RootBounds bounds;
      if (_pWindow == nullptr)
      {
         return bounds;
      }

      const auto windowWidth = static_cast<float>(_pWindow->getSize().x);
      const auto windowHeight = static_cast<float>(_pWindow->getSize().y);
      uint32_t actionButtonCount = 3;
      if (hasActiveGame == HasActiveGame::Yes)
      {
         actionButtonCount = 4;
      }

      const float actionBlockHeight =
         (static_cast<float>(actionButtonCount) * MenuButtonHeight) +
         (static_cast<float>(actionButtonCount - 1) * MenuButtonGap);
      const float panelHeight = MenuRootFirstButtonTop + actionBlockHeight + MenuPanelPaddingBottom;
      const float panelLeft = (windowWidth - MenuPanelWidth) * 0.5f;
      const float panelTop = (windowHeight - panelHeight) * 0.5f;
      bounds.panel = sf::FloatRect({panelLeft, panelTop}, {MenuPanelWidth, panelHeight});

      const float buttonLeft = panelLeft + ((MenuPanelWidth - MenuButtonWidth) * 0.5f);
      float actionTop = panelTop + MenuRootFirstButtonTop;
      bounds.start = sf::FloatRect({buttonLeft, actionTop}, {MenuButtonWidth, MenuButtonHeight});
      actionTop += MenuButtonHeight + MenuButtonGap;
      bounds.resume = sf::FloatRect({buttonLeft, actionTop}, {MenuButtonWidth, MenuButtonHeight});
      if (hasActiveGame == HasActiveGame::Yes)
      {
         actionTop += MenuButtonHeight + MenuButtonGap;
      }
      bounds.settings = sf::FloatRect({buttonLeft, actionTop}, {MenuButtonWidth, MenuButtonHeight});
      actionTop += MenuButtonHeight + MenuButtonGap;
      bounds.quit = sf::FloatRect({buttonLeft, actionTop}, {MenuButtonWidth, MenuButtonHeight});
      return bounds;
   }

   MainMenu::SettingsBounds MainMenu::ComputeSettingsBounds(void) const
   {
      SettingsBounds bounds;
      if (_pWindow == nullptr)
      {
         return bounds;
      }

      const auto windowWidth = static_cast<float>(_pWindow->getSize().x);
      const auto windowHeight = static_cast<float>(_pWindow->getSize().y);
      const float toggleBlockHeight =
         (static_cast<float>(MenuSettingsToggleCount) * MenuButtonHeight) +
         (static_cast<float>(MenuSettingsToggleCount - 1) * MenuToggleGap);
      const float panelHeight =
         MenuToggleFirstTop + toggleBlockHeight + MenuBackTopExtra + MenuButtonHeight +
         MenuPanelPaddingBottom;
      const float panelLeft = (windowWidth - MenuPanelWidth) * 0.5f;
      const float panelTop = (windowHeight - panelHeight) * 0.5f;
      bounds.panel = sf::FloatRect({panelLeft, panelTop}, {MenuPanelWidth, panelHeight});

      const float buttonLeft = panelLeft + ((MenuPanelWidth - MenuButtonWidth) * 0.5f);
      const float stepRowWidth = MenuStepSize + MenuStepperGap + MenuValueWidth + MenuStepperGap + MenuStepSize;
      const float stepRowLeft = panelLeft + ((MenuPanelWidth - stepRowWidth) * 0.5f);
      const float stepTop = panelTop + MenuStepperTop;
      bounds.decrease = sf::FloatRect({stepRowLeft, stepTop}, {MenuStepSize, MenuStepSize});
      bounds.value = sf::FloatRect(
         {stepRowLeft + MenuStepSize + MenuStepperGap, stepTop},
         {MenuValueWidth, MenuStepSize});
      bounds.increase = sf::FloatRect(
         {stepRowLeft + MenuStepSize + MenuStepperGap + MenuValueWidth + MenuStepperGap, stepTop},
         {MenuStepSize, MenuStepSize});

      float toggleTop = panelTop + MenuToggleFirstTop;
      bounds.randomPool = sf::FloatRect({buttonLeft, toggleTop}, {MenuButtonWidth, MenuButtonHeight});
      toggleTop += MenuButtonHeight + MenuToggleGap;
      bounds.randomOrder = sf::FloatRect({buttonLeft, toggleTop}, {MenuButtonWidth, MenuButtonHeight});
      toggleTop += MenuButtonHeight + MenuToggleGap;
      bounds.events = sf::FloatRect({buttonLeft, toggleTop}, {MenuButtonWidth, MenuButtonHeight});
      toggleTop += MenuButtonHeight + MenuBackTopExtra;
      bounds.back = sf::FloatRect({buttonLeft, toggleTop}, {MenuButtonWidth, MenuButtonHeight});
      return bounds;
   }

   uint32_t MainMenu::ParsedStationCount(void) const
   {
      return ParseDigitText(_stationCountText, _committedStationCount);
   }

   void MainMenu::CommitStationCount(uint32_t catalogStationCount)
   {
      _committedStationCount = ClampStationCount(ParsedStationCount(), catalogStationCount);
      _stationCountText = std::to_string(_committedStationCount);
      _stationCountFocus = StationCountFocus::No;
   }

   void MainMenu::CancelStationCountEdit(void)
   {
      _stationCountText = std::to_string(_committedStationCount);
      _stationCountFocus = StationCountFocus::No;
   }

   void MainMenu::BeginStationCountEdit(void)
   {
      _stationCountFocus = StationCountFocus::Yes;
      _stationCountText = std::to_string(_committedStationCount);
   }

   void MainMenu::AdjustStationCount(StationLimitStep step, uint32_t catalogStationCount)
   {
      CommitStationCount(catalogStationCount);
      uint32_t nextCount = _committedStationCount;
      if (step == StationLimitStep::Decrease)
      {
         if (nextCount > (MinimumStationCap + StationCapStep))
         {
            nextCount -= StationCapStep;
         }
         else
         {
            nextCount = MinimumStationCap;
         }
      }
      else
      {
         nextCount += StationCapStep;
      }

      _committedStationCount = ClampStationCount(nextCount, catalogStationCount);
      _stationCountText = std::to_string(_committedStationCount);
   }

   std::string MainMenu::FormatStationCountField(void) const
   {
      if (_stationCountFocus == StationCountFocus::Yes)
      {
         return _stationCountText + "|";
      }

      return _stationCountText;
   }

   bool MainMenu::ContainsPixel(const sf::FloatRect& bounds, sf::Vector2i pixel) const
   {
      const sf::Vector2f point(static_cast<float>(pixel.x), static_cast<float>(pixel.y));
      return bounds.contains(point);
   }

   uint32_t MainMenu::GetSelectedMaxStationCount(void) const
   {
      return ParsedStationCount();
   }

   void MainMenu::SetSelectedMaxStationCount(uint32_t maxStationCount, uint32_t catalogStationCount)
   {
      _committedStationCount = ClampStationCount(maxStationCount, catalogStationCount);
      _stationCountText = std::to_string(_committedStationCount);
      _stationCountFocus = StationCountFocus::No;
   }

   RandomPool MainMenu::GetRandomPool(void) const
   {
      return _randomPool;
   }

   RandomOrder MainMenu::GetRandomOrder(void) const
   {
      return _randomOrder;
   }

   EventsEnabled MainMenu::GetEventsEnabled(void) const
   {
      return _eventsEnabled;
   }

   void MainMenu::ShowRootPage(void)
   {
      CancelStationCountEdit();
      _page = MenuPage::Root;
   }

   void MainMenu::HandleTextEntered(char32_t unicode)
   {
      if (_page != MenuPage::Settings || _stationCountFocus == StationCountFocus::No)
      {
         return;
      }
      if (unicode < U'0' || unicode > U'9')
      {
         return;
      }
      if (_stationCountText.size() >= MenuStationCountMaxDigits)
      {
         return;
      }

      _stationCountText.push_back(static_cast<char>(unicode));
   }

   void MainMenu::DrawRoot(HasActiveGame hasActiveGame, sf::Vector2i cursorPixel)
   {
      const RootBounds bounds = ComputeRootBounds(hasActiveGame);
      sf::RectangleShape panel(bounds.panel.size);
      panel.setPosition(bounds.panel.position);
      panel.setFillColor(sf::Color(255, 252, 245, 235));
      panel.setOutlineColor(sf::Color(120, 110, 100));
      panel.setOutlineThickness(1.0f);
      _pWindow->draw(panel);

      const float centerX = bounds.panel.position.x + (bounds.panel.size.x * 0.5f);
      const float panelTop = bounds.panel.position.y;
      DrawCenteredText(*_pWindow, *_pFont, "MiniDB", centerX, panelTop + MenuTitleTop, 42, sf::Color(35, 35, 35));
      DrawCenteredText(
         *_pWindow,
         *_pFont,
         "Real-time railway for Germany",
         centerX,
         panelTop + MenuSubtitleTop,
         16,
         sf::Color(80, 75, 70));

      ButtonHover startHover = ButtonHover::No;
      if (ContainsPixel(bounds.start, cursorPixel))
      {
         startHover = ButtonHover::Yes;
      }
      DrawPanelButton(*_pWindow, *_pFont, bounds.start, "Start", startHover);

      if (hasActiveGame == HasActiveGame::Yes)
      {
         ButtonHover resumeHover = ButtonHover::No;
         if (ContainsPixel(bounds.resume, cursorPixel))
         {
            resumeHover = ButtonHover::Yes;
         }
         DrawPanelButton(*_pWindow, *_pFont, bounds.resume, "Resume", resumeHover);
      }

      ButtonHover settingsHover = ButtonHover::No;
      if (ContainsPixel(bounds.settings, cursorPixel))
      {
         settingsHover = ButtonHover::Yes;
      }
      DrawPanelButton(*_pWindow, *_pFont, bounds.settings, "Settings", settingsHover);

      ButtonHover quitHover = ButtonHover::No;
      if (ContainsPixel(bounds.quit, cursorPixel))
      {
         quitHover = ButtonHover::Yes;
      }
      DrawPanelButton(*_pWindow, *_pFont, bounds.quit, "Quit", quitHover);
   }

   void MainMenu::DrawSettings(uint32_t catalogStationCount, sf::Vector2i cursorPixel)
   {
      const SettingsBounds bounds = ComputeSettingsBounds();
      sf::RectangleShape panel(bounds.panel.size);
      panel.setPosition(bounds.panel.position);
      panel.setFillColor(sf::Color(255, 252, 245, 235));
      panel.setOutlineColor(sf::Color(120, 110, 100));
      panel.setOutlineThickness(1.0f);
      _pWindow->draw(panel);

      const float centerX = bounds.panel.position.x + (bounds.panel.size.x * 0.5f);
      const float panelTop = bounds.panel.position.y;
      DrawCenteredText(
         *_pWindow,
         *_pFont,
         "Settings",
         centerX,
         panelTop + MenuSettingsTitleTop,
         32,
         sf::Color(35, 35, 35));
      DrawCenteredText(
         *_pWindow,
         *_pFont,
         "Stations",
         centerX,
         panelTop + MenuStationsLabelTop,
         18,
         sf::Color(35, 35, 35));

      ButtonHover decreaseHover = ButtonHover::No;
      if (ContainsPixel(bounds.decrease, cursorPixel))
      {
         decreaseHover = ButtonHover::Yes;
      }
      DrawPanelButton(*_pWindow, *_pFont, bounds.decrease, "<", decreaseHover);

      ButtonHover increaseHover = ButtonHover::No;
      if (ContainsPixel(bounds.increase, cursorPixel))
      {
         increaseHover = ButtonHover::Yes;
      }
      DrawPanelButton(*_pWindow, *_pFont, bounds.increase, ">", increaseHover);

      sf::RectangleShape valueField(bounds.value.size);
      valueField.setPosition(bounds.value.position);
      valueField.setFillColor(sf::Color(255, 255, 255));
      if (_stationCountFocus == StationCountFocus::Yes)
      {
         valueField.setOutlineColor(sf::Color(40, 90, 160));
         valueField.setOutlineThickness(2.0f);
      }
      else
      {
         valueField.setOutlineColor(sf::Color(120, 110, 100));
         valueField.setOutlineThickness(1.0f);
      }
      _pWindow->draw(valueField);
      DrawCenteredText(
         *_pWindow,
         *_pFont,
         FormatStationCountField(),
         centerX,
         bounds.value.position.y + 6.0f,
         22,
         sf::Color(35, 35, 35));

      std::ostringstream hintStream;
      hintStream << "Type a number, or use < >. Catalog " << catalogStationCount << ".";
      DrawCenteredText(
         *_pWindow,
         *_pFont,
         hintStream.str(),
         centerX,
         panelTop + MenuHintTop,
         13,
         sf::Color(90, 85, 80));

      ButtonHover poolHover = ButtonHover::No;
      if (ContainsPixel(bounds.randomPool, cursorPixel))
      {
         poolHover = ButtonHover::Yes;
      }
      DrawPanelButton(
         *_pWindow,
         *_pFont,
         bounds.randomPool,
         ToggleLabel("Random pool", _randomPool == RandomPool::Yes),
         poolHover);

      ButtonHover orderHover = ButtonHover::No;
      if (ContainsPixel(bounds.randomOrder, cursorPixel))
      {
         orderHover = ButtonHover::Yes;
      }
      DrawPanelButton(
         *_pWindow,
         *_pFont,
         bounds.randomOrder,
         ToggleLabel("Random order", _randomOrder == RandomOrder::Yes),
         orderHover);

      ButtonHover eventsHover = ButtonHover::No;
      if (ContainsPixel(bounds.events, cursorPixel))
      {
         eventsHover = ButtonHover::Yes;
      }
      DrawPanelButton(
         *_pWindow,
         *_pFont,
         bounds.events,
         ToggleLabel("Events", _eventsEnabled == EventsEnabled::Yes),
         eventsHover);

      ButtonHover backHover = ButtonHover::No;
      if (ContainsPixel(bounds.back, cursorPixel))
      {
         backHover = ButtonHover::Yes;
      }
      DrawPanelButton(*_pWindow, *_pFont, bounds.back, "Back", backHover);
   }

   void MainMenu::Draw(HasActiveGame hasActiveGame, uint32_t catalogStationCount, sf::Vector2i cursorPixel)
   {
      if (_pWindow == nullptr || _pFont == nullptr)
      {
         return;
      }

      if (_page == MenuPage::Settings)
      {
         DrawSettings(catalogStationCount, cursorPixel);
         return;
      }

      DrawRoot(hasActiveGame, cursorPixel);
   }

   MenuAction MainMenu::HandleRootClick(sf::Vector2i pixel, HasActiveGame hasActiveGame)
   {
      const RootBounds bounds = ComputeRootBounds(hasActiveGame);
      if (ContainsPixel(bounds.start, pixel))
      {
         return MenuAction::Start;
      }
      if (hasActiveGame == HasActiveGame::Yes && ContainsPixel(bounds.resume, pixel))
      {
         return MenuAction::Resume;
      }
      if (ContainsPixel(bounds.settings, pixel))
      {
         _page = MenuPage::Settings;
         return MenuAction::None;
      }
      if (ContainsPixel(bounds.quit, pixel))
      {
         return MenuAction::Quit;
      }

      return MenuAction::None;
   }

   MenuAction MainMenu::HandleSettingsClick(sf::Vector2i pixel, uint32_t catalogStationCount)
   {
      const SettingsBounds bounds = ComputeSettingsBounds();
      if (ContainsPixel(bounds.value, pixel))
      {
         BeginStationCountEdit();
         return MenuAction::None;
      }

      CommitStationCount(catalogStationCount);
      if (ContainsPixel(bounds.decrease, pixel))
      {
         AdjustStationCount(StationLimitStep::Decrease, catalogStationCount);
         return MenuAction::None;
      }
      if (ContainsPixel(bounds.increase, pixel))
      {
         AdjustStationCount(StationLimitStep::Increase, catalogStationCount);
         return MenuAction::None;
      }
      if (ContainsPixel(bounds.randomPool, pixel))
      {
         if (_randomPool == RandomPool::Yes)
         {
            _randomPool = RandomPool::No;
         }
         else
         {
            _randomPool = RandomPool::Yes;
         }
         return MenuAction::None;
      }
      if (ContainsPixel(bounds.randomOrder, pixel))
      {
         if (_randomOrder == RandomOrder::Yes)
         {
            _randomOrder = RandomOrder::No;
         }
         else
         {
            _randomOrder = RandomOrder::Yes;
         }
         return MenuAction::None;
      }
      if (ContainsPixel(bounds.events, pixel))
      {
         if (_eventsEnabled == EventsEnabled::Yes)
         {
            _eventsEnabled = EventsEnabled::No;
         }
         else
         {
            _eventsEnabled = EventsEnabled::Yes;
         }
         return MenuAction::None;
      }
      if (ContainsPixel(bounds.back, pixel))
      {
         ShowRootPage();
         return MenuAction::None;
      }

      return MenuAction::None;
   }

   MenuAction MainMenu::HandleClick(
      sf::Vector2i pixel,
      HasActiveGame hasActiveGame,
      uint32_t catalogStationCount)
   {
      if (_page == MenuPage::Settings)
      {
         return HandleSettingsClick(pixel, catalogStationCount);
      }

      return HandleRootClick(pixel, hasActiveGame);
   }

   MenuAction MainMenu::HandleRootKeyPressed(
      const sf::Event::KeyPressed& keyPressed,
      HasActiveGame hasActiveGame)
   {
      if (keyPressed.code == sf::Keyboard::Key::Enter)
      {
         if (hasActiveGame == HasActiveGame::Yes)
         {
            return MenuAction::Resume;
         }
         return MenuAction::Start;
      }
      if (keyPressed.code == sf::Keyboard::Key::Escape)
      {
         if (hasActiveGame == HasActiveGame::Yes)
         {
            return MenuAction::Resume;
         }

         return MenuAction::Quit;
      }

      return MenuAction::None;
   }

   MenuAction MainMenu::HandleSettingsKeyPressed(
      const sf::Event::KeyPressed& keyPressed,
      uint32_t catalogStationCount)
   {
      if (_stationCountFocus == StationCountFocus::Yes)
      {
         if (keyPressed.code == sf::Keyboard::Key::Backspace)
         {
            if (!_stationCountText.empty())
            {
               _stationCountText.pop_back();
            }
            return MenuAction::None;
         }
         if (keyPressed.code == sf::Keyboard::Key::Enter)
         {
            CommitStationCount(catalogStationCount);
            return MenuAction::None;
         }
         if (keyPressed.code == sf::Keyboard::Key::Escape)
         {
            CancelStationCountEdit();
            return MenuAction::None;
         }

         return MenuAction::None;
      }

      if (keyPressed.code == sf::Keyboard::Key::Left)
      {
         AdjustStationCount(StationLimitStep::Decrease, catalogStationCount);
         return MenuAction::None;
      }
      if (keyPressed.code == sf::Keyboard::Key::Right)
      {
         AdjustStationCount(StationLimitStep::Increase, catalogStationCount);
         return MenuAction::None;
      }
      if (keyPressed.code == sf::Keyboard::Key::Escape)
      {
         ShowRootPage();
         return MenuAction::None;
      }

      return MenuAction::None;
   }

   MenuAction MainMenu::HandleKeyPressed(
      const sf::Event::KeyPressed& keyPressed,
      HasActiveGame hasActiveGame,
      uint32_t catalogStationCount)
   {
      if (_page == MenuPage::Settings)
      {
         return HandleSettingsKeyPressed(keyPressed, catalogStationCount);
      }

      return HandleRootKeyPressed(keyPressed, hasActiveGame);
   }
} // namespace MiniDb

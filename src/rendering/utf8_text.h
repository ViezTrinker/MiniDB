/*!
 *\file utf8_text.h
 *\brief Converts UTF-8 text to SFML strings.
 */

#ifndef UTF8_TEXT_H
#define UTF8_TEXT_H

#include <string_view>

#include <SFML/System/String.hpp>

namespace MiniDb
{
   /*!
    *\brief Interprets the bytes as UTF-8 for SFML drawing.
    *
    *\param[in] text UTF-8 text.
    */
   inline sf::String Utf8SfString(std::string_view text)
   {
      return sf::String::fromUtf8(text.begin(), text.end());
   }
} // namespace MiniDb

#endif // UTF8_TEXT_H

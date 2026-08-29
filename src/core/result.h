/*!
 *\file result.h
 *\brief Result codes and helpers used across MiniDB.
 */

#ifndef RESULT_H
#define RESULT_H

#include <cstdint>

namespace MiniDb
{
   enum class Result : int8_t
   {
      InsufficientFunds = -9,
      StationNotFound = -8,
      LineTooShort = -7,
      DuplicateStation = -6,
      NoDraftLine = -5,
      AlreadyDrafting = -4,
      InvalidArgument = -3,
      FileError = -2,
      Error = -1,
      Ok = 0
   };

   /*!
    *\brief Returns true when the result represents an error.
    *
    *\param[in] result Result to inspect.
    */
   inline bool IsErr(Result result)
   {
      return static_cast<int8_t>(result) < 0;
   }

   /*!
    *\brief Returns true when the result represents success.
    *
    *\param[in] result Result to inspect.
    */
   inline bool IsOk(Result result)
   {
      return result == Result::Ok;
   }

   /*!
    *\brief Returns true when the result is a non-error message.
    *
    *\param[in] result Result to inspect.
    */
   inline bool IsMsg(Result result)
   {
      return static_cast<int8_t>(result) > 0;
   }
} // namespace MiniDb

#endif // RESULT_H

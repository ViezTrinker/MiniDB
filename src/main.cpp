/*!
 *\file main.cpp
 *\brief MiniDB application entry point.
 */

#include "application/game.h"

#include <string_view>

int main(int argumentCount, char** pArgumentValues)
{
   MiniDb::Game game;
   std::string_view executablePath = "MiniDB";
   if (argumentCount > 0 && pArgumentValues != nullptr && pArgumentValues[0] != nullptr)
   {
      executablePath = pArgumentValues[0];
   }

   const MiniDb::Result result = game.Initialize(executablePath);
   if (MiniDb::IsErr(result))
   {
      return 1;
   }

   return game.Run();
}

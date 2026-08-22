/*!
 *\file line_editor_test.cpp
 *\brief Tests for deleting a selected finished line.
 */

#include <gtest/gtest.h>

#include "input/line_editor.h"
#include "simulation/world.h"

namespace
{
   const char CatalogJson[] =
      "{"
      "\"stations\":["
      "{\"id\":0,\"cityName\":\"Berlin\",\"stationName\":\"Berlin\","
      "\"latitude\":52.52,\"longitude\":13.40,\"population\":3600000},"
      "{\"id\":1,\"cityName\":\"Hamburg\",\"stationName\":\"Hamburg\","
      "\"latitude\":53.55,\"longitude\":10.00,\"population\":1800000},"
      "{\"id\":2,\"cityName\":\"Munich\",\"stationName\":\"Munich\","
      "\"latitude\":48.14,\"longitude\":11.58,\"population\":1500000},"
      "{\"id\":3,\"cityName\":\"Alpha\",\"stationName\":\"Alpha\","
      "\"latitude\":51.00,\"longitude\":10.00,\"population\":100000},"
      "{\"id\":4,\"cityName\":\"Beta\",\"stationName\":\"Beta\","
      "\"latitude\":51.01,\"longitude\":10.00,\"population\":90000}"
      "]"
      "}";
} // namespace

TEST(LineEditorTest, DeleteSelectedLineRemovesLine)
{
   MiniDb::World world(7);
   world.SetPassengerAutoSpawn(MiniDb::PassengerAutoSpawn::No);
   ASSERT_TRUE(MiniDb::IsOk(world.LoadCatalogFromString(CatalogJson)));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));
   ASSERT_TRUE(MiniDb::IsOk(world.SpawnNextStation()));

   MiniDb::StationIdList stationIds;
   stationIds.push_back(3);
   stationIds.push_back(4);
   MiniDb::LineId lineId = MiniDb::InvalidLineId;
   ASSERT_TRUE(MiniDb::IsOk(world.AddLine(stationIds, lineId)));

   MiniDb::LineEditor editor;
   editor.SelectLine(lineId);
   ASSERT_TRUE(MiniDb::IsOk(editor.DeleteSelectedLine(world)));
   EXPECT_EQ(world.GetNetwork().FindLine(lineId), nullptr);
   EXPECT_EQ(editor.GetSelectedLineId(), MiniDb::InvalidLineId);
   EXPECT_FALSE(editor.IsDrafting());
}

TEST(LineEditorTest, DeleteSelectedLineRejectsWhenNothingSelected)
{
   MiniDb::World world(1);
   MiniDb::LineEditor editor;
   EXPECT_TRUE(MiniDb::IsErr(editor.DeleteSelectedLine(world)));
}

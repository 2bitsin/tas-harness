#include "tash/trace/sqlite-export.hpp"

#include "tash/trace/_scratch-path.hpp"
#include "tash/trace/format.hpp"
#include "tash/trace/header.hpp"
#include "tash/trace/reader.hpp"
#include "tash/trace/record.hpp"
#include "tash/trace/writer.hpp"

#include <gtest/gtest.h>
#include <sqlite3.h>

#include <cstdint>
#include <filesystem>
#include <string>

namespace tash::trace
{
  namespace
  {
    using detail::scratch_path::ScratchPath;

    constexpr std::int64_t CREATED{ 1'789'300'800'123'456'789 };

    auto Ask(std::filesystem::path const& database, std::string const& query)
      -> std::string
    {
      sqlite3* opened{ nullptr };
      if (sqlite3_open(database.string().c_str(), &opened) != SQLITE_OK)
        return "<cannot open>";
      sqlite3_stmt* statement{ nullptr };
      std::string answer{ "<no row>" };
      if (sqlite3_prepare_v2(opened, query.c_str(), -1, &statement, nullptr)
            == SQLITE_OK
          && sqlite3_step(statement) == SQLITE_ROW)
      {
        auto const* text{ sqlite3_column_text(statement, 0) };
        answer = text == nullptr
          ? std::string{ }
          : std::string{ reinterpret_cast<char const*>(text) };
      }
      sqlite3_finalize(statement);
      sqlite3_close(opened);
      return answer;
    }

    auto WriteSample(std::filesystem::path const& path) -> void
    {
      auto writing{ Writer::Open(
        path, Header{ FORMAT_VERSION, CREATED, "tash 0.1" }) };
      ASSERT_TRUE(writing.has_value()) << writing.error();
      EXPECT_TRUE(writing->Write(FrameRecord{
        1, 16'666'667, 0xdead'beef'0bad'c0deull, 2, 3, 0.25 }));
      EXPECT_TRUE(writing->Write(FrameRecord{ 2, 33'333'334, 4, 5, 6, 0.5 }));
      EXPECT_TRUE(writing->Write(InputRecord{ 2, 0, 0x30u }));
      EXPECT_TRUE(writing->Write(WatchRecord{ 2, 7, -4242 }));
      EXPECT_TRUE(writing->Write(DecisionRecord{ 3, "agent",
                                                 "play the tape" }));
      EXPECT_TRUE(writing->Write(VerdictRecord{ 4, "level", true,
                                                "reached" }));
      EXPECT_TRUE(writing->Write(TriggerRecord{ 5, "spike", "over 0.4" }));
      EXPECT_TRUE(writing->Write(MarkRecord{ 6, "first column",
                                             "decision" }));
      EXPECT_TRUE(writing->Write(RestoreRecord{ 7, 2, "search-0" }));
      EXPECT_TRUE(writing->Write(ResetRecord{ 8 }));
    }
  }

  TEST(TraceSqliteExport, OneRowPerRecordAndTheHeader)
  {
    ScratchPath const area{ };
    auto const path{ area.File("run.bin") };
    auto const database{ area.File("run.db") };
    WriteSample(path);

    auto reading{ Reader::Open(path) };
    ASSERT_TRUE(reading.has_value()) << reading.error();
    auto const rows{ ExportToSqlite(*reading, database) };
    ASSERT_TRUE(rows.has_value()) << rows.error();
    EXPECT_EQ(*rows, 10u);

    EXPECT_EQ(Ask(database, "SELECT count(*) FROM frames"), "2");
    EXPECT_EQ(Ask(database, "SELECT count(*) FROM inputs"), "1");
    EXPECT_EQ(Ask(database, "SELECT count(*) FROM watches"), "1");
    EXPECT_EQ(Ask(database, "SELECT count(*) FROM decisions"), "1");
    EXPECT_EQ(Ask(database, "SELECT count(*) FROM verdicts"), "1");
    EXPECT_EQ(Ask(database, "SELECT count(*) FROM triggers"), "1");
    EXPECT_EQ(Ask(database, "SELECT count(*) FROM marks"), "1");
    EXPECT_EQ(Ask(database, "SELECT count(*) FROM restores"), "1");
    EXPECT_EQ(Ask(database, "SELECT count(*) FROM resets"), "1");

    EXPECT_EQ(Ask(database, "SELECT value FROM meta WHERE key='producer'"),
              "tash 0.1");
    EXPECT_EQ(Ask(database,
                  "SELECT value FROM meta WHERE key='format_version'"), "5");
    EXPECT_EQ(Ask(database, "SELECT value FROM meta WHERE key='creation_time'"),
              "1789300800123456789");
    EXPECT_EQ(Ask(database, "SELECT value FROM meta WHERE key='truncated'"),
              "0");
  }

  TEST(TraceSqliteExport, ValuesSurviveTheRoundTrip)
  {
    ScratchPath const area{ };
    auto const path{ area.File("run.bin") };
    auto const database{ area.File("run.db") };
    WriteSample(path);

    auto reading{ Reader::Open(path) };
    ASSERT_TRUE(reading.has_value()) << reading.error();
    ASSERT_TRUE(ExportToSqlite(*reading, database).has_value());

    // SQLite integers are signed, so a hash with the top bit set reads back
    // negative; printf('%016x') is what a query does about it.
    EXPECT_EQ(Ask(database, "SELECT printf('%016x', hash_exact) FROM frames "
                            "WHERE frame=1"), "deadbeef0badc0de");
    EXPECT_EQ(Ask(database, "SELECT change_amount FROM frames WHERE frame=2"),
              "0.5");
    EXPECT_EQ(Ask(database, "SELECT value FROM watches"), "-4242");
    EXPECT_EQ(Ask(database, "SELECT passed FROM verdicts"), "1");
    EXPECT_EQ(Ask(database, "SELECT text FROM marks"), "first column");
    EXPECT_EQ(Ask(database, R"(SELECT "group" FROM marks)"), "decision");
    EXPECT_EQ(Ask(database, "SELECT actor FROM decisions"), "agent");
    EXPECT_EQ(Ask(database, R"(SELECT "to" FROM restores)"), "2");
    EXPECT_EQ(Ask(database, "SELECT frame FROM resets"), "8");
  }

  TEST(TraceSqliteExport, RefusesToOverwriteAnExistingDatabase)
  {
    ScratchPath const area{ };
    auto const path{ area.File("run.bin") };
    auto const database{ area.File("run.db") };
    WriteSample(path);

    auto first{ Reader::Open(path) };
    ASSERT_TRUE(first.has_value()) << first.error();
    ASSERT_TRUE(ExportToSqlite(*first, database).has_value());

    auto again{ Reader::Open(path) };
    ASSERT_TRUE(again.has_value()) << again.error();
    auto const rows{ ExportToSqlite(*again, database) };
    ASSERT_FALSE(rows.has_value());
    EXPECT_NE(rows.error().find("already exists"), std::string::npos);
  }
}

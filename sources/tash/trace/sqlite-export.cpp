#include "tash/trace/sqlite-export.hpp"

#include "tash/trace/record.hpp"

#include <sqlite3.h>

#include <cstdint>
#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace tash::trace::detail::sqlite_export
{
  using utilities::Result;
  using utilities::Refused;

  namespace
  {
    constexpr std::string_view SCHEMA{ R"(
CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);
CREATE TABLE frames (frame INTEGER NOT NULL,
                     harness_time INTEGER NOT NULL,
                     hash_exact INTEGER NOT NULL,
                     hash_difference INTEGER NOT NULL,
                     hash_perceptual INTEGER NOT NULL,
                     change_amount REAL NOT NULL);
CREATE TABLE inputs (frame INTEGER NOT NULL,
                     port INTEGER NOT NULL,
                     pad INTEGER NOT NULL);
CREATE TABLE watches (frame INTEGER NOT NULL,
                      watch INTEGER NOT NULL,
                      value INTEGER NOT NULL);
CREATE TABLE decisions (frame INTEGER NOT NULL,
                        actor TEXT NOT NULL,
                        text TEXT NOT NULL);
CREATE TABLE verdicts (frame INTEGER NOT NULL,
                       name TEXT NOT NULL,
                       passed INTEGER NOT NULL,
                       text TEXT NOT NULL);
CREATE TABLE triggers (frame INTEGER NOT NULL,
                       name TEXT NOT NULL,
                       text TEXT NOT NULL);
CREATE TABLE marks (frame INTEGER NOT NULL,
                    text TEXT NOT NULL,
                    "group" TEXT NOT NULL);
CREATE TABLE restores (frame INTEGER NOT NULL,
                       "to" INTEGER NOT NULL,
                       name TEXT NOT NULL);
CREATE TABLE resets (frame INTEGER NOT NULL);
)" };

    constexpr std::string_view INDEXES{ R"(
CREATE INDEX frames_frame ON frames(frame);
CREATE INDEX inputs_frame ON inputs(frame);
CREATE INDEX watches_frame ON watches(frame);
CREATE INDEX decisions_frame ON decisions(frame);
CREATE INDEX verdicts_frame ON verdicts(frame);
CREATE INDEX triggers_frame ON triggers(frame);
CREATE INDEX marks_frame ON marks(frame);
CREATE INDEX restores_frame ON restores(frame);
CREATE INDEX resets_frame ON resets(frame);
)" };

    struct DatabaseCloser
    {
      auto operator () (sqlite3* database) const noexcept -> void
      { sqlite3_close(database); }
    };

    struct StatementFinalizer
    {
      auto operator () (sqlite3_stmt* statement) const noexcept -> void
      { sqlite3_finalize(statement); }
    };

    using DatabaseHandle  = std::unique_ptr<sqlite3, DatabaseCloser>;
    using StatementHandle = std::unique_ptr<sqlite3_stmt, StatementFinalizer>;

    [[nodiscard]] constexpr auto InsertFor(record::Kind kind)
      -> std::string_view
    {
      switch (kind)
      {
        case record::Kind::FRAME:
          return "INSERT INTO frames VALUES (?,?,?,?,?,?)";
        case record::Kind::INPUT:
          return "INSERT INTO inputs VALUES (?,?,?)";
        case record::Kind::WATCH:
          return "INSERT INTO watches VALUES (?,?,?)";
        case record::Kind::DECISION:
          return "INSERT INTO decisions VALUES (?,?,?)";
        case record::Kind::VERDICT:
          return "INSERT INTO verdicts VALUES (?,?,?,?)";
        case record::Kind::TRIGGER:
          return "INSERT INTO triggers VALUES (?,?,?)";
        case record::Kind::MARK:
          return "INSERT INTO marks VALUES (?,?,?)";
        case record::Kind::RESTORE:
          return "INSERT INTO restores VALUES (?,?,?)";
        case record::Kind::RESET:
          return "INSERT INTO resets VALUES (?)";
      }
      std::unreachable();
    }

    // One prepared insert per kind, held where the same switch finds it.
    struct Inserts
    {
      StatementHandle frames{ };
      StatementHandle inputs{ };
      StatementHandle watches{ };
      StatementHandle decisions{ };
      StatementHandle verdicts{ };
      StatementHandle triggers{ };
      StatementHandle marks{ };
      StatementHandle restores{ };
      StatementHandle resets{ };

      [[nodiscard]] auto For(record::Kind kind) -> StatementHandle&
      {
        switch (kind)
        {
          case record::Kind::FRAME:    return frames;
          case record::Kind::INPUT:    return inputs;
          case record::Kind::WATCH:    return watches;
          case record::Kind::DECISION: return decisions;
          case record::Kind::VERDICT:  return verdicts;
          case record::Kind::TRIGGER:  return triggers;
          case record::Kind::MARK:     return marks;
          case record::Kind::RESTORE:  return restores;
          case record::Kind::RESET:    return resets;
        }
        std::unreachable();
      }
    };

    auto Exec(sqlite3& database, std::string_view statements) -> std::string
    {
      char* message{ nullptr };
      if (sqlite3_exec(&database, std::string{ statements }.c_str(), nullptr,
                       nullptr, &message) == SQLITE_OK)
        return { };
      std::string const problem{ message == nullptr ? "sqlite failed"
                                                    : message };
      sqlite3_free(message);
      return problem;
    }

    auto BindText(sqlite3_stmt& statement, int column, std::string const& text)
      -> void
    {
      sqlite3_bind_text(&statement, column, text.data(),
                        static_cast<int>(text.size()), SQLITE_TRANSIENT);
    }

    auto Fill(sqlite3_stmt& statement, record::FrameRecord const& record)
      -> void
    {
      sqlite3_bind_int64(&statement, 1,
                         static_cast<sqlite3_int64>(record.frame));
      sqlite3_bind_int64(&statement, 2, record.harness_time);
      sqlite3_bind_int64(&statement, 3,
                         static_cast<sqlite3_int64>(record.hash_exact));
      sqlite3_bind_int64(&statement, 4,
                         static_cast<sqlite3_int64>(record.hash_difference));
      sqlite3_bind_int64(&statement, 5,
                         static_cast<sqlite3_int64>(record.hash_perceptual));
      sqlite3_bind_double(&statement, 6, record.change_amount);
    }

    auto Fill(sqlite3_stmt& statement, record::InputRecord const& record)
      -> void
    {
      sqlite3_bind_int64(&statement, 1,
                         static_cast<sqlite3_int64>(record.frame));
      sqlite3_bind_int64(&statement, 2, record.port);
      sqlite3_bind_int64(&statement, 3, record.pad);
    }

    auto Fill(sqlite3_stmt& statement, record::WatchRecord const& record)
      -> void
    {
      sqlite3_bind_int64(&statement, 1,
                         static_cast<sqlite3_int64>(record.frame));
      sqlite3_bind_int64(&statement, 2, record.watch);
      sqlite3_bind_int64(&statement, 3, record.value);
    }

    auto Fill(sqlite3_stmt& statement, record::DecisionRecord const& record)
      -> void
    {
      sqlite3_bind_int64(&statement, 1,
                         static_cast<sqlite3_int64>(record.frame));
      BindText(statement, 2, record.actor);
      BindText(statement, 3, record.text);
    }

    auto Fill(sqlite3_stmt& statement, record::VerdictRecord const& record)
      -> void
    {
      sqlite3_bind_int64(&statement, 1,
                         static_cast<sqlite3_int64>(record.frame));
      BindText(statement, 2, record.name);
      sqlite3_bind_int64(&statement, 3, record.passed ? 1 : 0);
      BindText(statement, 4, record.text);
    }

    auto Fill(sqlite3_stmt& statement, record::TriggerRecord const& record)
      -> void
    {
      sqlite3_bind_int64(&statement, 1,
                         static_cast<sqlite3_int64>(record.frame));
      BindText(statement, 2, record.name);
      BindText(statement, 3, record.text);
    }

    auto Fill(sqlite3_stmt& statement, record::MarkRecord const& record)
      -> void
    {
      sqlite3_bind_int64(&statement, 1,
                         static_cast<sqlite3_int64>(record.frame));
      BindText(statement, 2, record.text);
      BindText(statement, 3, record.group);
    }

    auto Fill(sqlite3_stmt& statement, record::RestoreRecord const& record)
      -> void
    {
      sqlite3_bind_int64(&statement, 1,
                         static_cast<sqlite3_int64>(record.frame));
      sqlite3_bind_int64(&statement, 2,
                         static_cast<sqlite3_int64>(record.to));
      BindText(statement, 3, record.name);
    }

    auto Fill(sqlite3_stmt& statement, record::ResetRecord const& record)
      -> void
    {
      sqlite3_bind_int64(&statement, 1,
                         static_cast<sqlite3_int64>(record.frame));
    }

    auto PutMeta(sqlite3_stmt& statement, std::string_view key,
                 std::string const& value) -> bool
    {
      sqlite3_reset(&statement);
      sqlite3_bind_text(&statement, 1, key.data(),
                        static_cast<int>(key.size()), SQLITE_TRANSIENT);
      BindText(statement, 2, value);
      return sqlite3_step(&statement) == SQLITE_DONE;
    }
  }

  auto ExportToSqlite(reader::Reader& source,
                      std::filesystem::path const& database)
    -> Result<std::uint64_t>
  {
    if (std::filesystem::exists(database))
      return Refused("trace: '{}' already exists", database.string());

    sqlite3* opened{ nullptr };
    if (sqlite3_open(database.string().c_str(), &opened) != SQLITE_OK)
    {
      DatabaseHandle const closing{ opened };
      return Refused("trace: cannot create '{}'", database.string());
    }
    DatabaseHandle const held{ opened };

    if (auto problem{ Exec(*held, SCHEMA) }; !problem.empty())
      return Refused("trace: {}", problem);
    if (auto problem{ Exec(*held, "BEGIN") }; !problem.empty())
      return Refused("trace: {}", problem);

    Inserts inserts{ };
    std::uint64_t rows{ 0 };
    std::string problem{ };
    source.ForEach([&](auto const& record) {
      if (!problem.empty())
        return;
      StatementHandle& prepared{ inserts.For(record.KIND) };
      if (!prepared)
      {
        std::string_view const sql{ InsertFor(record.KIND) };
        sqlite3_stmt* opening{ nullptr };
        if (sqlite3_prepare_v2(held.get(), sql.data(),
                               static_cast<int>(sql.size()), &opening,
                               nullptr) != SQLITE_OK)
        {
          problem = sqlite3_errmsg(held.get());
          return;
        }
        prepared.reset(opening);
      }
      sqlite3_stmt& statement{ *prepared };
      sqlite3_reset(&statement);
      Fill(statement, record);
      if (sqlite3_step(&statement) != SQLITE_DONE)
        problem = sqlite3_errmsg(held.get());
      else
        ++rows;
    });
    if (!problem.empty())
      return Refused("trace: {}", problem);

    sqlite3_stmt* meta{ nullptr };
    if (sqlite3_prepare_v2(held.get(), "INSERT INTO meta VALUES (?,?)", -1,
                           &meta, nullptr) != SQLITE_OK)
      return Refused("trace: {}", sqlite3_errmsg(held.get()));
    StatementHandle const meta_held{ meta };

    auto const& file_header{ source.FileHeader() };
    bool const stored{
      PutMeta(*meta, "format_version",
              std::format("{}", file_header.format_version))
      && PutMeta(*meta, "creation_time",
                 std::format("{}", file_header.creation_time))
      && PutMeta(*meta, "producer", file_header.producer)
      && PutMeta(*meta, "records", std::format("{}", source.Records()))
      && PutMeta(*meta, "unknown_records",
                 std::format("{}", source.Unknown()))
      && PutMeta(*meta, "truncated", source.Truncated() ? "1" : "0") };
    if (!stored)
      return Refused("trace: {}", sqlite3_errmsg(held.get()));

    if (auto failed{ Exec(*held, INDEXES) }; !failed.empty())
      return Refused("trace: {}", failed);
    if (auto failed{ Exec(*held, "COMMIT") }; !failed.empty())
      return Refused("trace: {}", failed);

    return rows;
  }
}

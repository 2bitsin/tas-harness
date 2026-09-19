#pragma once

#include "tash/trace/record.hpp"
#include "tash/utilities/outcome.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace tash::recorder::detail::verdicts_writer
{
  using utilities::Outcome;
  using utilities::Result;

  // The bundle's own shape for a verdict, so the JSON a reader sees does not
  // move when the trace record grows a field.
  struct VerdictLine
  {
    friend constexpr auto reflect_scheme(VerdictLine*);

    std::uint64_t frame{ 0 };
    std::string   name{ };
    bool          passed{ false };
    std::string   text{ };

    auto operator == (VerdictLine const&) const -> bool = default;
  };

  // One JSON object per line, appended: a reader may follow a live run, and a
  // crash costs at most the line being written.
  class VerdictsWriter
  {
  public:
    [[nodiscard]] static auto Open(std::filesystem::path const& path)
      -> Result<VerdictsWriter>;

    VerdictsWriter(VerdictsWriter const&) = delete;
    auto operator = (VerdictsWriter const&) -> VerdictsWriter& = delete;
    VerdictsWriter(VerdictsWriter&&) = default;
    auto operator = (VerdictsWriter&&) -> VerdictsWriter& = default;

    ~VerdictsWriter() = default;

    auto Append(trace::VerdictRecord const& verdict) -> Outcome;

    [[nodiscard]] auto Written() const noexcept -> std::uint64_t
    { return _written; }

  private:
    explicit VerdictsWriter(std::ofstream file, std::filesystem::path path);

    std::ofstream         _file;
    std::filesystem::path _path;
    std::uint64_t         _written{ 0 };
  };
}

namespace tash::recorder
{
  using detail::verdicts_writer::VerdictLine;
  using detail::verdicts_writer::VerdictsWriter;
}

#include "tash/linked/tash.hpp"

#include "_recording.hpp"

#include <cstdlib>
#include <format>
#include <utility>

namespace tash::linked::detail::harness
{
  namespace
  {
    auto Noted(Options const& options, std::string const& text) -> void
    {
      if (options.note)
        options.note(text);
    }

    [[nodiscard]] auto Variable(std::string_view name)
      -> std::optional<std::string>
    {
      char const* const value{ std::getenv(std::string{ name }.c_str()) };
      if (value == nullptr || value[0] == '\0')
        return std::nullopt;
      return std::string{ value };
    }

    [[nodiscard]] auto Directory(Options const& options)
      -> std::optional<std::filesystem::path>
    {
      if (!options.directory.empty())
        return options.directory;
      if (auto const named{ Variable(RECORD_VARIABLE) })
        return std::filesystem::path{ *named };
      return std::nullopt;
    }

    [[nodiscard]] auto Opened(Options const& options)
      -> std::unique_ptr<recording::Recording>
    {
      if (options.sink == Sink::MEMORY)
        return recording::Recording::InMemory(options);

      if (options.sink == Sink::ENVIRONMENT
          && Variable(SESSION_VARIABLE).has_value())
        Noted(options, std::format("libtash: {} names a harness, and attached "
                                   "mode is not in this build",
                                   SESSION_VARIABLE));

      auto const directory{ Directory(options) };
      if (!directory)
      {
        if (options.sink == Sink::DIRECTORY)
          Noted(options, std::format("libtash: no directory to record into, "
                                     "and {} names none", RECORD_VARIABLE));
        return nullptr;
      }

      auto made{ recording::Recording::InDirectory(options,
                                                   *directory) };
      if (!made)
      {
        Noted(options, made.error());
        return nullptr;
      }
      return std::move(*made);
    }

    [[nodiscard]] auto Made(Options options)
      -> std::unique_ptr<recording::Recording>
    {
      if (options.sink == Sink::NONE)
        return nullptr;
      if (options.api != API_VERSION)
      {
        Noted(options, std::format("libtash: this target was built against "
                                   "api {}, the library is api {}",
                                   options.api, API_VERSION));
        return nullptr;
      }
      return Opened(options);
    }

    // A recording that runs out of memory stops growing; the target it is
    // watching plays on, which is the whole point of it being there.
    template <typename Work>
    auto Guarded(Work work) noexcept -> void
    {
      try
      {
        work();
      }
      catch (...)
      {
      }
    }

    template <typename Work, typename Answer>
    [[nodiscard]] auto Answered(Work work, Answer instead) noexcept -> Answer
    {
      try
      {
        return work();
      }
      catch (...)
      {
        return instead;
      }
    }
  }

  Harness::Harness(Options options)
    : _recording{ Answered([&options]
                           { return Made(std::move(options)); },
                           std::unique_ptr<recording::Recording>{ }) }
  { }

  Harness::~Harness() = default;

  auto Harness::ModeOf() const -> harness::Mode
  {
    return harness::Mode::FILE;
  }

  auto Harness::Began(Input&) -> bool
  {
    Guarded([this] { _recording->Begin(); });
    return false;
  }

  auto Harness::Submitted(Input const& input) -> void
  {
    Guarded([this, &input] { _recording->Input(input); });
  }

  auto Harness::Submitted(Video const& video) -> void
  {
    Guarded([this, &video] { _recording->Video(video); });
  }

  auto Harness::Submitted(Audio const& audio) -> void
  {
    Guarded([this, &audio] { _recording->Audio(audio); });
  }

  auto Harness::Registered(Watch const& watch) -> std::optional<std::size_t>
  {
    return Answered([this, &watch] { return _recording->Watch(watch); },
                    std::optional<std::size_t>{ });
  }

  auto Harness::Registered(State state) -> void
  {
    Guarded([this, &state] { _recording->Keep(std::move(state)); });
  }

  auto Harness::Emitted(std::string_view name, std::string_view text) -> void
  {
    Guarded([this, name, text] { _recording->Event(name, text); });
  }

  auto Harness::Stepped(double own) -> double
  {
    return Answered([this, own] { return _recording->Dt(own); }, own);
  }

  auto Harness::Seeded(std::uint64_t own) -> std::uint64_t
  {
    return Answered([this, own] { return _recording->Seed(own); }, own);
  }

  auto Harness::Flushed() -> void
  {
    Guarded([this] { _recording->Flush(); });
  }

  auto Harness::Fetched(Artifact which) const -> std::span<std::byte const>
  {
    return Answered([this, which] { return _recording->Bytes(which); },
                    std::span<std::byte const>{ });
  }
}

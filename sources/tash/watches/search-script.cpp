#include "tash/watches/search-script.hpp"

#include "tash/libretro/libretro.h"

#include <array>
#include <charconv>
#include <string>
#include <utility>

namespace tash::watches::detail::search_script
{
  using utilities::Forwarded;
  using utilities::Refused;
  using utilities::Result;

  namespace
  {
    constexpr std::string_view SPACE{ " \t" };
    constexpr char STEP_SEPARATOR{ ';' };
    constexpr char BUTTON_SEPARATOR{ ',' };
    constexpr int DECIMAL_BASE{ 10 };

    constexpr std::array<std::pair<std::string_view, unsigned>, 12> BUTTONS{ {
      { "b", RETRO_DEVICE_ID_JOYPAD_B },
      { "y", RETRO_DEVICE_ID_JOYPAD_Y },
      { "select", RETRO_DEVICE_ID_JOYPAD_SELECT },
      { "start", RETRO_DEVICE_ID_JOYPAD_START },
      { "up", RETRO_DEVICE_ID_JOYPAD_UP },
      { "down", RETRO_DEVICE_ID_JOYPAD_DOWN },
      { "left", RETRO_DEVICE_ID_JOYPAD_LEFT },
      { "right", RETRO_DEVICE_ID_JOYPAD_RIGHT },
      { "a", RETRO_DEVICE_ID_JOYPAD_A },
      { "x", RETRO_DEVICE_ID_JOYPAD_X },
      { "l", RETRO_DEVICE_ID_JOYPAD_L },
      { "r", RETRO_DEVICE_ID_JOYPAD_R },
    } };

    [[nodiscard]] auto Trimmed(std::string_view text) noexcept
      -> std::string_view
    {
      auto const first{ text.find_first_not_of(SPACE) };
      if (first == std::string_view::npos)
        return {};
      return text.substr(first, text.find_last_not_of(SPACE) - first + 1u);
    }

    [[nodiscard]] auto Split(std::string_view text, char on)
      -> std::vector<std::string_view>
    {
      std::vector<std::string_view> parts;
      std::size_t at{ 0 };
      while (at <= text.size())
      {
        auto const next{ text.find(on, at) };
        auto const end{ next == std::string_view::npos ? text.size() : next };
        parts.push_back(Trimmed(text.substr(at, end - at)));
        if (next == std::string_view::npos)
          break;
        at = next + 1u;
      }
      return parts;
    }

    [[nodiscard]] auto Words(std::string_view step)
      -> std::vector<std::string_view>
    {
      std::vector<std::string_view> words;
      std::size_t at{ 0 };
      while (at < step.size())
      {
        auto const start{ step.find_first_not_of(SPACE, at) };
        if (start == std::string_view::npos)
          break;
        auto const end{ step.find_first_of(SPACE, start) };
        words.push_back(step.substr(start, end == std::string_view::npos
                                             ? std::string_view::npos
                                             : end - start));
        at = end == std::string_view::npos ? step.size() : end;
      }
      return words;
    }

    template <typename Number>
    [[nodiscard]] auto NumberOf(std::string_view text, std::string_view what)
      -> Result<Number>
    {
      Number value{ 0 };
      auto const read{ std::from_chars(text.data(), text.data() + text.size(),
                                       value, DECIMAL_BASE) };
      if (read.ec != std::errc{} || read.ptr != text.data() + text.size())
        return Refused("watches: {} wants a number, not {}", what, text);
      return value;
    }
  }

  auto PadOf(std::string_view names) -> Result<std::uint32_t>
  {
    std::uint32_t pad{ 0 };
    for (std::string_view const name : Split(names, BUTTON_SEPARATOR))
    {
      if (name.empty())
        continue;
      auto found{ false };
      for (auto const& [spelt, id] : BUTTONS)
        if (spelt == name)
        {
          pad |= 1u << id;
          found = true;
        }
      if (!found)
        return Refused("watches: {} is not a button on a joypad", name);
    }
    if (pad == 0u)
      return Refused("watches: hold wants at least one button");
    return pad;
  }

  auto StepGrammar() noexcept -> std::string_view
  {
    return "run <frames> | hold <button,...> | release | snapshot | "
           "equal | changed | increased | decreased | value <n> | list [n]";
  }

  auto StepsFrom(std::string_view text) -> Result<std::vector<SearchStep>>
  {
    std::vector<SearchStep> steps;
    for (std::string_view const phrase : Split(text, STEP_SEPARATOR))
    {
      std::vector<std::string_view> const words{ Words(phrase) };
      if (words.empty())
        continue;

      std::string_view const verb{ words.front() };
      auto const arguments{ words.size() - 1u };
      SearchStep step{};

      if (verb == "run")
      {
        if (arguments != 1u)
          return Refused("watches: run wants a frame count");
        Result<std::uint64_t> const frames{
          NumberOf<std::uint64_t>(words[1], "run") };
        if (!frames)
          return Forwarded(frames);
        step = SearchStep{ StepKind::RUN, *frames };
      }
      else if (verb == "hold")
      {
        if (arguments != 1u)
          return Refused("watches: hold wants a button list");
        Result<std::uint32_t> const pad{ PadOf(words[1]) };
        if (!pad)
          return Forwarded(pad);
        step.kind = StepKind::HOLD;
        step.pad = *pad;
      }
      else if (verb == "release")
        step.kind = StepKind::RELEASE;
      else if (verb == "snapshot")
        step.kind = StepKind::SNAPSHOT;
      else if (verb == "list")
      {
        step.kind = StepKind::LIST;
        if (arguments == 1u)
        {
          Result<std::size_t> const limit{
            NumberOf<std::size_t>(words[1], "list") };
          if (!limit)
            return Forwarded(limit);
          step.limit = *limit;
        }
        else if (arguments != 0u)
          return Refused("watches: list wants a count or nothing");
      }
      else
      {
        Result<Comparison> const how{ memory_search::ComparisonOf(verb) };
        if (!how)
          return Refused("watches: {} is not a step; say {}", verb,
                         StepGrammar());
        step.kind = StepKind::NARROW;
        step.how = *how;
        if (*how == Comparison::VALUE)
        {
          if (arguments != 1u)
            return Refused("watches: value wants the number to look for");
          Result<std::int64_t> const against{
            NumberOf<std::int64_t>(words[1], "value") };
          if (!against)
            return Forwarded(against);
          step.against = *against;
        }
        else if (arguments != 0u)
          return Refused("watches: {} takes no argument", verb);
      }
      steps.push_back(step);
    }
    if (steps.empty())
      return Refused("watches: no steps; say {}", StepGrammar());
    return steps;
  }
}

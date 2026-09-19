#include "tash/mcp/argument-line.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <format>
#include <string_view>

namespace tash::mcp::detail::argument_line
{
  using utilities::Refused;

  namespace
  {
    inline constexpr std::string_view OPTION_PREFIX{ "--" };
    inline constexpr std::string_view BOOLEAN{ "boolean" };
    inline constexpr std::string_view INTEGER{ "integer" };
    inline constexpr std::string_view NUMBER{ "number" };
    inline constexpr std::string_view TRUTH{ "true" };
    inline constexpr std::string_view FALSEHOOD{ "false" };
    inline constexpr char SEPARATOR{ ',' };

    [[nodiscard]] auto Scalar(std::string_view type, std::string const& word)
      -> Result<nlohmann::json>
    {
      if (type == BOOLEAN)
      {
        if (word == TRUTH)
          return nlohmann::json(true);
        if (word == FALSEHOOD)
          return nlohmann::json(false);
        return Refused("mcp: '{}' is not true or false", word);
      }
      if (type == INTEGER)
      {
        std::int64_t held{ 0 };
        auto const [stopped, failed]{ std::from_chars(
          word.data(), word.data() + word.size(), held) };
        if (failed != std::errc{ } || stopped != word.data() + word.size())
          return Refused("mcp: '{}' is not a whole number", word);
        return nlohmann::json(held);
      }
      if (type == NUMBER)
      {
        double held{ 0.0 };
        auto const [stopped, failed]{ std::from_chars(
          word.data(), word.data() + word.size(), held) };
        if (failed != std::errc{ } || stopped != word.data() + word.size())
          return Refused("mcp: '{}' is not a number", word);
        return nlohmann::json(held);
      }
      return nlohmann::json(word);
    }

    [[nodiscard]] auto Listed(std::string_view type, std::string const& word)
      -> Result<nlohmann::json>
    {
      nlohmann::json held = nlohmann::json::array();
      if (word.empty())
        return held;
      for (std::size_t at{ 0 }; at <= word.size();)
      {
        std::size_t const end{
          std::min(word.find(SEPARATOR, at), word.size()) };
        Result<nlohmann::json> const one{
          Scalar(type, word.substr(at, end - at)) };
        if (!one)
          return std::unexpected{ one.error() };
        held.push_back(*one);
        at = end + 1;
      }
      return held;
    }
  }

  auto ArgumentsFrom(tool_schema::ToolSchema const& schema,
                     std::vector<std::string> const& words)
    -> Result<std::string>
  {
    nlohmann::json given = nlohmann::json::object();
    for (std::size_t at{ 0 }; at < words.size(); ++at)
    {
      if (!words[at].starts_with(OPTION_PREFIX))
        return Refused("mcp: '{}' is not --name", words[at]);

      std::string const name{ words[at].substr(OPTION_PREFIX.size()) };
      auto const known{ schema.properties.find(name) };
      if (known == schema.properties.end())
        return Refused("mcp: this tool takes no --{}", name);

      bool const listed{ known->second.type == tool_schema::ARRAY_TYPE };
      std::string_view const type{
        listed && known->second.items ? known->second.items->type
                                      : known->second.type };
      if (type == BOOLEAN && !listed
          && (at + 1 == words.size()
              || words[at + 1].starts_with(OPTION_PREFIX)))
      {
        given[name] = true;
        continue;
      }
      if (at + 1 == words.size())
        return Refused("mcp: --{} wants a value", name);

      Result<nlohmann::json> const held{
        listed ? Listed(type, words[at + 1])
               : Scalar(type, words[at + 1]) };
      if (!held)
        return std::unexpected{ held.error() };
      given[name] = *held;
      ++at;
    }
    return given.dump();
  }
}

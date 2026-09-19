#include "tash/mcp/content.hpp"

#include <cstddef>
#include <format>
#include <utility>

namespace tash::mcp::detail::content
{
  namespace
  {
    // Module names are lowercase here, so an exception's own `NameError:`
    // survives while `python: mcp:` prefixes do not.
    [[nodiscard]] auto Module(std::string_view word) -> bool
    {
      if (word.empty())
        return false;
      for (char const letter : word)
        if ((letter < 'a' || letter > 'z') && letter != '_')
          return false;
      return true;
    }

    auto TextBlock(std::string text) -> ContentBlock
    {
      ContentBlock block;
      block.type = std::string{ TEXT_BLOCK };
      block.text = std::move(text);
      return block;
    }
  }

  auto Said(std::string text) -> ToolOutcome
  {
    return ToolOutcome{ { TextBlock(std::move(text)) }, false };
  }

  auto Failed(std::string text) -> ToolOutcome
  {
    return ToolOutcome{ { TextBlock(std::move(text)) }, true };
  }

  auto Under(std::string_view tool, std::string_view said) -> std::string
  {
    for (std::size_t at{ said.find(':') };
         at != std::string_view::npos && at + 1 < said.size()
           && said[at + 1] == ' ' && Module(said.substr(0, at));
         at = said.find(':'))
      said.remove_prefix(at + 2);
    return std::format("{}: {}", tool, said);
  }

  auto Named(std::string_view tool, ToolOutcome said) -> ToolOutcome
  {
    if (!said.is_error)
      return said;
    for (ContentBlock& block : said.content)
      if (block.text)
        block.text = Under(tool, *block.text);
    return said;
  }

  auto Shown(std::string encoded, std::string text) -> ToolOutcome
  {
    ContentBlock image;
    image.type = std::string{ IMAGE_BLOCK };
    image.data = std::move(encoded);
    image.mime_type = std::string{ PNG_MEDIA_TYPE };
    return ToolOutcome{ { TextBlock(std::move(text)), std::move(image) },
                        false };
  }
}

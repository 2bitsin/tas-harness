#pragma once
// What a tool answers with: the text and image content blocks of MCP tools
// 2.6, in one shape whose unused members are absent from the wire.

#include <_buildutil/reflect.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tash::mcp::detail::content
{
  inline constexpr std::string_view TEXT_BLOCK{ "text" };
  inline constexpr std::string_view IMAGE_BLOCK{ "image" };
  inline constexpr std::string_view PNG_MEDIA_TYPE{ "image/png" };

  struct ContentBlock
  {
    friend constexpr auto reflect_scheme(ContentBlock*);

    std::string                type;
    std::optional<std::string> text;
    std::optional<std::string> data;

    _Label(mimeType) std::optional<std::string> mime_type;
  };

  // MCP tools 2.9: a tool that failed says so in its result, and only the
  // protocol's own refusals become json-rpc errors.
  struct ToolOutcome
  {
    friend constexpr auto reflect_scheme(ToolOutcome*);

    std::vector<ContentBlock> content;

    _Label(isError) bool is_error{ false };
  };

  [[nodiscard]] auto Said(std::string text) -> ToolOutcome;

  [[nodiscard]] auto Failed(std::string text) -> ToolOutcome;

  [[nodiscard]] auto Shown(std::string encoded, std::string text)
    -> ToolOutcome;

  // A refusal answers in the tool's name: which module the verb is built on
  // is the server's business, not the caller's.
  [[nodiscard]] auto Under(std::string_view tool, std::string_view said)
    -> std::string;

  [[nodiscard]] auto Named(std::string_view tool, ToolOutcome said)
    -> ToolOutcome;
}

namespace tash::mcp
{
  using detail::content::ContentBlock;
  using detail::content::Failed;
  using detail::content::Named;
  using detail::content::IMAGE_BLOCK;
  using detail::content::PNG_MEDIA_TYPE;
  using detail::content::Said;
  using detail::content::Shown;
  using detail::content::TEXT_BLOCK;
  using detail::content::ToolOutcome;
  using detail::content::Under;
}

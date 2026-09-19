#pragma once

#include "tash/utilities/outcome.hpp"

#include <map>
#include <string>
#include <string_view>

namespace tash::report::detail::html_writer
{
  using utilities::Result;

  // A page is a template with `{{slot}}` holes and this fills them: the tags
  // stay in one file that reads as html, the code only says what goes in.
  // Filling is where escaping is decided, so nothing reaches the page
  // unescaped by forgetting.
  class HtmlWriter
  {
  public:
    explicit HtmlWriter(std::string_view page) : _page{ page } { }

    auto Fill(std::string_view slot, std::string_view text) -> HtmlWriter&;

    auto FillHtml(std::string_view slot, std::string html) -> HtmlWriter&;

    [[nodiscard]] auto Page() const -> Result<std::string>;

    [[nodiscard]] static auto Escaped(std::string_view text) -> std::string;

  private:
    std::string_view                    _page;
    std::map<std::string, std::string>  _filled;
  };
}

namespace tash::report
{
  using detail::html_writer::HtmlWriter;
}

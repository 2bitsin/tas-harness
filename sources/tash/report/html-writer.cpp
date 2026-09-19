#include "tash/report/html-writer.hpp"

#include "tash/utilities/outcome.hpp"

#include <utility>

namespace tash::report::detail::html_writer
{
  using utilities::Refused;

  namespace
  {
    constexpr std::string_view SLOT_OPEN{ "{{" };
    constexpr std::string_view SLOT_CLOSE{ "}}" };
  }

  auto HtmlWriter::Fill(std::string_view slot, std::string_view text)
    -> HtmlWriter&
  {
    _filled[std::string{ slot }] = Escaped(text);
    return *this;
  }

  auto HtmlWriter::FillHtml(std::string_view slot, std::string html)
    -> HtmlWriter&
  {
    _filled[std::string{ slot }] = std::move(html);
    return *this;
  }

  auto HtmlWriter::Page() const -> Result<std::string>
  {
    std::string page;
    page.reserve(_page.size());
    for (std::size_t at{ 0 }; at < _page.size(); )
    {
      std::size_t const opened{ _page.find(SLOT_OPEN, at) };
      if (opened == std::string_view::npos)
      {
        page.append(_page.substr(at));
        break;
      }
      std::size_t const closed{ _page.find(SLOT_CLOSE, opened) };
      if (closed == std::string_view::npos)
        return Refused("report: a template slot is never closed");

      page.append(_page.substr(at, opened - at));
      std::string const slot{ _page.substr(
        opened + SLOT_OPEN.size(), closed - opened - SLOT_OPEN.size()) };
      auto const filled{ _filled.find(slot) };
      if (filled == _filled.end())
        return Refused("report: nothing was filled into the {} slot", slot);
      page.append(filled->second);
      at = closed + SLOT_CLOSE.size();
    }
    return page;
  }

  auto HtmlWriter::Escaped(std::string_view text) -> std::string
  {
    std::string safe;
    safe.reserve(text.size());
    for (char const letter : text)
      switch (letter)
      {
        case '&':  safe.append("&amp;");   break;
        case '<':  safe.append("&lt;");    break;
        case '>':  safe.append("&gt;");    break;
        case '"':  safe.append("&quot;");  break;
        case '\'': safe.append("&#39;");   break;
        default:   safe.push_back(letter); break;
      }
    return safe;
  }
}

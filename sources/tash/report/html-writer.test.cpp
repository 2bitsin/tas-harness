#include "tash/report/html-writer.hpp"

#include <gtest/gtest.h>

#include <string>

namespace
{
  using tash::report::HtmlWriter;
}

TEST(HtmlWriter, EscapingIsTheSameFiveCharactersEveryTime)
{
  EXPECT_EQ(HtmlWriter::Escaped(R"(<a href="x" id='y'>&</a>)"),
            "&lt;a href=&quot;x&quot; id=&#39;y&#39;&gt;&amp;&lt;/a&gt;");
  EXPECT_EQ(HtmlWriter::Escaped("plain"), "plain");
}

TEST(HtmlWriter, ASlotNothingWasFilledIntoIsARefusal)
{
  HtmlWriter writer{ "<p>{{here}}</p><p>{{missing}}</p>" };
  writer.Fill("here", "text");
  auto const page{ writer.Page() };
  ASSERT_FALSE(page.has_value());
  EXPECT_NE(page.error().find("missing"), std::string::npos);
}

TEST(HtmlWriter, TextIsEscapedAndFragmentsAreNot)
{
  HtmlWriter writer{ "<p>{{text}}</p><ul>{{rows}}</ul>" };
  writer.Fill("text", "a & b");
  writer.FillHtml("rows", "<li>one</li>");
  auto const page{ writer.Page() };
  ASSERT_TRUE(page.has_value()) << (page ? "" : page.error());
  EXPECT_EQ(*page, "<p>a &amp; b</p><ul><li>one</li></ul>");
}

TEST(HtmlWriter, TheSameSlotTwiceIsFilledTwice)
{
  HtmlWriter writer{ "{{name}}/{{name}}" };
  writer.Fill("name", "x");
  auto const page{ writer.Page() };
  ASSERT_TRUE(page.has_value()) << (page ? "" : page.error());
  EXPECT_EQ(*page, "x/x");
}

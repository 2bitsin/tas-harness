#include "tash/mcp/dispatcher.hpp"

#include "tash/mcp/_nothing-host.hpp"
#include "tash/mcp/json-rpc.hpp"
#include "tash/mcp/protocol.hpp"
#include "tash/mcp/tool-table.hpp"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
  using tash::mcp::Dispatcher;
  using NoHost = tash::mcp::testing::NothingHost;
  using tash::mcp::ToolContext;

  auto Asked(std::string const& text) -> tash::mcp::RpcMessage
  {
    auto read{ tash::mcp::MessageFrom(text) };
    EXPECT_TRUE(read.has_value()) << (read ? "" : read.error());
    return read ? *read : tash::mcp::RpcMessage{ };
  }

  auto Opened(Dispatcher& answering) -> void
  {
    auto const said{ answering.Handle(Asked(
      R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":)"
      R"({"protocolVersion":"2025-06-18"}})")) };
    ASSERT_TRUE(said.result.has_value());
  }
}

TEST(McpDispatcher, InitializeNamesTheServerAndItsTools)
{
  NoHost host;
  ToolContext tools{ host };
  Dispatcher answering{ tools };

  auto const said{ answering.Handle(Asked(
    R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":)"
    R"({"protocolVersion":"2025-06-18"}})")) };
  ASSERT_TRUE(said.result.has_value()) << (said.failed
                                             ? said.failed->message : "");

  auto const read = nlohmann::json::parse(*said.result);
  EXPECT_EQ(read["serverInfo"]["name"], tash::mcp::SERVER_NAME);
  EXPECT_EQ(read["protocolVersion"], tash::mcp::PROTOCOL_VERSION);
  EXPECT_TRUE(read["capabilities"].contains("tools"));
  EXPECT_TRUE(answering.Initialized());
}

TEST(McpDispatcher, ARealMethodBeforeInitializeIsRefused)
{
  NoHost host;
  ToolContext tools{ host };
  Dispatcher answering{ tools };

  auto const said{ answering.Handle(Asked(
    R"({"jsonrpc":"2.0","id":7,"method":"tools/list"})")) };
  ASSERT_TRUE(said.failed.has_value());
  EXPECT_EQ(said.failed->code, tash::mcp::INVALID_REQUEST);
}

TEST(McpDispatcher, PingIsAnsweredBeforeInitialize)
{
  NoHost host;
  ToolContext tools{ host };
  Dispatcher answering{ tools };

  auto const said{ answering.Handle(Asked(
    R"({"jsonrpc":"2.0","id":2,"method":"ping"})")) };
  ASSERT_TRUE(said.result.has_value());
  EXPECT_EQ(nlohmann::json::parse(*said.result), nlohmann::json::object());
}

TEST(McpDispatcher, AnUnknownMethodIsMethodNotFound)
{
  NoHost host;
  ToolContext tools{ host };
  Dispatcher answering{ tools };
  Opened(answering);

  auto const said{ answering.Handle(Asked(
    R"({"jsonrpc":"2.0","id":3,"method":"tools/dance"})")) };
  ASSERT_TRUE(said.failed.has_value());
  EXPECT_EQ(said.failed->code, tash::mcp::METHOD_NOT_FOUND);
}

TEST(McpDispatcher, EveryToolListsASchemaThatParses)
{
  NoHost host;
  ToolContext tools{ host };
  Dispatcher answering{ tools };
  Opened(answering);

  auto const said{ answering.Handle(Asked(
    R"({"jsonrpc":"2.0","id":4,"method":"tools/list"})")) };
  ASSERT_TRUE(said.result.has_value());

  auto const read = nlohmann::json::parse(*said.result);
  ASSERT_TRUE(read["tools"].is_array());
  EXPECT_EQ(read["tools"].size(), tash::mcp::AllTools().size());

  for (auto const& tool : read["tools"])
  {
    EXPECT_FALSE(tool["name"].get<std::string>().empty());
    EXPECT_FALSE(tool["description"].get<std::string>().empty());
    auto const& schema{ tool["inputSchema"] };
    EXPECT_EQ(schema["type"], "object");
    EXPECT_TRUE(schema.contains("properties"));
    for (auto const& named : schema["required"])
      EXPECT_TRUE(schema["properties"].contains(named.get<std::string>()))
        << tool["name"];
  }
}

TEST(McpDispatcher, TheStopForAPythonJobIsListedAmongThePythonTools)
{
  NoHost host;
  ToolContext tools{ host };
  Dispatcher answering{ tools };
  Opened(answering);

  auto const said{ answering.Handle(Asked(
    R"({"jsonrpc":"2.0","id":9,"method":"tools/list"})")) };
  ASSERT_TRUE(said.result.has_value());

  auto const read = nlohmann::json::parse(*said.result);
  std::vector<std::string> named;
  for (auto const& tool : read["tools"])
    named.push_back(tool["name"].get<std::string>());

  for (std::string_view const wanted : { "python", "python_file",
                                         "python_cancel", "python_status",
                                         "python_output" })
    EXPECT_NE(std::ranges::find(named, wanted), named.end()) << wanted;
  EXPECT_EQ(std::ranges::find(named, std::string_view{ "cancel" }),
            named.end());
}

TEST(McpDispatcher, PeekAsksForAnAddressAndACountAndNotForARegion)
{
  NoHost host;
  ToolContext tools{ host };
  Dispatcher answering{ tools };
  Opened(answering);

  auto const said{ answering.Handle(Asked(
    R"({"jsonrpc":"2.0","id":8,"method":"tools/list"})")) };
  ASSERT_TRUE(said.result.has_value());

  auto const read = nlohmann::json::parse(*said.result);
  nlohmann::json peek;
  for (auto const& tool : read["tools"])
    if (tool["name"] == "peek")
      peek = tool;
  ASSERT_FALSE(peek.is_null());

  auto const& schema{ peek["inputSchema"] };
  EXPECT_TRUE(schema["properties"].contains("region"));
  std::vector<std::string> const required{
    schema["required"].get<std::vector<std::string>>() };
  EXPECT_TRUE(std::ranges::contains(required, std::string{ "address" }));
  EXPECT_TRUE(std::ranges::contains(required, std::string{ "count" }));
  EXPECT_FALSE(std::ranges::contains(required, std::string{ "region" }));
}

TEST(McpDispatcher, PeekWithNoTargetRefusesRatherThanThrows)
{
  NoHost host;
  ToolContext tools{ host };
  Dispatcher answering{ tools };
  Opened(answering);

  auto const said{ answering.Handle(Asked(
    R"({"jsonrpc":"2.0","id":9,"method":"tools/call","params":)"
    R"({"name":"peek","arguments":{"address":"0xc800","count":16}}})")) };
  ASSERT_TRUE(said.result.has_value());
  EXPECT_TRUE(nlohmann::json::parse(*said.result)["isError"].get<bool>());
}

TEST(McpDispatcher, AnUnknownToolIsInvalidParams)
{
  NoHost host;
  ToolContext tools{ host };
  Dispatcher answering{ tools };
  Opened(answering);

  auto const said{ answering.Handle(Asked(
    R"({"jsonrpc":"2.0","id":5,"method":"tools/call",)"
    R"("params":{"name":"fly","arguments":{}}})")) };
  ASSERT_TRUE(said.failed.has_value());
  EXPECT_EQ(said.failed->code, tash::mcp::INVALID_PARAMS);
}

TEST(McpDispatcher, AToolWithNoTargetRefusesRatherThanThrows)
{
  NoHost host;
  ToolContext tools{ host };
  Dispatcher answering{ tools };
  Opened(answering);

  auto const said{ answering.Handle(Asked(
    R"({"jsonrpc":"2.0","id":6,"method":"tools/call",)"
    R"("params":{"name":"observe","arguments":{}}})")) };
  ASSERT_TRUE(said.result.has_value());
  auto const read = nlohmann::json::parse(*said.result);
  EXPECT_TRUE(read["isError"].get<bool>());
}

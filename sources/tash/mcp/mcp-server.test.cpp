#include "tash/mcp/mcp-server.hpp"

#include "tash/mcp/_nothing-host.hpp"
#include "tash/mcp/mcp-client.hpp"
#include "tash/mcp/protocol.hpp"
#include "tash/mcp/tool-context.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <format>
#include <future>
#include <string>
#include <thread>
#include <vector>

namespace
{
  using tash::mcp::McpClient;
  using tash::mcp::McpServer;

  constexpr int SERVED{ 200 };
  constexpr int REFUSED{ 400 };
  constexpr int GONE{ 404 };

  constexpr char const* STRANGER{ "0123456789abcdef0123456789abcdef" };

  // Short enough to wait out in a test, longer than the sweep's own floor.
  constexpr std::chrono::milliseconds IDLE_TEST{ 1000 };
  constexpr std::chrono::milliseconds SWEEP_GRACE{ 500 };

  // Enough half-idles in a row that a timer that did not move would fire.
  constexpr int CALLS_OVER_IDLE{ 4 };

  // The endpoint on a thread of its own, over a host that launches nothing:
  // what is proved here is the session map, not what a tool does.
  class Serving
  {
  public:
    explicit Serving(std::chrono::seconds idle = tash::mcp::SESSION_IDLE)
    {
      std::promise<std::uint16_t> listening;
      std::future<std::uint16_t> ready{ listening.get_future() };
      _serving = std::thread{ [this, &listening, idle]
      {
        tash::mcp::testing::NothingHost host;
        tash::mcp::ToolContext tools{ host };
        tash::mcp::Dispatcher answering{ tools };
        auto opened{ McpServer::Open(answering,
                                     tash::mcp::McpOptions{ "127.0.0.1", 0,
                                                            idle }) };
        if (!opened)
        {
          listening.set_value(0);
          return;
        }
        _server = opened->get();
        (*opened)->Start();
        listening.set_value((*opened)->Port());
        (*opened)->Run();
        _server = nullptr;
      } };
      _port = ready.get();
    }

    ~Serving()
    {
      if (McpServer* const serving{ _server.load() })
        serving->Stop();
      if (_serving.joinable())
        _serving.join();
    }

    Serving(Serving const&)                    = delete;
    auto operator = (Serving const&) -> Serving& = delete;

    [[nodiscard]] auto Talking() -> McpClient
    { return McpClient{ "127.0.0.1", _port }; }

    [[nodiscard]] auto Port() const noexcept -> std::uint16_t
    { return _port; }

  private:
    std::thread              _serving{ };
    std::atomic<McpServer*>  _server{ nullptr };
    std::uint16_t            _port{ 0 };
  };

  auto Ping() -> std::string
  {
    return std::format(R"({{"jsonrpc":"{}","id":9,"method":"{}"}})",
                       tash::mcp::JSONRPC_VERSION,
                       tash::mcp::METHOD_PING);
  }
}

TEST(McpServer, ASecondInitializeLeavesTheFirstSessionServed)
{
  Serving serving;
  ASSERT_NE(serving.Port(), 0);

  McpClient first{ serving.Talking() };
  ASSERT_TRUE(first.Initialize().has_value());
  ASSERT_FALSE(first.Session().empty());
  std::string const held{ first.Session() };

  McpClient second{ serving.Talking() };
  ASSERT_TRUE(second.Initialize().has_value());
  EXPECT_NE(second.Session(), held);

  // The one-shot's whole life, between two calls of the client that stays.
  EXPECT_TRUE(first.Call(tash::mcp::METHOD_PING, "{}").has_value());
  EXPECT_TRUE(second.Call(tash::mcp::METHOD_PING, "{}").has_value());
  EXPECT_TRUE(second.End().has_value());

  auto const after{ first.Call(tash::mcp::METHOD_PING, "{}") };
  EXPECT_TRUE(after.has_value()) << (after ? "" : after.error());
  EXPECT_EQ(first.Session(), held);
  EXPECT_TRUE(first.End().has_value());
}

TEST(McpServer, AnIdTheServerNeverMintedIsNotFound)
{
  Serving serving;
  ASSERT_NE(serving.Port(), 0);

  McpClient held{ serving.Talking() };
  ASSERT_TRUE(held.Initialize().has_value());

  McpClient stranger{ serving.Talking() };
  auto const posted{ stranger.Post(
    Ping(), { oxbox::http::Header{
                std::string{ tash::mcp::SESSION_HEADER }, STRANGER } }) };
  ASSERT_TRUE(posted.has_value()) << (posted ? "" : posted.error());
  EXPECT_EQ(posted->status, GONE);

  // No id at all, once a session lives, is the other refusal the spec asks
  // for: a request the server cannot place.
  auto const bare{ stranger.Post(Ping()) };
  ASSERT_TRUE(bare.has_value()) << (bare ? "" : bare.error());
  EXPECT_EQ(bare->status, REFUSED);

  EXPECT_TRUE(held.Call(tash::mcp::METHOD_PING, "{}").has_value());
  EXPECT_TRUE(held.End().has_value());
}

TEST(McpServer, TheFirstSessionIsServedWithNoIdOfItsOwn)
{
  Serving serving;
  ASSERT_NE(serving.Port(), 0);

  McpClient talking{ serving.Talking() };
  auto const posted{ talking.Post(Ping()) };
  ASSERT_TRUE(posted.has_value()) << (posted ? "" : posted.error());
  EXPECT_EQ(posted->status, SERVED);
}

TEST(McpServer, ASessionThatGoesQuietIsDroppedAsADeleteDropsOne)
{
  Serving serving{ std::chrono::duration_cast<std::chrono::seconds>(
    IDLE_TEST) };
  ASSERT_NE(serving.Port(), 0);

  McpClient talking{ serving.Talking() };
  ASSERT_TRUE(talking.Initialize().has_value());
  EXPECT_TRUE(talking.Call(tash::mcp::METHOD_PING, "{}").has_value());

  std::this_thread::sleep_for(IDLE_TEST + SWEEP_GRACE);

  auto const after{ talking.Post(
    Ping(), { oxbox::http::Header{ std::string{ tash::mcp::SESSION_HEADER },
                                   talking.Session() } }) };
  ASSERT_TRUE(after.has_value()) << (after ? "" : after.error());
  EXPECT_EQ(after->status, GONE);

  // The set is empty again, so the next caller is the first caller.
  McpClient fresh{ serving.Talking() };
  auto const bare{ fresh.Post(Ping()) };
  ASSERT_TRUE(bare.has_value()) << (bare ? "" : bare.error());
  EXPECT_EQ(bare->status, SERVED);
}

TEST(McpServer, ASessionStillCallingIsNotDropped)
{
  Serving serving{ std::chrono::duration_cast<std::chrono::seconds>(
    IDLE_TEST) };
  ASSERT_NE(serving.Port(), 0);

  McpClient talking{ serving.Talking() };
  ASSERT_TRUE(talking.Initialize().has_value());
  for (int spoken{ 0 }; spoken < CALLS_OVER_IDLE; ++spoken)
  {
    std::this_thread::sleep_for(IDLE_TEST / 2);
    EXPECT_TRUE(talking.Call(tash::mcp::METHOD_PING, "{}").has_value())
      << spoken;
  }
  EXPECT_TRUE(talking.End().has_value());
}

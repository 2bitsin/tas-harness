#include "tash/mcp/mcp-server.hpp"

#include "tash/mcp/json-rpc.hpp"
#include "tash/mcp/protocol.hpp"

#include <oxbox/http/asio.hpp>

#include <boost/asio/as_tuple.hpp>
#include <boost/system/error_code.hpp>

#include <algorithm>
#include <chrono>
#include <exception>
#include <format>
#include <map>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tash::mcp::detail::mcp_server
{
  namespace asio = boost::asio;
  namespace http = oxbox::http;

  using utilities::Refused;

  namespace
  {
    using protocol::JSON_MEDIA_TYPE;
    using protocol::SSE_MEDIA_TYPE;
    inline constexpr std::string_view NO_STORE{ "no-store" };
    inline constexpr std::string_view CONTENT_TYPE{ "Content-Type" };
    inline constexpr std::string_view CACHE_CONTROL{ "Cache-Control" };
    inline constexpr std::string_view ACCEPT{ "Accept" };
    inline constexpr std::string_view NO_ID{ "null" };
    inline constexpr std::string_view EMPTY_RESULT{ "{}" };

    inline constexpr int SERVED{ 200 };
    inline constexpr int TAKEN{ 202 };
    inline constexpr int REFUSED{ 400 };
    inline constexpr int GONE{ 404 };

    // How often a held stream looks up to see whether it should wind up.
    inline constexpr std::chrono::milliseconds STREAM_BEAT{ 200 };

    // An expiry is noticed within a tenth of the time it takes, and the
    // sweep never wakes more than ten times a second to notice it.
    inline constexpr int SWEEP_PARTS{ 10 };
    inline constexpr std::chrono::milliseconds SWEEP_FLOOR{ 100 };

    [[nodiscard]] auto Beat(std::chrono::seconds idle)
      -> std::chrono::milliseconds
    {
      return std::max(SWEEP_FLOOR,
                      std::chrono::duration_cast<std::chrono::milliseconds>(
                        idle) / SWEEP_PARTS);
    }

    [[nodiscard]] auto NewSessionId() -> std::string
    {
      std::random_device entropy;
      std::mt19937_64    bits{ entropy() };
      return std::format("{:016x}{:016x}", bits(), bits());
    }

    [[nodiscard]] auto Accepts(http::ServerRequest const& request,
                               std::string_view type) -> bool
    {
      std::optional<std::string_view> const said{ request[ACCEPT] };
      return said && said->find(type) != std::string_view::npos;
    }

    [[nodiscard]] auto Headed(std::string_view type, std::string_view session)
      -> http::FieldTable
    {
      http::FieldTable fields;
      fields.set(CONTENT_TYPE, type);
      fields.set(CACHE_CONTROL, NO_STORE);
      if (!session.empty())
        fields.set(protocol::SESSION_HEADER, session);
      return fields;
    }

    [[nodiscard]] auto EventOf(std::string_view body) -> std::string
    {
      return std::format("event: message\ndata: {}\n\n", body);
    }

    [[nodiscard]] auto Complaint(int code, std::string text) -> std::string
    {
      return json_rpc::ErrorMessage(
        NO_ID, json_rpc::RpcError{ code, std::move(text), std::nullopt });
    }

    [[nodiscard]] auto Named(std::optional<std::string_view> given)
      -> std::string
    {
      return given ? std::string{ *given } : std::string{ };
    }
  }

  struct McpServer::State
  {
    using Clock = std::chrono::steady_clock;

    dispatcher::Dispatcher*     answering;
    McpOptions                  options;
    asio::io_context            loop{ 1 };
    std::optional<http::Server> server{ };

    // Every live session and when it last asked for anything: one agent's
    // client and a one-shot `tash session` share the port, so one
    // initialize never ends another's session.
    std::map<std::string, Clock::time_point> sessions{ };

    std::optional<asio::steady_timer> sweeping{ };
    bool                              started{ false };
    bool                              stopping{ false };

    // True once a run has been open here, so a server still waiting for its
    // first launch is not one whose run has been shut down.
    bool                              served{ false };

    auto Note(std::string const& session) -> void
    {
      served = served || answering->Serving();
      if (auto const held{ sessions.find(session) }; held != sessions.end())
        held->second = Clock::now();
    }

    auto Ended(std::string const& session) -> void
    {
      sessions.erase(session);

      // The lifecycle gate is the server's, so it reopens only once the
      // last session that passed it has gone.
      if (sessions.empty())
        answering->Reset();
    }

    [[nodiscard]] auto Quiet() -> std::vector<std::string>
    {
      std::vector<std::string> gone;
      Clock::time_point const cut{ Clock::now() - options.idle };
      for (auto const& [session, spoke] : sessions)
        if (spoke < cut)
          gone.push_back(session);
      return gone;
    }

    // A run that was opened here and is closed again, with nobody left.
    [[nodiscard]] auto Spent() const -> bool
    {
      return served && sessions.empty() && !answering->Serving();
    }

    // Posted whole: Stop() is called from whatever thread built the server,
    // and everything here belongs to the one the loop runs on.
    auto Halt() -> void
    {
      asio::post(loop, [this]
                       {
                         if (stopping)
                           return;
                         stopping = true;
                         if (sweeping)
                           sweeping->cancel();
                         server->Stop();
                       });
    }
  };

  McpServer::McpServer(dispatcher::Dispatcher& answering, McpOptions options)
  : _state{ std::make_unique<State>(&answering, std::move(options)) }
  {
    http::ServerOptions listening;
    listening.host = _state->options.host;
    listening.port = _state->options.port;
    _state->server.emplace(_state->loop.get_executor(), listening);
  }

  McpServer::~McpServer() = default;

  auto McpServer::Open(dispatcher::Dispatcher& answering, McpOptions options)
    -> Result<std::unique_ptr<McpServer>>
  {
    try
    {
      return std::make_unique<McpServer>(answering, std::move(options));
    }
    catch (std::exception const& failure)
    {
      return Refused("mcp: cannot serve: {}", failure.what());
    }
  }

  auto McpServer::Port() const -> std::uint16_t
  {
    return _state->server->Port();
  }

  auto McpServer::Start() -> void
  {
    if (_state->started)
      return;
    _state->started = true;
    State* const state{ _state.get() };

    state->server->OnStream(
      http::Method::post, std::string{ protocol::MCP_PATH },
      [state](http::ServerRequest const& request,
              http::ResponseStream& stream) -> asio::awaitable<void>
      {
        std::string session{ Named(request[protocol::SESSION_HEADER]) };
        Result<json_rpc::RpcMessage> const message{
          json_rpc::MessageFrom(request.body) };
        if (!message)
        {
          std::string const body{
            Complaint(protocol::PARSE_ERROR, message.error()) };
          co_await stream.Begin(REFUSED, Headed(JSON_MEDIA_TYPE, session));
          co_await stream.Write(body);
          co_await stream.End();
          co_return;
        }

        bool const opening{ message->method
                            && *message->method
                                 == protocol::METHOD_INITIALIZE };

        if (opening)
        {
          session = NewSessionId();
          state->sessions.emplace(session, State::Clock::now());
        }
        else if (!state->sessions.empty() && session.empty())
        {
          std::string const body{ Complaint(
            protocol::INVALID_REQUEST,
            "mcp: no session: this session wants its Mcp-Session-Id back") };
          co_await stream.Begin(REFUSED, Headed(JSON_MEDIA_TYPE, ""));
          co_await stream.Write(body);
          co_await stream.End();
          co_return;
        }
        else if (!session.empty() && !state->sessions.contains(session))
        {
          co_await stream.Begin(GONE, Headed(JSON_MEDIA_TYPE, ""));
          co_await stream.End();
          co_return;
        }

        dispatcher::Answered const answered{
          state->answering->Handle(*message) };
        state->Note(session);
        std::string const id{ json_rpc::IdText(message->id) };

        if (!message->id)
        {
          std::string const body{
            answered.failed ? json_rpc::ErrorMessage(id, *answered.failed)
                            : std::string{ } };
          co_await stream.Begin(answered.failed ? REFUSED : TAKEN,
                                Headed(JSON_MEDIA_TYPE, session));
          if (!body.empty())
            co_await stream.Write(body);
          co_await stream.End();
          co_return;
        }

        std::string const answer{
          answered.failed
            ? json_rpc::ErrorMessage(id, *answered.failed)
            : json_rpc::ResultMessage(
                id, answered.result.value_or(std::string{ EMPTY_RESULT })) };
        bool const        streaming{ Accepts(request, SSE_MEDIA_TYPE) };
        std::string const body{ streaming ? EventOf(answer) : answer };

        co_await stream.Begin(
          SERVED, Headed(streaming ? SSE_MEDIA_TYPE : JSON_MEDIA_TYPE,
                         session));
        co_await stream.Write(body);
        co_await stream.End();
      });

    state->server->OnStream(
      http::Method::get, std::string{ protocol::MCP_PATH },
      [state](http::ServerRequest const& request,
              http::ResponseStream& stream) -> asio::awaitable<void>
      {
        std::string const session{
          Named(request[protocol::SESSION_HEADER]) };
        if (!session.empty() && !state->sessions.contains(session))
        {
          co_await stream.Begin(GONE, Headed(JSON_MEDIA_TYPE, ""));
          co_await stream.End();
          co_return;
        }

        // v1 answers rather than speaks, so this carries only the comments
        // that keep an idle stream alive (transports 2.2, SSE 9.2.6).
        co_await stream.Begin(SERVED, Headed(SSE_MEDIA_TYPE, session));
        asio::steady_timer beating{ co_await asio::this_coro::executor };
        while (stream.Open())
        {
          beating.expires_after(STREAM_BEAT);
          auto const [failed]{
            co_await beating.async_wait(asio::as_tuple(asio::deferred)) };
          if (failed || !stream.Open())
            break;
          co_await stream.Write(": tash\n\n");

          // A stream the client is still reading is a client still there.
          state->Note(session);
        }
        co_await stream.End();
      });

    state->server->On(
      http::Method::delete_, std::string{ protocol::MCP_PATH },
      [state](http::ServerRequest const& request) -> http::ServerResponse
      {
        std::string const session{
          Named(request[protocol::SESSION_HEADER]) };
        if (!state->sessions.empty()
            && (session.empty() || !state->sessions.contains(session)))
          return http::ServerResponse{ GONE, { }, { }, { } };
        state->Ended(session);
        if (state->Spent())
          state->Halt();
        return http::ServerResponse{ SERVED, { }, { }, { } };
      });

    state->server->Start();
    state->sweeping.emplace(state->loop.get_executor());
    Sweep();
  }

  auto McpServer::Sweep() -> void
  {
    State* const state{ _state.get() };
    state->sweeping->expires_after(Beat(state->options.idle));
    state->sweeping->async_wait(
      [this, state](boost::system::error_code const& failed)
      {
        if (failed || state->stopping)
          return;
        for (std::string const& gone : state->Quiet())
          state->Ended(gone);
        if (state->Spent())
        {
          state->Halt();
          return;
        }
        Sweep();
      });
  }

  auto McpServer::Run() -> void
  {
    _state->loop.run();
  }

  auto McpServer::Stop() -> void
  {
    _state->Halt();
  }
}

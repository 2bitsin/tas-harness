#include "_recording.hpp"

#include "_bundle-names.hpp"
#include "_manifest-text.hpp"

#include <xxhash.h>

#include <cstring>
#include <format>
#include <utility>

namespace tash::linked::detail::recording
{
  using utilities::Forwarded;

  namespace
  {
    constexpr std::string_view LIBRARY{ "libtash" };
    constexpr std::string_view RECORDED{ "recorded" };

    [[nodiscard]] auto PixelBytes(harness::Pixels format) -> std::size_t
    {
      switch (format)
      {
        case harness::Pixels::RGB565:   return 2u;
        case harness::Pixels::RGB888:   return 3u;
        case harness::Pixels::XRGB8888:
        case harness::Pixels::RGBA8888: return 4u;
      }
      return 0u;
    }

    // The digest the harness computes for the same frame: xxh3 over each
    // row's visible bytes, with no padding and no geometry in it.
    [[nodiscard]] auto Hashed(harness::Video const& video) -> std::uint64_t
    {
      std::size_t const row{ PixelBytes(video.format) * video.width };
      auto const* bytes{
        reinterpret_cast<unsigned char const*>(video.pixels.data()) };
      if (video.pitch == row)
        return XXH3_64bits(bytes, row * video.height);

      XXH3_state_t* const state{ XXH3_createState() };
      if (state == nullptr || XXH3_64bits_reset(state) == XXH_ERROR)
        return 0u;
      for (std::uint32_t line{ 0 }; line < video.height; ++line)
        XXH3_64bits_update(state, bytes + std::size_t{ line } * video.pitch,
                           row);
      std::uint64_t const digest{ XXH3_64bits_digest(state) };
      XXH3_freeState(state);
      return digest;
    }

    template <typename Number>
    [[nodiscard]] auto Read(void const* address) -> std::int64_t
    {
      Number held{ };
      std::memcpy(&held, address, sizeof(Number));
      return static_cast<std::int64_t>(held);
    }

    [[nodiscard]] auto Valued(void const* address, harness::Number number)
      -> std::int64_t
    {
      switch (number)
      {
        case harness::Number::U8:  return Read<std::uint8_t>(address);
        case harness::Number::I8:  return Read<std::int8_t>(address);
        case harness::Number::U16: return Read<std::uint16_t>(address);
        case harness::Number::I16: return Read<std::int16_t>(address);
        case harness::Number::U32: return Read<std::uint32_t>(address);
        case harness::Number::I32: return Read<std::int32_t>(address);
        case harness::Number::U64: return Read<std::uint64_t>(address);
        case harness::Number::I64: return Read<std::int64_t>(address);
      }
      return 0;
    }
  }

  Recording::Recording(harness::Options const& options,
                       byte_sink::ByteSink tape, byte_sink::ByteSink trace,
                       byte_sink::ByteSink manifest)
    : _note{ options.note },
      _target{ options.target }, _profile{ options.profile },
      _fps{ options.fps },
      _tape{ _target, _profile },
      _tape_sink{ std::move(tape) },
      _trace{ std::move(trace), std::string{ LIBRARY } },
      _manifest{ std::move(manifest) }
  { }

  auto Recording::InDirectory(harness::Options const& options,
                              std::filesystem::path directory)
    -> Result<std::unique_ptr<Recording>>
  {
    std::error_code failed;
    std::filesystem::create_directories(directory, failed);
    if (failed)
      return utilities::Refused("libtash: cannot make '{}': {}",
                                directory.string(), failed.message());

    Result<byte_sink::ByteSink> tape{
      byte_sink::ByteSink::File(directory / bundle_names::TAPE_NAME) };
    if (!tape)
      return Forwarded(tape);
    Result<byte_sink::ByteSink> trace{
      byte_sink::ByteSink::File(directory / bundle_names::TRACE_NAME) };
    if (!trace)
      return Forwarded(trace);
    Result<byte_sink::ByteSink> manifest{
      byte_sink::ByteSink::File(directory / bundle_names::MANIFEST_NAME) };
    if (!manifest)
      return Forwarded(manifest);

    return std::unique_ptr<Recording>{ new Recording{
      options, std::move(*tape), std::move(*trace), std::move(*manifest) } };
  }

  auto Recording::InMemory(harness::Options const& options)
    -> std::unique_ptr<Recording>
  {
    return std::unique_ptr<Recording>{ new Recording{
      options, byte_sink::ByteSink::Memory(), byte_sink::ByteSink::Memory(),
      byte_sink::ByteSink::Memory() } };
  }

  Recording::~Recording()
  {
    Close();
    Write();
    _trace.Flush();
    if (!_trace.Refusal().empty())
      Note(_trace.Refusal());
  }

  auto Recording::Note(std::string_view text) const -> void
  {
    if (_note)
      _note(text);
  }

  auto Recording::Elapsed() const -> std::int64_t
  {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now() - _started).count();
  }

  auto Recording::Begin() -> void
  {
    Close();
    _open = true;
  }

  auto Recording::Close() -> void
  {
    if (!_open)
      return;
    _trace.Frame(_frame, Elapsed(), _hash);
    for (std::uint32_t at{ 0 }; at < _watches.size(); ++at)
    {
      Watched const& watch{ _watches[at] };
      _trace.Watch(_frame, at,
                   watch.reader ? watch.reader()
                                : Valued(watch.address, watch.number));
    }
    _hash = 0u;
    _open = false;
    ++_frame;
    if (_frame % REWRITE_FRAMES == 0u)
      Write();
  }

  auto Recording::Input(harness::Input const& input) -> void
  {
    _tape.Input(_frame, input);
    if (_tape.Keys() && !_said_keys)
    {
      _said_keys = true;
      Note("libtash: a tape holds pads and pointers, so the keys this "
           "target samples are not in it");
    }
  }

  auto Recording::Video(harness::Video const& video) -> void
  {
    if (video.pixels.empty() || PixelBytes(video.format) == 0u)
      return;
    std::size_t const wanted{ video.pitch * video.height };
    if (video.pixels.size() < wanted)
    {
      Note(std::format("libtash: a {}x{} frame of {} bytes needs {}",
                       video.width, video.height, video.pixels.size(),
                       wanted));
      return;
    }
    _hash = Hashed(video);
  }

  // Nothing in the formats a recording writes holds a sample: the encoder
  // is the harness's, and this is here for the attached mode that has one.
  auto Recording::Audio(harness::Audio const&) -> void
  { }

  auto Recording::Watch(harness::Watch const& watch)
    -> std::optional<std::size_t>
  {
    if (watch.name.empty()
        || (watch.address == nullptr && !watch.reader))
      return std::nullopt;
    _named.emplace_back(watch.name);
    _watches.push_back(Watched{ watch.address, watch.number, watch.reader });
    return _watches.size() - 1u;
  }

  auto Recording::Keep(harness::State state) -> void
  {
    _state = std::move(state);
  }

  auto Recording::Event(std::string_view name, std::string_view text) -> void
  {
    if (name.empty())
      return;
    _trace.Event(_frame, name, text);
  }

  auto Recording::Decided(std::string_view text) -> void
  {
    _trace.Decision(_frame, LIBRARY, text);
  }

  auto Recording::Dt(double own) -> double
  {
    if (!std::exchange(_said_dt, true))
      Decided(std::format("dt {}", own));
    return own;
  }

  auto Recording::Seed(std::uint64_t own) -> std::uint64_t
  {
    if (!std::exchange(_said_seed, true))
      Decided(std::format("seed {}", own));
    return own;
  }

  auto Recording::Write() -> void
  {
    _tape_sink.Replace(_tape.Text(_frame));
    _manifest.Replace(manifest_text::ManifestText(manifest_text::Recorded{
      _target, _profile, _frame, _fps, _named, RECORDED }));
  }

  auto Recording::Flush() -> void
  {
    Close();
    Write();
    _trace.Flush();
  }

  auto Recording::Bytes(harness::Artifact which) const
    -> std::span<std::byte const>
  {
    switch (which)
    {
      case harness::Artifact::TAPE:     return _tape_sink.Bytes();
      case harness::Artifact::TRACE:    return _trace.Bytes();
      case harness::Artifact::MANIFEST: return _manifest.Bytes();
    }
    return { };
  }
}

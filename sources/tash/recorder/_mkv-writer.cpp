#include "tash/recorder/_mkv-writer.hpp"

#include "tash/bus/audio-ring.hpp"
#include "tash/recorder/_scanlines.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <span>
#include <utility>

namespace tash::recorder::detail::mkv_writer
{
  using utilities::Refused;

  namespace
  {
    constexpr char const* CONTAINER{ "matroska" };
    constexpr AVPixelFormat SOURCE_FORMAT{ AV_PIX_FMT_RGB565LE };
    constexpr AVPixelFormat LOSSY_FORMAT{ AV_PIX_FMT_YUV420P };
    constexpr AVPixelFormat LOSSLESS_FORMAT{ AV_PIX_FMT_BGR0 };
    constexpr int FFV1_VERSION_WITH_RGB{ 3 };
    // A run is 50 or 60 Hz and a core's fps is a measured double; a million
    // is far more denominator than any of them needs.
    constexpr int MAXIMUM_RATE_DENOMINATOR{ 1'000'000 };

    // Every frame is its own group of pictures: a clip cut out of the middle
    // of a run must not need a keyframe from before its first frame.
    constexpr int GOP_SIZE{ 1 };

    // A film is watched from its start and no clip is cut from it, so it
    // pays for none of that: a keyframe every so many seconds of run.
    [[nodiscard]] auto GopSizeOf(EncoderSettings const& settings, double fps)
      -> int
    {
      if (settings.keyframe_seconds <= 0 || fps <= 0.0)
        return GOP_SIZE;
      return std::max(1, static_cast<int>(std::llround(
                           fps * settings.keyframe_seconds)));
    }

    auto Configure(AVCodecContext& codec, EncoderSettings const& settings)
      -> void
    {
      if (settings.lossless)
      {
        codec.level = FFV1_VERSION_WITH_RGB;
        av_opt_set_int(codec.priv_data, "coder", 1, 0);
        av_opt_set_int(codec.priv_data, "context", 1, 0);
        return;
      }
      av_opt_set(codec.priv_data, "preset", settings.preset.c_str(), 0);
      av_opt_set_int(codec.priv_data, "crf", settings.crf, 0);
    }
  }

  MkvWriter::~MkvWriter()
  {
    if (_opened && !_finished)
      static_cast<void>(Finish());
    if (_format != nullptr && _format->pb != nullptr)
      avio_closep(&_format->pb);
  }

  auto MkvWriter::Open(std::filesystem::path const& path, std::uint32_t width,
                       std::uint32_t height, double fps,
                       std::uint32_t sample_rate,
                       EncoderSettings const& settings)
    -> Result<std::unique_ptr<MkvWriter>>
  {
    if (width == 0u || height == 0u)
      return Refused("recorder: a recording needs a width and a height");

    std::uint32_t const scale{ settings.scale == 0u ? 1u : settings.scale };
    std::unique_ptr<MkvWriter> writing{ new MkvWriter{ } };
    writing->_width         = width * scale;
    writing->_height        = height * scale;
    writing->_source_width  = width;
    writing->_source_height = height;
    writing->_scale         = scale;
    // FFV1 carries packed RGB, which has no luma plane to darken.
    writing->_scanlines     = settings.lossless ? 0.0
                                                : settings.scanline_darkening;
    writing->_path          = path;

    AVFormatContext* format{ nullptr };
    if (int const failed{ avformat_alloc_output_context2(
          &format, nullptr, CONTAINER, path.string().c_str()) };
        failed < 0 || format == nullptr)
      return Refused("recorder: cannot write matroska: {}",
                     libav::Worded(failed));
    writing->_format.reset(format);

    AVCodec const* const codec{ avcodec_find_encoder(
      settings.lossless ? AV_CODEC_ID_FFV1 : AV_CODEC_ID_H264) };
    if (codec == nullptr)
      return Refused("recorder: this ffmpeg has no {} encoder",
                     settings.lossless ? "ffv1" : "h264");

    writing->_video.reset(avcodec_alloc_context3(codec));
    if (writing->_video == nullptr)
      return Refused("recorder: cannot allocate the video encoder");

    AVCodecContext& video{ *writing->_video };
    video.width     = static_cast<int>(writing->_width);
    video.height    = static_cast<int>(writing->_height);
    video.pix_fmt   = settings.lossless ? LOSSLESS_FORMAT : LOSSY_FORMAT;
    video.time_base = AVRational{ 1, static_cast<int>(
      encoder_settings::NANOSECONDS_PER_SECOND) };
    video.gop_size  = GopSizeOf(settings, fps);
    // x264's lookahead and rate control, and matroska's default duration,
    // all read this; the timestamps stay harness time regardless.
    AVRational const rate{ fps > 0.0 ? av_d2q(fps, MAXIMUM_RATE_DENOMINATOR)
                                     : AVRational{ 0, 1 } };
    video.framerate = rate;
    if ((writing->_format->oformat->flags & AVFMT_GLOBALHEADER) != 0)
      video.flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    Configure(video, settings);

    if (int const failed{ avcodec_open2(&video, codec, nullptr) }; failed < 0)
      return Refused("recorder: cannot open the video encoder: {}",
                     libav::Worded(failed));

    writing->_video_stream = avformat_new_stream(writing->_format.get(),
                                                 nullptr);
    if (writing->_video_stream == nullptr)
      return Refused("recorder: cannot add the video track");
    writing->_video_stream->time_base      = video.time_base;
    writing->_video_stream->avg_frame_rate = rate;
    avcodec_parameters_from_context(writing->_video_stream->codecpar, &video);

    if (sample_rate != 0u)
    {
      AVCodec const* const pcm{ avcodec_find_encoder(AV_CODEC_ID_PCM_S16LE) };
      if (pcm == nullptr)
        return Refused("recorder: this ffmpeg has no pcm_s16le encoder");

      writing->_audio.reset(avcodec_alloc_context3(pcm));
      if (writing->_audio == nullptr)
        return Refused("recorder: cannot allocate the audio encoder");

      AVCodecContext& audio{ *writing->_audio };
      audio.sample_fmt  = AV_SAMPLE_FMT_S16;
      audio.sample_rate = static_cast<int>(sample_rate);
      audio.time_base   = AVRational{ 1, static_cast<int>(sample_rate) };
      av_channel_layout_default(&audio.ch_layout,
                                static_cast<int>(bus::AUDIO_CHANNELS));
      if ((writing->_format->oformat->flags & AVFMT_GLOBALHEADER) != 0)
        audio.flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

      if (int const failed{ avcodec_open2(&audio, pcm, nullptr) }; failed < 0)
        return Refused("recorder: cannot open the audio encoder: {}",
                       libav::Worded(failed));

      writing->_audio_stream = avformat_new_stream(writing->_format.get(),
                                                   nullptr);
      if (writing->_audio_stream == nullptr)
        return Refused("recorder: cannot add the audio track");
      writing->_audio_stream->time_base = audio.time_base;
      avcodec_parameters_from_context(writing->_audio_stream->codecpar,
                                      &audio);
    }

    if (int const failed{ avio_open(&writing->_format->pb,
                                    path.string().c_str(), AVIO_FLAG_WRITE) };
        failed < 0)
      return Refused("recorder: cannot create '{}': {}", path.string(),
                     libav::Worded(failed));

    if (int const failed{ avformat_write_header(writing->_format.get(),
                                                nullptr) };
        failed < 0)
      return Refused("recorder: cannot write the matroska header: {}",
                     libav::Worded(failed));
    writing->_opened = true;

    writing->_picture.reset(av_frame_alloc());
    writing->_packet.reset(av_packet_alloc());
    if (writing->_picture == nullptr || writing->_packet == nullptr)
      return Refused("recorder: cannot allocate a frame");
    writing->_picture->format = video.pix_fmt;
    writing->_picture->width  = video.width;
    writing->_picture->height = video.height;
    if (int const failed{ av_frame_get_buffer(writing->_picture.get(), 0) };
        failed < 0)
      return Refused("recorder: cannot allocate the picture: {}",
                     libav::Worded(failed));

    writing->_scaler.reset(sws_getContext(
      static_cast<int>(width), static_cast<int>(height), SOURCE_FORMAT,
      video.width, video.height, video.pix_fmt, SWS_POINT, nullptr, nullptr,
      nullptr));
    if (writing->_scaler == nullptr)
      return Refused("recorder: cannot convert rgb565 to the codec's pixels");

    return writing;
  }

  auto MkvWriter::Drain(libav::CodecHandle const& codec, AVStream const& stream)
    -> Outcome
  {
    while (true)
    {
      int const taken{ avcodec_receive_packet(codec.get(), _packet.get()) };
      if (taken == AVERROR(EAGAIN) || taken == AVERROR_EOF)
        return { };
      if (taken < 0)
        return Refused("recorder: the encoder refused a packet: {}",
                       libav::Worded(taken));

      av_packet_rescale_ts(_packet.get(), codec->time_base, stream.time_base);
      _packet->stream_index = stream.index;
      int const written{ av_interleaved_write_frame(_format.get(),
                                                    _packet.get()) };
      av_packet_unref(_packet.get());
      if (written < 0)
        return Refused("recorder: cannot write a packet: {}",
                       libav::Worded(written));
    }
  }

  auto MkvWriter::WriteVideo(std::span<std::uint8_t const> packed_rows,
                             std::uint32_t width, std::uint32_t height,
                             std::int64_t at) -> Outcome
  {
    // A Mega Drive changes video mode while it boots, so a run's first frames
    // are a different size from the rest: one geometry is kept and scaled to.
    if (width != _source_width || height != _source_height)
    {
      _scaler.reset(sws_getContext(
        static_cast<int>(width), static_cast<int>(height), SOURCE_FORMAT,
        static_cast<int>(_width), static_cast<int>(_height),
        static_cast<AVPixelFormat>(_picture->format), SWS_POINT, nullptr,
        nullptr, nullptr));
      if (_scaler == nullptr)
        return Refused("recorder: cannot take a {}x{} frame into a {}x{} "
                       "recording", width, height, _width, _height);
      _source_width  = width;
      _source_height = height;
    }

    if (int const failed{ av_frame_make_writable(_picture.get()) }; failed < 0)
      return Refused("recorder: cannot reuse the picture: {}",
                     libav::Worded(failed));

    std::uint8_t const* const rows{ packed_rows.data() };
    int const pitch{ static_cast<int>(width) * 2 };
    sws_scale(_scaler.get(), &rows, &pitch, 0, static_cast<int>(height),
              _picture->data, _picture->linesize);
    auto const stride{ static_cast<std::uint32_t>(_picture->linesize[0]) };
    scanlines::Darken(std::span{ _picture->data[0],
                                 std::size_t{ stride } * _height },
                      stride, _width, _height, _scale, _scanlines);
    _picture->pts = at;

    if (int const failed{ avcodec_send_frame(_video.get(), _picture.get()) };
        failed < 0)
      return Refused("recorder: the video encoder refused a frame: {}",
                     libav::Worded(failed));
    return Drain(_video, *_video_stream);
  }

  auto MkvWriter::WriteAudio(std::span<std::int16_t const> interleaved,
                             std::int64_t position) -> Outcome
  {
    if (_audio == nullptr || interleaved.empty())
      return { };

    auto const count{ static_cast<int>(interleaved.size()
                                       / bus::AUDIO_CHANNELS) };
    if (count == 0)
      return { };

    libav::FrameHandle block{ av_frame_alloc() };
    if (block == nullptr)
      return Refused("recorder: cannot allocate an audio frame");
    block->format      = _audio->sample_fmt;
    block->sample_rate = _audio->sample_rate;
    block->nb_samples  = count;
    av_channel_layout_copy(&block->ch_layout, &_audio->ch_layout);
    if (int const failed{ av_frame_get_buffer(block.get(), 0) }; failed < 0)
      return Refused("recorder: cannot allocate audio samples: {}",
                     libav::Worded(failed));

    std::memcpy(block->data[0], interleaved.data(),
                static_cast<std::size_t>(count) * bus::AUDIO_CHANNELS
                  * sizeof(std::int16_t));
    block->pts = position;

    if (int const failed{ avcodec_send_frame(_audio.get(), block.get()) };
        failed < 0)
      return Refused("recorder: the audio encoder refused a block: {}",
                     libav::Worded(failed));
    return Drain(_audio, *_audio_stream);
  }

  auto MkvWriter::Finish() -> Outcome
  {
    if (_finished)
      return { };
    _finished = true;

    if (int const failed{ avcodec_send_frame(_video.get(), nullptr) };
        failed < 0 && failed != AVERROR_EOF)
      return Refused("recorder: cannot flush the video encoder: {}",
                     libav::Worded(failed));
    if (Outcome const drained{ Drain(_video, *_video_stream) }; !drained)
      return drained;

    if (_audio != nullptr)
    {
      if (int const failed{ avcodec_send_frame(_audio.get(), nullptr) };
          failed < 0 && failed != AVERROR_EOF)
        return Refused("recorder: cannot flush the audio encoder: {}",
                       libav::Worded(failed));
      if (Outcome const drained{ Drain(_audio, *_audio_stream) }; !drained)
        return drained;
    }

    if (int const failed{ av_write_trailer(_format.get()) }; failed < 0)
      return Refused("recorder: cannot write the matroska trailer: {}",
                     libav::Worded(failed));

    // The trailer is the last byte, and avio keeps the rest of the file in a
    // buffer until the handle goes; a reader opening now must see all of it.
    if (int const failed{ avio_closep(&_format->pb) }; failed < 0)
      return Refused("recorder: cannot close '{}': {}", _path.string(),
                     libav::Worded(failed));
    return { };
  }
}

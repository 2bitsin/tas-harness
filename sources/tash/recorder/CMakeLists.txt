Init_submodule()
Link_dependencies(
  utilities          # Result: a failed recording is an answer, not a throw
  bus                # FrameView: the rows the video callback delivers
  perception         # Region: the crop a shot may be taken of
  trace              # VerdictRecord: what a verdicts line is made from
  oxbox::oxbox       # the yaml a manifest is, and the json a verdict is
  ffmpeg::avformat   # _mkv-writer.cpp: the matroska muxer
  ffmpeg::avcodec    # _mkv-writer.cpp: libx264, ffv1, pcm_s16le
  ffmpeg::avutil     # frames, channel layouts, the option API
  ffmpeg::swscale    # rgb565 to the codec's pixel format
  opencv_core        # frame-png.cpp: the packed Mat
  opencv_imgproc     # frame-png.cpp: cvtColor out of rgb565
  opencv_imgcodecs   # frame-png.cpp: imwrite
)

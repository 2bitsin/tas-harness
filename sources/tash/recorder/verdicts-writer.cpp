#include "tash/recorder/verdicts-writer.hpp"

#include <oxbox/serialization/io.hpp>

#include <exception>
#include <ios>
#include <utility>

namespace tash::recorder::detail::verdicts_writer
{
  using utilities::Refused;

  VerdictsWriter::VerdictsWriter(std::ofstream file,
                                 std::filesystem::path path)
  : _file{ std::move(file) }, _path{ std::move(path) }
  {
  }

  auto VerdictsWriter::Open(std::filesystem::path const& path)
    -> Result<VerdictsWriter>
  {
    std::ofstream file{ path, std::ios::app };
    if (!file)
      return Refused("recorder: cannot append to '{}'", path.string());
    return VerdictsWriter{ std::move(file), path };
  }

  auto VerdictsWriter::Append(trace::VerdictRecord const& verdict) -> Outcome
  {
    VerdictLine const line{ verdict.frame, verdict.name, verdict.passed,
                            verdict.text };
    std::string written{ };
    try
    {
      written = oxbox::serialization::ToJson(line);
    }
    catch (std::exception const& failure)
    {
      return Refused("recorder: cannot write a verdict as json: {}",
                     failure.what());
    }

    _file << written << '\n';
    _file.flush();
    if (!_file)
      return Refused("recorder: cannot append to '{}'", _path.string());
    ++_written;
    return { };
  }
}

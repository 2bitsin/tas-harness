#include "tash/tash/python-job.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace tash::cli::detail::python_job
{
  using utilities::Refused;

  namespace
  {
    inline constexpr unsigned CONTINUATION_MASK{ 0xc0u };
    inline constexpr unsigned CONTINUATION_BITS{ 0x80u };

    // A tail starts at a line where there is one, and never inside a
    // character: what it answers goes out as json, which must be utf-8.
    [[nodiscard]] auto TailFrom(std::string const& text, std::size_t tail)
      -> std::size_t
    {
      std::size_t at{ text.size() - tail };
      if (at == 0 || text[at - 1] == '\n')
        return at;
      if (std::size_t const line{ text.find('\n', at) };
          line != std::string::npos)
        return line + 1;
      while (at < text.size()
             && (static_cast<unsigned char>(text[at]) & CONTINUATION_MASK)
                  == CONTINUATION_BITS)
        ++at;
      return at;
    }
  }

  PythonJob::~PythonJob()
  {
    Wait();
  }

  auto PythonJob::Start(std::string name, Work work) -> Outcome
  {
    if (_thread.joinable())
      return Refused("python: {} has not been waited for", Name());

    {
      std::lock_guard const held{ _guard };
      _name = std::move(name);
      _printed.clear();
      _errored.clear();
      _answer = std::string{ };
    }
    _running.store(true, std::memory_order_release);

    _thread = std::thread{ [this, work = std::move(work)]
    {
      Result<std::string> answered{ work() };
      {
        std::lock_guard const held{ _guard };
        _answer = std::move(answered);
      }
      _running.store(false, std::memory_order_release);
    } };
    return { };
  }

  auto PythonJob::Name() const -> std::string
  {
    std::lock_guard const held{ _guard };
    return _name;
  }

  auto PythonJob::Printed(std::size_t tail) const -> std::string
  {
    std::lock_guard const held{ _guard };
    if (tail == 0 || _printed.size() <= tail)
      return _printed;
    return _printed.substr(TailFrom(_printed, tail));
  }

  auto PythonJob::Errored(std::size_t tail) const -> std::string
  {
    std::lock_guard const held{ _guard };
    if (tail == 0 || _errored.size() <= tail)
      return _errored;
    return _errored.substr(TailFrom(_errored, tail));
  }

  auto PythonJob::Answer() const -> Result<std::string>
  {
    std::lock_guard const held{ _guard };
    return _answer;
  }

  auto PythonJob::Print(std::string_view text) -> void
  {
    std::lock_guard const held{ _guard };
    _printed.append(text);
  }

  auto PythonJob::PrintError(std::string_view text) -> void
  {
    std::lock_guard const held{ _guard };
    _errored.append(text);
  }

  auto PythonJob::Wait() -> void
  {
    if (_thread.joinable())
      _thread.join();
  }
}

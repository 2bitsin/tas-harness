#include "tash/utilities/scratch-area.hpp"

#include <exception>

namespace tash::utilities::detail::scratch_area
{
  auto ScratchAreaOf(std::string_view purpose)
      -> Result<oxbox::platform::ScratchArea>
  {
    try
    {
      return oxbox::platform::ScratchArea{ purpose, SCRATCH_PROGRAM };
    }
    catch (std::exception const& refused)
    {
      // oxbox names the path and what the OS said; nothing to add.
      return Refused("{}", refused.what());
    }
  }
}

#pragma once
// OpenCV answers by throwing; the module answers with a refusal.

#include "tash/utilities/outcome.hpp"

#include <opencv2/core/utility.hpp>

#include <type_traits>
#include <utility>

namespace tash::perception::detail::opencv_guard
{
  using utilities::Refused;

  template <typename Work>
  [[nodiscard]] auto Guarded(Work&& work) -> std::invoke_result_t<Work>
  {
    try
    {
      return std::forward<Work>(work)();
    }
    catch (cv::Exception const& refusal)
    {
      return Refused("perception: opencv refused: {}", refusal.what());
    }
  }
}

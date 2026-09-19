#include "tash/python/bindings.hpp"

#include "tash/perception/change-amount.hpp"
#include "tash/perception/colour-count.hpp"
#include "tash/perception/frame-hash.hpp"
#include "tash/perception/perceptual-hash.hpp"
#include "tash/perception/region.hpp"
#include "tash/perception/rgb565-view.hpp"
#include "tash/perception/template-match.hpp"
#include "tash/tape/anchor-image.hpp"

#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace tash::python::detail::bindings
{
  namespace py = pybind11;

  using scenario_run::ScenarioRun;

  namespace
  {
    using Rectangle = std::array<std::uint32_t, 4>;
    using Rgb       = std::array<std::uint32_t, 3>;

    auto RegionOf(Rectangle const& rectangle) -> perception::Region
    {
      return perception::Region{ rectangle[0], rectangle[1], rectangle[2],
                                 rectangle[3] };
    }

    auto LatestOf(py::handle module) -> bus::FrameView
    {
      return Given(RunOf(module).Latest());
    }
  }

  auto BindPerception(py::module_& module) -> void
  {
    module.def("exact_hash", [module]
               { return Given(perception::ExactHash(LatestOf(module))); });

    module.def("difference_hash", [module]
               {
                 return Given(perception::DifferenceHash(LatestOf(module)));
               });

    module.def("perceptual_hash", [module]
               {
                 return Given(perception::PerceptualHash(LatestOf(module)));
               });

    module.def("change_from",
               [module](std::filesystem::path const& previous)
               {
                 tape::AnchorImage const before{
                   Given(tape::AnchorImage::Read(previous)) };
                 perception::ChangeAmount const change{ Given(
                   perception::ChangeBetween(
                     before.View(),
                     perception::ViewOf(LatestOf(module)))) };
                 py::dict amount;
                 amount["changed"] = change.changed_ratio;
                 amount["mean"] = change.mean_absolute_difference;
                 return amount;
               },
               py::arg("previous"));

    module.def("find",
               [module](std::filesystem::path const& pattern,
                        std::optional<Rectangle> const& region)
               {
                 tape::AnchorImage const wanted{
                   Given(tape::AnchorImage::Read(pattern)) };
                 bus::FrameView const frame{ LatestOf(module) };
                 perception::TemplateMatch const found{
                   region ? Given(perception::BestMatch(frame, wanted.View(),
                                                        RegionOf(*region)))
                          : Given(perception::BestMatch(frame,
                                                        wanted.View())) };
                 py::dict where;
                 where["score"] = found.score;
                 where["x"] = found.x;
                 where["y"] = found.y;
                 return where;
               },
               py::arg("pattern"),
               py::arg("region") = std::optional<Rectangle>{ });

    RunClass(module).def(
      "colours",
      [](ScenarioRun& self, std::string const& region,
         Rgb const& rgb, std::uint32_t within)
      {
        return Given(self.Colours(
          Given(perception::RegionFrom(region)),
          perception::Colour{ rgb[0], rgb[1], rgb[2] }, within));
      },
      py::arg("region"), py::arg("rgb"), py::arg("within") = 0u);
  }
}

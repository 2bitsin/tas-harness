#include "tash/session/rom-file.hpp"

#include <zip.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>

namespace tash::session::detail::rom_file
{
  using utilities::Outcome;
  using utilities::Result;
  using utilities::Refused;

  namespace
  {
    constexpr std::array<std::string_view, 4> CARTRIDGE_EXTENSIONS{
      ".bin", ".md", ".gen", ".smd" };

    auto Lowered(std::string_view text) -> std::string
    {
      std::string lowered{ text };
      std::ranges::transform(lowered, lowered.begin(),
                             [](unsigned char letter)
                             { return static_cast<char>(std::tolower(letter)); });
      return lowered;
    }

    auto IsCartridge(std::string_view member) -> bool
    {
      std::string const name{ Lowered(member) };
      return std::ranges::any_of(CARTRIDGE_EXTENSIONS,
                                 [&name](std::string_view extension)
                                 { return name.ends_with(extension); });
    }

    auto MemberName(std::string_view member) -> std::string
    {
      std::size_t const slash{ member.find_last_of('/') };
      return std::string{ slash == std::string_view::npos
                            ? member
                            : member.substr(slash + 1) };
    }

    auto Extracted(zip_t* archive, zip_int64_t index, zip_uint64_t size,
                   std::filesystem::path const& to) -> Outcome
    {
      zip_file_t* const member{ zip_fopen_index(archive, index, 0) };
      if (member == nullptr)
        return Refused("cannot read member {} of the archive", index);
      std::string content(size, '\0');
      zip_int64_t const read{ zip_fread(member, content.data(), size) };
      zip_fclose(member);
      if (read < 0 || static_cast<zip_uint64_t>(read) != size)
        return Refused("member {} is {} bytes but read {}", index, size, read);

      std::ofstream file{ to, std::ios::binary };
      if (!file.write(content.data(), static_cast<std::streamsize>(size)))
        return Refused("cannot write {}", to.string());
      return {};
    }
  }

  auto PreparedRom(std::filesystem::path const& rom,
                   std::filesystem::path const& scratch)
    -> Result<std::filesystem::path>
  {
    if (Lowered(rom.extension().string()) != ".zip")
    {
      if (!std::filesystem::is_regular_file(rom))
        return Refused("no ROM at {}", rom.string());
      return rom;
    }

    int failure{ 0 };
    zip_t* const archive{ zip_open(rom.c_str(), ZIP_RDONLY, &failure) };
    if (archive == nullptr)
      return Refused("cannot open {} as a zip (libzip error {})",
                     rom.string(), failure);

    zip_int64_t const members{ zip_get_num_entries(archive, 0) };
    for (zip_int64_t index{ 0 }; index < members; ++index)
    {
      zip_stat_t entry{};
      if (zip_stat_index(archive, index, 0, &entry) != 0 ||
          entry.name == nullptr || !IsCartridge(entry.name))
        continue;

      std::filesystem::path const to{ scratch / MemberName(entry.name) };
      Outcome const written{ Extracted(archive, index, entry.size, to) };
      zip_close(archive);
      if (!written)
        return std::unexpected{ written.error() };
      return to;
    }

    zip_close(archive);
    return Refused("{} holds no .bin, .md, .gen or .smd member", rom.string());
  }
}

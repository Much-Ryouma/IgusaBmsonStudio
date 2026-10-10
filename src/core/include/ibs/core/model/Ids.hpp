#pragma once

#include <compare>
#include <cstdint>
#include <functional>

namespace ibs::core {

template <class Tag>
struct Id {
    std::uint64_t value = 0;

    [[nodiscard]] constexpr bool isValid() const noexcept { return value != 0; }
    friend constexpr auto operator<=>(const Id&, const Id&) = default;
};

using AssetId = Id<struct AssetTag>;
using PlacementId = Id<struct PlacementTag>;
using CutPointId = Id<struct CutPointTag>;
using NoteId = Id<struct NoteTag>;

}  // namespace ibs::core

template <class Tag>
struct std::hash<ibs::core::Id<Tag>> {
    std::size_t operator()(const ibs::core::Id<Tag>& id) const noexcept { return std::hash<std::uint64_t>{}(id.value); }
};

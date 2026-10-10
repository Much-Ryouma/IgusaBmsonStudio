#include "ibs/core/mode/KeyMode.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>

namespace ibs::core {

KeyMode::KeyMode(std::string modeHint, std::string displayName, std::vector<LaneDefinition> lanes,
                 int bgmColumnStartX)
    : modeHint_(std::move(modeHint)),
      displayName_(std::move(displayName)),
      lanes_(std::move(lanes)),
      bgmColumnStartX_(bgmColumnStartX) {
    if (modeHint_.empty()) {
        throw std::invalid_argument("mode_hint must not be empty");
    }
    if (lanes_.empty()) {
        throw std::invalid_argument("mode '" + modeHint_ + "' must define at least one lane");
    }
    std::set<int> seen;
    for (const auto& lane : lanes_) {
        if (lane.x <= 0) {
            throw std::invalid_argument("mode '" + modeHint_ + "': lane x must be positive");
        }
        if (!seen.insert(lane.x).second) {
            throw std::invalid_argument("mode '" + modeHint_ + "': duplicated lane x=" + std::to_string(lane.x));
        }
    }
    if (bgmColumnStartX_ <= *seen.rbegin()) {
        throw std::invalid_argument("mode '" + modeHint_ + "': bgm_column_start_x must be greater than every lane x");
    }
}

bool KeyMode::isPlayable(int x) const {
    return std::any_of(lanes_.begin(), lanes_.end(), [x](const LaneDefinition& lane) { return lane.x == x; });
}

void KeyModeCatalog::add(KeyMode mode) {
    if (find(mode.modeHint()) != nullptr) {
        throw std::invalid_argument("mode already registered: " + mode.modeHint());
    }
    modes_.push_back(std::move(mode));
}

const KeyMode* KeyModeCatalog::find(const std::string& modeHint) const {
    const auto found = std::find_if(modes_.begin(), modes_.end(),
                                    [&](const KeyMode& mode) { return mode.modeHint() == modeHint; });
    return found == modes_.end() ? nullptr : &*found;
}

}  // namespace ibs::core

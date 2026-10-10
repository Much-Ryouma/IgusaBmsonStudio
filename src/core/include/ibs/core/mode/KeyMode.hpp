#pragma once

#include <optional>
#include <string>
#include <vector>

namespace ibs::core {

enum class LaneKind { White, Black, Scratch, Other };

struct LaneDefinition {
    int x = 0;
    std::string label;
    LaneKind kind = LaneKind::Other;
    double width = 1.0;
};

class KeyMode {
public:
    KeyMode(std::string modeHint, std::string displayName, std::vector<LaneDefinition> lanes, int bgmColumnStartX);

    [[nodiscard]] const std::string& modeHint() const noexcept { return modeHint_; }
    [[nodiscard]] const std::string& displayName() const noexcept { return displayName_; }
    [[nodiscard]] const std::vector<LaneDefinition>& lanes() const noexcept { return lanes_; }
    [[nodiscard]] int bgmColumnStartX() const noexcept { return bgmColumnStartX_; }

    [[nodiscard]] bool isPlayable(int x) const;

private:
    std::string modeHint_;
    std::string displayName_;
    std::vector<LaneDefinition> lanes_;
    int bgmColumnStartX_;
};

class KeyModeCatalog {
public:
    void add(KeyMode mode);
    [[nodiscard]] const KeyMode* find(const std::string& modeHint) const;
    [[nodiscard]] const std::vector<KeyMode>& modes() const noexcept { return modes_; }

private:
    std::vector<KeyMode> modes_;
};

}  // namespace ibs::core

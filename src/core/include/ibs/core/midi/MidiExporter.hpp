#pragma once

#include <string>
#include <vector>

#include "ibs/core/midi/MidiSequence.hpp"
#include "ibs/core/model/Project.hpp"
#include "ibs/core/validation/Issue.hpp"

namespace ibs::core {

struct MidiFilePlan {
    std::string suggestedName;
    MidiSequence sequence;
};

struct MidiExportResult {
    std::vector<MidiFilePlan> files;
    std::vector<Issue> issues;
};

[[nodiscard]] MidiExportResult buildCutPointMidi(const Project& project, const MidiProfile& profile);

[[nodiscard]] MidiExportResult buildTempoMapMidi(const Project& project, const MidiProfile& profile);

}  // namespace ibs::core

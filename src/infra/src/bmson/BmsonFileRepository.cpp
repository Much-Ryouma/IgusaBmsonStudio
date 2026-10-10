#include "ibs/infra/bmson/BmsonFileRepository.hpp"

#include "ibs/infra/fs/Utf8Path.hpp"

namespace ibs::infra {

BmsonParseResult loadBmsonFile(const std::filesystem::path& path) {
    return parseBmson(readTextFile(path));
}

void saveBmsonFile(const std::filesystem::path& path, const core::bmson::Document& document) {
    writeTextFileAtomically(path, serializeBmson(document));
}

}  // namespace ibs::infra

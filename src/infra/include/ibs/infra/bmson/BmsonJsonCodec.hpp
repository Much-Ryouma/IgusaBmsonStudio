#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "ibs/core/bmson/BmsonDocument.hpp"
#include "ibs/core/validation/Issue.hpp"

namespace ibs::infra {

class BmsonFormatError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct BmsonParseResult {
    core::bmson::Document document;
    std::vector<core::Issue> notices;
};

[[nodiscard]] BmsonParseResult parseBmson(const std::string& text);

[[nodiscard]] std::string serializeBmson(const core::bmson::Document& document);

}  // namespace ibs::infra

#ifndef GOOGLE_INPUT_TOOLS_EMERGENCY_API_HPP
#define GOOGLE_INPUT_TOOLS_EMERGENCY_API_HPP

#include <functional>
#include <string>
#include <vector>

namespace google_input_tools {

// Asynchronously requests candidates from Google Input Tools API strictly as fallback
// when local dictionary has no match (local_hit == false).
void maybe_request_emergency_candidates(
    const std::string& roman_word,
    bool local_hit,
    std::function<void(const std::string&, const std::vector<std::string>&)> on_result);

// Synchronously fetches candidates from Google Input Tools API (fallback autocomplete).
std::vector<std::string> fetch_api_candidates_sync(const std::string& roman_word, int timeout_seconds = 2);

} // namespace google_input_tools

#endif

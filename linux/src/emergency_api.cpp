#include "emergency_api.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <sstream>
#include <iomanip>

namespace google_input_tools {

static bool is_devanagari_byte(unsigned char c) {
    return c >= 0xE0; // Devanagari UTF-8 starts with E0 A4 / E0 A5
}

static std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
        }
    }
    return escaped.str();
}

static std::vector<std::string> parse_inputtools_body(const std::string& body) {
    std::vector<std::string> out;
    if (body.find("SUCCESS") == std::string::npos) {
        return out;
    }

    size_t i = 0;
    while (i < body.size() && out.size() < 6) {
        if (body[i] != '"') {
            ++i;
            continue;
        }
        ++i;
        std::string tok;
        while (i < body.size() && body[i] != '"') {
            if (body[i] == '\\' && i + 1 < body.size()) {
                tok.push_back(body[i + 1]);
                i += 2;
                continue;
            }
            tok.push_back(body[i++]);
        }
        if (i < body.size() && body[i] == '"') ++i;
        if (!tok.empty() && is_devanagari_byte(static_cast<unsigned char>(tok[0]))) {
            if (std::find(out.begin(), out.end(), tok) == out.end()) {
                out.push_back(tok);
            }
        }
    }
    return out;
}

static std::string query_api(const std::string& roman_word, int timeout_secs) {
    std::string enc = url_encode(roman_word);
    std::string url = "https://inputtools.google.com/request?itc=hi-t-i0-und&num=5&cp=0&cs=1&ie=utf-8&oe=utf-8&text=" + enc;
    std::string cmd = "/usr/bin/curl -s --connect-timeout 2 --max-time " + std::to_string(timeout_secs) +
                      " -A \"google-input-tools-linux/1.0\" \"" + url + "\" 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    char buffer[512];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

static std::mutex g_mu;
static std::unordered_set<std::string> g_inflight;

void maybe_request_emergency_candidates(
    const std::string& roman_word,
    bool local_hit,
    std::function<void(const std::string&, const std::vector<std::string>&)> on_result) {

    // Strictly fallback: if local dictionary has a hit, do NOT call API!
    if (local_hit || roman_word.empty() || !on_result) return;

    // Optional user opt-out: GIT_DISABLE_API=1
    const char* disable = std::getenv("GIT_DISABLE_API");
    if (disable && (std::strcmp(disable, "1") == 0 || std::strcmp(disable, "true") == 0)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_mu);
        if (g_inflight.count(roman_word)) return;
        g_inflight.insert(roman_word);
    }

    std::thread([roman_word, on_result]() {
        std::string body = query_api(roman_word, 2);
        if (!body.empty()) {
            auto cands = parse_inputtools_body(body);
            if (!cands.empty()) {
                on_result(roman_word, cands);
            }
        }
        std::lock_guard<std::mutex> lock(g_mu);
        g_inflight.erase(roman_word);
    }).detach();
}

std::vector<std::string> fetch_api_candidates_sync(const std::string& roman_word, int timeout_seconds) {
    if (roman_word.empty()) return {};
    std::string body = query_api(roman_word, timeout_seconds);
    return parse_inputtools_body(body);
}

} // namespace google_input_tools

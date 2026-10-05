#include "engine.hpp"
#include "dict_trie.hpp"
#include "emergency_api.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <iomanip>

using namespace google_input_tools;

static void print_candidates(TransliterationEngine& engine, const std::string& word) {
    std::cout << "\nInput: [" << word << "]" << std::endl;
    auto cands = engine.get_candidates(word, 8);
    std::cout << "Top Autocomplete Candidates:" << std::endl;
    for (size_t i = 0; i < cands.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << cands[i] << std::endl;
    }
}

int main(int argc, char** argv) {
    TransliterationEngine engine("hi");

    if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1 == "--help" || arg1 == "-h") {
            std::cout << "Google Input Tools CLI (Linux Native, Headless)\n"
                      << "Usage:\n"
                      << "  git-cli <words...>\n"
                      << "  git-cli --cands <word>\n"
                      << "  git-cli --fallback <word>\n"
                      << "  git-cli                (interactive mode)\n";
            return 0;
        }

        if (arg1 == "--cands" && argc > 2) {
            print_candidates(engine, argv[2]);
            return 0;
        }

        if (arg1 == "--fallback" && argc > 2) {
            std::string word = argv[2];
            std::cout << "Querying Fallback Google Input Tools API for: [" << word << "]..." << std::endl;
            auto cands = fetch_api_candidates_sync(word, 3);
            if (cands.empty()) {
                std::cout << "No fallback candidates returned (offline or timeout).\n";
            } else {
                std::cout << "Fallback API Candidates:\n";
                for (size_t i = 0; i < cands.size(); ++i) {
                    std::cout << "  " << (i + 1) << ". " << cands[i] << std::endl;
                }
            }
            return 0;
        }

        // Transliterate full arguments text
        std::string full_input;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) full_input += " ";
            full_input += argv[i];
        }
        std::cout << engine.transliterate_text(full_input) << std::endl;
        return 0;
    }

    // Interactive terminal session
    std::cout << "=========================================================\n"
              << " Google Input Tools Linux - Interactive Terminal\n"
              << " 100% Native C++ (Headless, Zero GUI)\n"
              << " Type phonetically (e.g. 'namaste dosto'). Type 'exit' to quit.\n"
              << " Type '?<word>' to inspect all candidates.\n"
              << "=========================================================\n";

    std::string line;
    while (true) {
        std::cout << "\n(roman) > ";
        if (!std::getline(std::cin, line)) break;
        if (line == "exit" || line == "quit") break;
        if (line.empty()) continue;

        if (line[0] == '?') {
            std::string w = line.substr(1);
            print_candidates(engine, w);
            continue;
        }

        std::string hindi = engine.transliterate_text(line);
        std::cout << "(hindi) > " << hindi << std::endl;
    }

    return 0;
}

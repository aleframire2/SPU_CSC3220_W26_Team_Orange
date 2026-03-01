#include "export.h"
#include "database.h"
#include "colors.h"
#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>

static bool write_txt(const Story& s, const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << s.title << "\n";
    f << std::string(s.title.size(), '=') << "\n\n";
    f << "Created: " << s.created_at << "\n";
    f << "Words: "   << s.total_word_count << "  |  Time: " << s.total_time
      << "s  |  Flesch: " << std::fixed << std::setprecision(1) << s.flesch_score << "\n\n";
    f << s.content << "\n";
    return true;
}

static bool write_md(const Story& s, const std::vector<ChainWord>& chain, const std::string& path) {
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << "# " << s.title << "\n\n";
    f << "> *Created: " << s.created_at << "*\n\n";
    f << "| Words | Time (s) | Flesch Score |\n";
    f << "|-------|----------|--------------|\n";
    f << "| " << s.total_word_count << " | " << s.total_time << " | "
      << std::fixed << std::setprecision(1) << s.flesch_score << " |\n\n";

    if (!chain.empty()) {
        f << "## Word Chain\n\n";
        for (size_t i = 0; i < chain.size(); ++i) {
            if (i > 0) f << " -> ";
            f << "**" << chain[i].word_text << "**";
        }
        f << "\n\n";
    }

    f << "## Story\n\n";
    f << s.content << "\n";
    return true;
}

void run_export_menu(sqlite3* db, int user_id) {
    auto stories = story_list(db, user_id);
    if (stories.empty()) {
        std::cout << "  " CLR_DIM "No stories to export." CLR_RESET "\n";
        return;
    }

    std::cout << "\n" CLR_BLUE CLR_BOLD "=== Export Story ===" CLR_RESET "\n";
    for (auto& s : stories)
        std::cout << "  " CLR_GREEN "[" << s.story_id << "]" CLR_RESET " " << s.title << "\n";

    std::cout << "\n  Story ID to export: ";
    std::string id_str;
    std::getline(std::cin, id_str);
    int id = 0;
    try { id = std::stoi(id_str); } catch (...) { std::cout << "  " CLR_RED "Invalid ID." CLR_RESET "\n"; return; }

    Story s;
    if (!story_get(db, id, s) || s.user_id != user_id) {
        std::cout << "  " CLR_RED "Story not found." CLR_RESET "\n";
        return;
    }

    std::cout << "  Format -- " CLR_GREEN "1." CLR_RESET " .txt  " CLR_GREEN "2." CLR_RESET " .md : ";
    std::string fmt;
    std::getline(std::cin, fmt);

    std::string out_path = s.title;
    for (char& c : out_path) if (c == ' ' || c == '/') c = '_';

    bool ok = false;
    if (fmt == "2") {
        out_path += ".md";
        WordChain wc;
        std::vector<ChainWord> chain;
        if (wordchain_get_by_story(db, id, wc))
            chain = chainword_list(db, wc.chain_id);
        ok = write_md(s, chain, out_path);
    } else {
        out_path += ".txt";
        ok = write_txt(s, out_path);
    }

    if (ok) std::cout << "  " CLR_GREEN "Exported to: " << out_path << CLR_RESET "\n";
    else    std::cout << "  " CLR_RED "[!] Failed to write file." CLR_RESET "\n";
}

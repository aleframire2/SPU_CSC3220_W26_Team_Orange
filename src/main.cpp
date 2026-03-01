#include <iostream>
#include <string>
#include <limits>
#include "database.h"
#include "chain_engine.h"
#include "narrative.h"
#include "archive.h"
#include "analytics.h"
#include "wordpool.h"
#include "export.h"

// ── helpers ───────────────────────────────────────────────
static std::string prompt(const std::string& label) {
    std::cout << "  " << label << ": ";
    std::string s;
    std::getline(std::cin, s);
    return s;
}

static void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// ── auth ──────────────────────────────────────────────────
static bool do_login(sqlite3* db, User& user) {
    std::cout << "\n  First name: "; std::string fn; std::getline(std::cin, fn);
    std::cout << "  Last name:  "; std::string ln; std::getline(std::cin, ln);
    std::cout << "  Password:   "; std::string pw; std::getline(std::cin, pw);
    if (user_login(db, fn, ln, pw, user)) {
        std::cout << "  Welcome back, " << user.firstName << "!\n";
        return true;
    }
    std::cout << "  [!] Invalid credentials.\n";
    return false;
}

static bool do_register(sqlite3* db, User& user) {
    std::cout << "\n  First name: "; std::string fn; std::getline(std::cin, fn);
    std::cout << "  Last name:  "; std::string ln; std::getline(std::cin, ln);
    std::cout << "  Password:   "; std::string pw; std::getline(std::cin, pw);
    std::cout << "  Confirm:    "; std::string pw2; std::getline(std::cin, pw2);
    if (pw != pw2) { std::cout << "  [!] Passwords do not match.\n"; return false; }
    if (fn.empty() || ln.empty() || pw.empty()) { std::cout << "  [!] All fields required.\n"; return false; }

    int id = user_create(db, fn, ln, pw);
    if (!user_login(db, fn, ln, pw, user)) {
        std::cout << "  [!] Registration failed.\n";
        return false;
    }
    std::cout << "  Account created! Welcome, " << user.firstName << ".\n";
    return true;
}

// ── new session ───────────────────────────────────────────
static void new_session(sqlite3* db, const User& user) {
    std::cout << "\n  Timer seconds per word [default 10]: ";
    std::string t_str;
    std::getline(std::cin, t_str);
    int timer = 10;
    if (!t_str.empty()) { try { timer = std::stoi(t_str); } catch (...) {} }
    if (timer < 3)  timer = 3;
    if (timer > 60) timer = 60;

    std::cout << "  Minimum chain length [default 5]: ";
    std::string m_str;
    std::getline(std::cin, m_str);
    int min_chain = 5;
    if (!m_str.empty()) { try { min_chain = std::stoi(m_str); } catch (...) {} }
    if (min_chain < 1)  min_chain = 1;

    auto chain = run_chain_reaction(db, timer, min_chain);

    if (chain.empty()) {
        std::cout << "  Session ended with no words.\n";
        return;
    }

    std::cout << "\n  Proceed to Narrative Synthesis? (y/n): ";
    std::string ans;
    std::getline(std::cin, ans);
    if (ans == "y" || ans == "Y")
        run_narrative_session(db, user.user_id, chain);
}

// ── main menu ─────────────────────────────────────────────
static void main_menu(sqlite3* db, User& user) {
    while (true) {
        std::cout << "\n╔══════════════════════════════════════╗\n";
        std::cout << "║          NarrativeLink               ║\n";
        std::cout << "║  Hello, " << user.firstName
                  << std::string(28 - (int)user.firstName.size(), ' ') << "║\n";
        std::cout << "╠══════════════════════════════════════╣\n";
        std::cout << "║  [1] New Session                     ║\n";
        std::cout << "║  [2] Local Archive (CRUD)            ║\n";
        std::cout << "║  [3] Session Analytics               ║\n";
        std::cout << "║  [4] Word Pool Manager               ║\n";
        std::cout << "║  [5] Export Story                    ║\n";
        std::cout << "║  [q] Quit                            ║\n";
        std::cout << "╚══════════════════════════════════════╝\n  > ";

        std::string cmd;
        std::getline(std::cin, cmd);

        if (cmd == "1") new_session(db, user);
        else if (cmd == "2") run_archive(db, user.user_id);
        else if (cmd == "3") run_analytics(db, user.user_id);
        else if (cmd == "4") run_wordpool_menu(db);
        else if (cmd == "5") run_export_menu(db, user.user_id);
        else if (cmd == "q" || cmd == "Q") {
            std::cout << "  Goodbye!\n";
            break;
        }
    }
}

// ── entry point ───────────────────────────────────────────
int main() {
    // Resolve DB path relative to executable
    const std::string db_path = "narrativelink.db";

    sqlite3* db = nullptr;
    try {
        db = db_open(db_path);
        db_init_schema(db);
        seed_default_words(db);
    } catch (const std::exception& e) {
        std::cerr << "[FATAL] " << e.what() << "\n";
        return 1;
    }

    std::cout << "╔══════════════════════════════════════╗\n";
    std::cout << "║          NarrativeLink               ║\n";
    std::cout << "║   Gamified Creative Writing CLI      ║\n";
    std::cout << "╚══════════════════════════════════════╝\n";

    User user{};
    bool logged_in = false;

    while (!logged_in) {
        std::cout << "\n  [1] Login\n  [2] Register\n  [q] Quit\n  > ";
        std::string opt;
        std::getline(std::cin, opt);

        if (opt == "1")       logged_in = do_login(db, user);
        else if (opt == "2")  logged_in = do_register(db, user);
        else if (opt == "q" || opt == "Q") { db_close(db); return 0; }
    }

    main_menu(db, user);
    db_close(db);
    return 0;
}

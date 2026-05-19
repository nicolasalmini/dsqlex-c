#include "dsqlex/lexer.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <unordered_map>

namespace dsqlex {

namespace {

// Case-insensitive keyword/function lookup
struct CIHash {
    size_t operator()(std::string s) const {
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return std::hash<std::string>{}(s);
    }
};
struct CIEqual {
    bool operator()(std::string a, std::string b) const {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (std::toupper(a[i]) != std::toupper(b[i])) return false;
        return true;
    }
};

using WordMap = std::unordered_map<std::string, TokenType, CIHash, CIEqual>;

const WordMap& word_map() {
    static const WordMap m = {
        // Keywords
        {"SELECT", TokenType::Select},
        {"CASE",   TokenType::Case},
        {"WHEN",   TokenType::When},
        {"THEN",   TokenType::Then},
        {"ELSE",   TokenType::Else},
        {"END",    TokenType::End},
        {"AND",    TokenType::And},
        {"OR",     TokenType::Or},
        {"NOT",    TokenType::Not},
        {"NULL",   TokenType::Null},
        {"TRUE",   TokenType::True},
        {"FALSE",  TokenType::False},
        {"IS",     TokenType::Is},
        {"IN",     TokenType::In},
        {"LIKE",   TokenType::Like},
        // Functions
        {"UPPER",    TokenType::FnUpper},
        {"LOWER",    TokenType::FnLower},
        {"ROUND",    TokenType::FnRound},
        {"COALESCE", TokenType::FnCoalesce},
        {"NVL",      TokenType::FnCoalesce}, // alias
        {"ABS",      TokenType::FnAbs},
        {"CONCAT",   TokenType::FnConcat},
        {"EVENT",    TokenType::FnEvent},
    };
    return m;
}

bool is_ident_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool is_ident_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.';
}

} // anonymous namespace

std::vector<Token> tokenize(const std::string& input) {
    std::vector<Token> tokens;
    size_t pos = 0;
    const size_t len = input.size();

    while (pos < len) {
        char c = input[pos];

        // Skip whitespace
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++pos;
            continue;
        }

        // Line comments: -- or #
        if (c == '#' || (c == '-' && pos + 1 < len && input[pos + 1] == '-')) {
            while (pos < len && input[pos] != '\n') ++pos;
            continue;
        }

        // Block comments: /* ... */
        if (c == '/' && pos + 1 < len && input[pos + 1] == '*') {
            pos += 2;
            while (pos + 1 < len && !(input[pos] == '*' && input[pos + 1] == '/'))
                ++pos;
            if (pos + 1 >= len)
                throw std::runtime_error("Unterminated block comment");
            pos += 2;
            continue;
        }

        // Two-character operators
        if (pos + 1 < len) {
            char c2 = input[pos + 1];
            if (c == '!' && c2 == '=') { tokens.push_back({TokenType::Neq}); pos += 2; continue; }
            if (c == '<' && c2 == '=') { tokens.push_back({TokenType::Lte}); pos += 2; continue; }
            if (c == '>' && c2 == '=') { tokens.push_back({TokenType::Gte}); pos += 2; continue; }
        }

        // Single-character operators & structural
        switch (c) {
            case '+': tokens.push_back({TokenType::Plus});     ++pos; continue;
            case '-': tokens.push_back({TokenType::Minus});    ++pos; continue;
            case '*': tokens.push_back({TokenType::Multiply}); ++pos; continue;
            case '/': tokens.push_back({TokenType::Divide});   ++pos; continue;
            case '=': tokens.push_back({TokenType::Eq});       ++pos; continue;
            case '<': tokens.push_back({TokenType::Lt});       ++pos; continue;
            case '>': tokens.push_back({TokenType::Gt});       ++pos; continue;
            case '(': tokens.push_back({TokenType::LParen});   ++pos; continue;
            case ')': tokens.push_back({TokenType::RParen});   ++pos; continue;
            case ',': tokens.push_back({TokenType::Comma});    ++pos; continue;
            default: break;
        }

        // String literals: single-quoted
        if (c == '\'') {
            ++pos;
            std::string val;
            while (pos < len && input[pos] != '\'') {
                val += input[pos];
                ++pos;
            }
            if (pos >= len)
                throw std::runtime_error("Unterminated string");
            ++pos; // skip closing quote
            tokens.push_back({TokenType::String, std::move(val)});
            continue;
        }

        // Numbers
        if (std::isdigit(static_cast<unsigned char>(c))) {
            std::string num;
            while (pos < len && (std::isdigit(static_cast<unsigned char>(input[pos])) || input[pos] == '.'))
                num += input[pos++];
            tokens.push_back({TokenType::Number, std::move(num)});
            continue;
        }

        // Words: keywords, functions, identifiers
        if (is_ident_start(c)) {
            std::string word;
            while (pos < len && is_ident_char(input[pos]))
                word += input[pos++];

            auto& wm = word_map();
            auto it = wm.find(word);
            if (it != wm.end()) {
                tokens.push_back({it->second, word});
            } else {
                tokens.push_back({TokenType::Identifier, std::move(word)});
            }
            continue;
        }

        throw std::runtime_error(std::string("Unexpected character: '") + c + "'");
    }

    return tokens;
}

} // namespace dsqlex

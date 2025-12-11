#include "tokenizer.h"
#include <cctype>
#include <iostream>
#include <regex>
#include <unordered_map>

using namespace std;

static int g_lineNum = 1;
static string g_filename;

void syntaxError(string msg) { cerr << g_filename << ':' << g_lineNum << ' ' << msg << '\n'; }
void setFilename(string s) {
  g_filename = s;
  g_lineNum = 1;
}

static auto stringToKeyword(const string &keywordStr, TokenType defaultType = STRING) -> TokenType {
  // Map strings to token types; static so we only build it once

  static const unordered_map<string, TokenType> keywordMap = {
      {"true", TRUE},
      {"false", FALSE},
      {"null", NULL_TOKEN},

      {"ENTERS_BATTLEFIELD", ENTERS_BATTLEFIELD},
      {"DIES", DIES},
      {"ATTACKS", ATTACKS},
      {"DEALS_DAMAGE", DEALS_DAMAGE},
      {"DEALS_COMBAT_DAMAGE", DEALS_COMBAT_DAMAGE},
      {"BEGINNING_OF_UPKEEP", BEGINNING_OF_UPKEEP},
      {"END_OF_TURN", END_OF_TURN},
      {"SPELL_CAST", SPELL_CAST},
      {"BECOMES_TARGET", BECOMES_TARGET},

      {"SELF", SELF},
      {"ANY_CREATURE", ANY_CREATURE},
      {"ANOTHER_CREATURE", ANOTHER_CREATURE},
      {"CREATURE_YOU_CONTROL", CREATURE_YOU_CONTROL},
      {"CREATURE_OPPONENT_CONTROLS", CREATURE_OPPONENT_CONTROLS},
      {"ANY_PLAYER", ANY_PLAYER},

      {"DEAL_DAMAGE", DEAL_DAMAGE},
      {"GAIN_LIFE", GAIN_LIFE},
      {"LOSE_LIFE", LOSE_LIFE},
      {"DRAW_CARDS", DRAW_CARDS},
      {"DISCARD", DISCARD},
      {"DESTROY", DESTROY},
      {"SACRIFICE", SACRIFICE},
      {"EXILE", EXILE},
      {"ADD_COUNTERS", ADD_COUNTERS},
      {"REMOVE_COUNTERS", REMOVE_COUNTERS},
      {"CHANGE_POWER", CHANGE_POWER},
      {"CHANGE_TOUGHNESS", CHANGE_TOUGHNESS},
      {"TAP", TAP},
      {"UNTAP", UNTAP},
      {"CREATE_TOKEN", CREATE_TOKEN},
      {"SEARCH_LAND", SEARCH_LAND},
      {"MILL", MILL},
      {"BOUNCE", BOUNCE},
      {"COUNTERSPELL", COUNTERSPELL},

      {"NONE", NONE},
      {"ANY_TARGET", ANY_TARGET},
      {"CREATURE", CREATURE},
      {"PLAYER", PLAYER},
      {"OPPONENT", OPPONENT},
      {"EACH_OPPONENT", EACH_OPPONENT},
      {"CONTROLLER", CONTROLLER},
      {"PERMANENT", PERMANENT},
      {"SPELL", SPELL},
  };

  auto iter = keywordMap.find(keywordStr);

  if (iter != keywordMap.end()) {
    return iter->second;
  }

  return defaultType;
}

auto tokenTypeToString(TokenType tokenType) -> string {
  // Error handling

  static const unordered_map<TokenType, string> tokenMap = {
      {LBRACE, "'{'"},
      {RBRACE, "'}'"},
      {LBRACKET, "'['"},
      {RBRACKET, "']'"},
      {COLON, "':'"},
      {COMMA, "','"},
      {STRING, "string"},
      {NUMBER, "number"},
      {TRUE, "true"},
      {FALSE, "false"},
      {NULL_TOKEN, "null"},
      {END_OF_FILE, "EOF"},
      {ERROR_TOKEN, "error token"}
  };

  auto tokenIter = tokenMap.find(tokenType);
  if (tokenIter != tokenMap.end()) {
    return tokenIter->second;
  }
  return "unknown token";
}

void Tokenizer::advance() {
  // Advance one character and track line/column.

  if (pos < input.length()) {
    if (input[pos] == '\n') {
      line++;
      g_lineNum++;
      col = 1;
    } else {
      col++;
    }
    pos++;
  }
}

auto Tokenizer::current() const -> char {
  // Current character or '\0'

  if (pos >= input.length()) {
    return '\0';
  }
  return input[pos];
}

void Tokenizer::skipWhitespace() {
  while (current() != '\0' && isspace(static_cast<unsigned char>(current())) != 0) {
    advance();
  }
}

auto Tokenizer::getNext() -> Token {
  skipWhitespace();

  // Save position for error reporting
  int startLine = line;
  int startCol = col;

  // Check for end of input
  if (current() == '\0') {
    return {END_OF_FILE, "EOF", 0, startLine, startCol};
  }

  string remaining = input.substr(pos);

  // JSON punctuation via regex (as proof of concept)
  static const regex punctRe("^[\\{\\}\\[\\]:,]");
  smatch pm;
  if (regex_search(remaining, pm, punctRe)) {
    char c = pm.str()[0];
    pos += 1;
    col += 1;
    switch (c) {
    case '{': return {LBRACE, "{", 0, startLine, startCol};
    case '}': return {RBRACE, "}", 0, startLine, startCol};
    case '[': return {LBRACKET, "[", 0, startLine, startCol};
    case ']': return {RBRACKET, "]", 0, startLine, startCol};
    case ':': return {COLON, ":", 0, startLine, startCol};
    case ',': return {COMMA, ",", 0, startLine, startCol};
    default: break;
    }
  }

  char currentChar = current();

  // String literal via regex
  if (currentChar == '"') {
    static const regex strRe("^\"([^\"]*)\"");
    smatch m;
    if (regex_search(remaining, m, strRe)) {
      string value = m[1].str();
      pos += m[0].length();
      col += m[0].length();
      TokenType keywordType = stringToKeyword(value, STRING);
      return {keywordType, value, 0, startLine, startCol};
    }
    return {ERROR_TOKEN, "Unterminated string literal", 0, startLine, startCol};
  }

  // Number regex (kept small; nervous to regex everything)
  if (currentChar == '-' || isdigit(static_cast<unsigned char>(currentChar)) != 0) {
    static const regex numRe("^-?\\d+");
    smatch m;
    string remaining = input.substr(pos);
    if (regex_search(remaining, m, numRe)) {
      string numStr = m.str();
      pos += numStr.length();
      col += numStr.length();
      return {NUMBER, numStr, stoi(numStr), startLine, startCol};
    }
    return {ERROR_TOKEN, "Unexpected character: " + string(1, currentChar), 0, startLine, startCol};
  }

  // Bare keyword
  if (isalpha(static_cast<unsigned char>(currentChar)) != 0 || currentChar == '_') {
    static const regex wordRe("^[A-Za-z_][A-Za-z0-9_]*");
    smatch m;
    if (regex_search(remaining, m, wordRe)) {
      string word = m.str();
      pos += word.length();
      col += word.length();
      TokenType keywordType = stringToKeyword(word, ERROR_TOKEN);
      if (keywordType == ERROR_TOKEN) {
        return {ERROR_TOKEN, "Unexpected keyword: " + word, 0, startLine, startCol};
      }
      return {keywordType, word, 0, startLine, startCol};
    }
  }

  // Unknown character
  string err(1, current());
  advance();
  return {ERROR_TOKEN, "Unexpected character: " + err, 0, startLine, startCol};
}

auto Tokenizer::peekNext() -> Token {
  // Save current state
  size_t oldPos = pos;
  int oldLine = line;
  int oldCol = col;

  // Get the next token
  Token tok = getNext();

  // Restore state
  pos = oldPos;
  line = oldLine;
  col = oldCol;

  return tok;
}

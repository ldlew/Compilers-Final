#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <string>

using namespace std;

void syntaxError(string msg);
void setFilename(string s);

enum TokenType {
  // JSON tokens
  LBRACE,
  RBRACE,
  LBRACKET,
  RBRACKET,
  COLON,
  COMMA,

  // Value types
  STRING,
  NUMBER,

  // Boolean/null literals
  TRUE,
  FALSE,
  NULL_TOKEN,

  // Trigger-event
  ENTERS_BATTLEFIELD,
  DIES,
  ATTACKS,
  DEALS_DAMAGE,
  DEALS_COMBAT_DAMAGE,
  BEGINNING_OF_UPKEEP,
  END_OF_TURN,
  SPELL_CAST,
  BECOMES_TARGET,

  // Trigger-scope
  SELF,
  ANY_CREATURE,
  ANOTHER_CREATURE,
  CREATURE_YOU_CONTROL,
  CREATURE_OPPONENT_CONTROLS,
  ANY_PLAYER,

  // Effect-type
  DEAL_DAMAGE,
  GAIN_LIFE,
  LOSE_LIFE,
  DRAW_CARDS,
  DISCARD,
  DESTROY,
  SACRIFICE,
  EXILE,
  ADD_COUNTERS,
  REMOVE_COUNTERS,
  CHANGE_POWER,
  CHANGE_TOUGHNESS,
  TAP,
  UNTAP,
  CREATE_TOKEN,
  SEARCH_LAND,
  MILL,
  BOUNCE,
  COUNTERSPELL,

  // Target-type
  NONE,
  ANY_TARGET,
  CREATURE,
  PLAYER,
  OPPONENT,
  EACH_OPPONENT,
  CONTROLLER,
  PERMANENT,
  SPELL,

  // Special
  END_OF_FILE,
  ERROR_TOKEN
};

// For debugging
auto tokenTypeToString(TokenType type) -> string;

struct Token {
  // Contains type, value, and location
  TokenType type = ERROR_TOKEN;
  string str;
  int num = 0;
  int line = 1;
  int col = 1;
};

class Tokenizer {
  string input;
  size_t pos = 0;
  int line = 1;
  int col = 1;

  // Move to next character, updating line/col tracking
  void advance();

  // Get current character (or '\0' if at end)
  auto current() const -> char;

  // Skip whitespace characters
  void skipWhitespace();

public:
  explicit Tokenizer(string src) : input(src) { line = 1; col = 1; }

  // Get and consume the next token
  auto getNext() -> Token;

  // Look at the next token without consuming it
  auto peekNext() -> Token;

  // Get current position info (for error messages)
  auto currentLine() const -> int { return line; }
  auto currentCol() const -> int { return col; }
};

#endif

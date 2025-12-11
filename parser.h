/*
  Takes tokens from the tokenizer to build the GameInput struct.
*/

#ifndef PARSER_H
#define PARSER_H

#include "tokenizer.h"
#include "types.h"
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

class Parser {
  Tokenizer tok;

  // Consumes a token and check it's what we expected
  void expect(TokenType expectedToken, const string &context = "");

  // Throw error at the current position
  void error(const string &msg);

  // Called for unknown JSON keys
  void skip(const string &unknownKey);

  // Parse a single field within a triggered ability
  void parseTriggeredAbilityField(TriggeredAbility &ability, bool &explicitTrigger, const string &key);

  // Try to parse effects
  static void applyRulesTextFallback(CardDef &card);

  // Convert token types to enum values
  auto requireTriggerEvent(TokenType token) -> TriggerEvent;
  auto requireTriggerScope(TokenType token) -> TriggerScope;
  auto requireEffectType(TokenType token) -> EffectType;
  auto requireTargetType(TokenType token, const string &context) -> TargetType;

  // Parse a JSON array of strings
  void parseStringArray(vector<string> &outVec);

  // Parse the various object types
  void parseCards(unordered_map<string, CardDef> &cards);
  auto parseCardDef(const string &name) -> CardDef;
  auto parseTriggeredAbility() -> TriggeredAbility;
  auto parseTriggerCondition() -> TriggerCondition;
  auto parseEffect() -> Effect;
  void parseBoards(unordered_map<PlayerID, Board> &boards);
  auto parseBoard() -> Board;
  auto parsePermanent() -> Permanent;
  void parseStack(vector<StackItem> &stack);
  auto parseStackItem() -> StackItem;

public:
  explicit Parser(string json) : tok(json) {}

  // Main entry point
  auto parse() -> GameInput;
};

#endif

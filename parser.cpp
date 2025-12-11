#include "parser.h"
#include "ability_parser.h"
#include <iostream>

using namespace std;

void Parser::error(const string &msg) {
  // Throw a parse error at the current position

  syntaxError(msg);
}

void Parser::expect(TokenType expectedToken, const string &context) {
  // Consume the next token and verify it's what we expected.

  Token got = tok.getNext();

  if (g_debug) {
    cout << "[PARSER] Expecting " << tokenTypeToString(expectedToken)
         << ", consumed " << tokenTypeToString(got.type) << '\n';
  }

  if (got.type != expectedToken) {
    // Build error message

    string expectedStr = tokenTypeToString(expectedToken);
    string gotStr;

    if (got.type == STRING) {
      gotStr = "string \"" + got.str + "\"";
    } else if (got.type == NUMBER) {
      gotStr = "number " + to_string(got.num);
    } else if (got.type == ERROR_TOKEN) {
      gotStr = "error: " + got.str;
    } else {
      gotStr = tokenTypeToString(got.type);
    }

    string msg = "Expected " + expectedStr + " but got " + gotStr;
    if (!context.empty()) {
      msg += " while parsing " + context;
    }
    error(msg);
  }
}

void Parser::skip(const string &unknownKey) {
  // Handle unknown JSON keys.

  error("Unexpected '" + unknownKey + "'");
}

void Parser::parseTriggeredAbilityField(TriggeredAbility &ability, bool &explicitTrigger, const string &key) {
  // Parse a single field of a triggered ability.

  if (key == "trigger") {
    ability.trigger = parseTriggerCondition();
    explicitTrigger = true;
    return;
  }

  if (key == "effects") {
    expect(LBRACKET);
    while (tok.peekNext().type != RBRACKET) {
      ability.effects.push_back(parseEffect());
      if (tok.peekNext().type == COMMA) {
        tok.getNext();
      }
    }
    expect(RBRACKET);
    return;
  }

  if (key == "isMay") {
    Token mayToken = tok.getNext();
    if (mayToken.type != TRUE && mayToken.type != FALSE) {
      error("Expected boolean 'true' or 'false' for isMay");
    }
    ability.isMay = (mayToken.type == TRUE);
    return;
  }

  if (key == "text") {
    ability.text = tok.getNext().str;
    return;
  }

  skip(key);
}

void Parser::applyRulesTextFallback(CardDef &card) {
  // Parse card text when explicit fields are missing

  if (card.rulesText.empty()) {
    return;
  }

  // Try to parse the rules text
  AbilityParseResult parsed = parseAbilityText(card.rulesText);

  // Only use parsed data if explicit fields weren't provided
  if (card.spellEffects.empty() && !parsed.effects.empty()) {
    card.spellEffects = parsed.effects;
    if (card.spellTarget == TargetType::NONE && !card.spellEffects.empty()) {
      card.spellTarget = card.spellEffects.front().target;
    }
  }

  if (card.triggeredAbilities.empty() && parsed.trigger.has_value()) {
    TriggeredAbility ability;
    ability.trigger = *parsed.trigger;
    ability.text = card.rulesText;
    ability.isMay = parsed.isMay;
    ability.effects = parsed.effects;
    card.triggeredAbilities.push_back(ability);
  }
}

auto Parser::requireTriggerEvent(TokenType token) -> TriggerEvent {
  // Convert a token to TriggerEvent

  switch (token) {
  case ENTERS_BATTLEFIELD:
    return TriggerEvent::ENTERS_BATTLEFIELD;
  case DIES:
    return TriggerEvent::DIES;
  case ATTACKS:
    return TriggerEvent::ATTACKS;
  case DEALS_DAMAGE:
    return TriggerEvent::DEALS_DAMAGE;
  case DEALS_COMBAT_DAMAGE:
    return TriggerEvent::DEALS_COMBAT_DAMAGE;
  case BEGINNING_OF_UPKEEP:
    return TriggerEvent::BEGINNING_OF_UPKEEP;
  case END_OF_TURN:
    return TriggerEvent::END_OF_TURN;
  case SPELL_CAST:
    return TriggerEvent::SPELL_CAST;
  case BECOMES_TARGET:
    return TriggerEvent::BECOMES_TARGET;
  default:
    error("Expected trigger event");
    return TriggerEvent::ENTERS_BATTLEFIELD;
  }
}

auto Parser::requireTriggerScope(TokenType token) -> TriggerScope {
  // Convert a token to TriggerScope

  switch (token) {
  case SELF:
    return TriggerScope::SELF;
  case ANY_CREATURE:
    return TriggerScope::ANY_CREATURE;
  case ANOTHER_CREATURE:
    return TriggerScope::ANOTHER_CREATURE;
  case CREATURE_YOU_CONTROL:
    return TriggerScope::CREATURE_YOU_CONTROL;
  case CREATURE_OPPONENT_CONTROLS:
    return TriggerScope::CREATURE_OPPONENT_CONTROLS;
  case ANY_PLAYER:
    return TriggerScope::ANY_PLAYER;
  default:
    error("Expected a trigger scope");
    return TriggerScope::SELF;  // Never reached
  }
}

auto Parser::requireEffectType(TokenType token) -> EffectType {
  // Convert a token to EffectType

  switch (token) {
  case DEAL_DAMAGE:
    return EffectType::DEAL_DAMAGE;
  case GAIN_LIFE:
    return EffectType::GAIN_LIFE;
  case LOSE_LIFE:
    return EffectType::LOSE_LIFE;
  case DRAW_CARDS:
    return EffectType::DRAW_CARDS;
  case COUNTERSPELL:
    return EffectType::COUNTERSPELL;
  case DISCARD:
    return EffectType::DISCARD;
  case DESTROY:
    return EffectType::DESTROY;
  case SACRIFICE:
    return EffectType::SACRIFICE;
  case EXILE:
    return EffectType::EXILE;
  case ADD_COUNTERS:
    return EffectType::ADD_COUNTERS;
  case REMOVE_COUNTERS:
    return EffectType::REMOVE_COUNTERS;
  case CHANGE_POWER:
    return EffectType::CHANGE_POWER;
  case CHANGE_TOUGHNESS:
    return EffectType::CHANGE_TOUGHNESS;
  case TAP:
    return EffectType::TAP;
  case UNTAP:
    return EffectType::UNTAP;
  case CREATE_TOKEN:
    return EffectType::CREATE_TOKEN;
  case SEARCH_LAND:
    return EffectType::SEARCH_LAND;
  case MILL:
    return EffectType::MILL;
  case BOUNCE:
    return EffectType::BOUNCE;
  default:
    error("Expected an effect type");
    return EffectType::DEAL_DAMAGE;
  }
}

auto Parser::requireTargetType(TokenType token, const string &context) -> TargetType {\
  // Convert a token to TargetType

  switch (token) {
  case NONE:
    return TargetType::NONE;
  case ANY_TARGET:
    return TargetType::ANY_TARGET;
  case CREATURE:
    return TargetType::CREATURE;
  case PLAYER:
    return TargetType::PLAYER;
  case OPPONENT:
    return TargetType::OPPONENT;
  case EACH_OPPONENT:
    return TargetType::EACH_OPPONENT;
  case CONTROLLER:
    return TargetType::CONTROLLER;
  case PERMANENT:
    return TargetType::PERMANENT;
  case SPELL:
    return TargetType::SPELL;
  default:
    string msg = "Expected a target type";

    if (!context.empty()) {
      msg += " for " + context;
    }

    error(msg);
    return TargetType::NONE;
  }
}

void Parser::parseStringArray(vector<string> &outVec) {
  // Parse a JSON array

  expect(LBRACKET);

  while (tok.peekNext().type != RBRACKET) {
    outVec.push_back(tok.getNext().str);
    if (tok.peekNext().type == COMMA) {
      tok.getNext();
    }
  }

  expect(RBRACKET);
}

auto Parser::parseTriggerCondition() -> TriggerCondition {
  // Parse { "event": X, "scope": Y }

  TriggerCondition cond;
  bool haveEvent = false;
  bool haveScope = false;

  expect(LBRACE, "trigger condition");

  while (tok.peekNext().type != RBRACE) {
    string key = tok.getNext().str;
    expect(COLON);

    if (key == "event") {
      cond.event = requireTriggerEvent(tok.getNext().type);
      haveEvent = true;
    } else if (key == "scope") {
      cond.scope = requireTriggerScope(tok.getNext().type);
      haveScope = true;
    } else {
      skip(key);
    }

    if (tok.peekNext().type == COMMA) {
      tok.getNext();
    }
  }
  expect(RBRACE);

  // Both event and scope are required
  if (!haveEvent || !haveScope) {
    error("Trigger condition missing a requirement");
  }
  return cond;
}

auto Parser::parseEffect() -> Effect {
  // Parse { "type": X, "value": Y, "target": Z }.

  Effect eff;
  bool hasType = false;

  expect(LBRACE, "effect");

  while (tok.peekNext().type != RBRACE) {
    string key = tok.getNext().str;
    expect(COLON);

    if (key == "type") {
      eff.type = requireEffectType(tok.getNext().type);
      hasType = true;
    } else if (key == "value") {
      eff.value = tok.getNext().num;
    } else if (key == "target") {
      eff.target = requireTargetType(tok.getNext().type, "effect target");
    } else {
      skip(key);
    }

    if (tok.peekNext().type == COMMA) {
      tok.getNext();
    }
  }
  expect(RBRACE);

  if (!hasType) {
    error("Effect missing required field");
  }
  return eff;
}

auto Parser::parseTriggeredAbility() -> TriggeredAbility {
  // Parse a triggered ability

  TriggeredAbility ability;
  expect(LBRACE, "triggered ability");

  bool explicitTrigger = false;

  while (tok.peekNext().type != RBRACE) {
    string key = tok.getNext().str;
    expect(COLON);
    parseTriggeredAbilityField(ability, explicitTrigger, key);
    if (tok.peekNext().type == COMMA) {
      tok.getNext();
    }
  }
  expect(RBRACE);

  // If we only got text, try to parse it for effects
  if (!ability.text.empty()) {
    AbilityParseResult parsed = parseAbilityText(ability.text);
    if (ability.effects.empty()) {
      ability.effects = parsed.effects;
    }
    if (!explicitTrigger && parsed.trigger.has_value()) {
      ability.trigger = *parsed.trigger;
    }
    ability.isMay = ability.isMay || parsed.isMay;
  }
  return ability;
}

auto Parser::parseCardDef(const string &name) -> CardDef {
  // Parse a card definition

  if (g_debug) {
    cout << "[PARSER] Parsing card for \"" << name << "\"" << '\n';
  }

  CardDef card;
  card.name = name;

  expect(LBRACE, "card for " + name);

  while (tok.peekNext().type != RBRACE) {
    string key = tok.getNext().str;
    expect(COLON);

    if (key == "text") {
      card.rulesText = tok.getNext().str;
    } else if (key == "types") {
      parseStringArray(card.types);
    } else if (key == "subtypes") {
      parseStringArray(card.subtypes);
    } else if (key == "keywords") {
      parseStringArray(card.keywords);
    } else if (key == "power") {
      card.power = tok.getNext().num;
    } else if (key == "toughness") {
      card.toughness = tok.getNext().num;
    } else if (key == "spellTarget") {
      card.spellTarget = requireTargetType(tok.getNext().type, "spellTarget");
    } else if (key == "spellEffects") {
      // Parse array of effects
      expect(LBRACKET);
      while (tok.peekNext().type != RBRACKET) {
        card.spellEffects.push_back(parseEffect());
        if (tok.peekNext().type == COMMA) {
          tok.getNext();
        }
      }
      expect(RBRACKET);
    } else if (key == "triggeredAbilities") {
      // Parse array of triggered abilities
      expect(LBRACKET);
      while (tok.peekNext().type != RBRACKET) {
        card.triggeredAbilities.push_back(parseTriggeredAbility());
        if (tok.peekNext().type == COMMA) {
          tok.getNext();
        }
      }
      expect(RBRACKET);
    } else {
      skip(key);
    }

    if (tok.peekNext().type == COMMA) {
      tok.getNext();
    }
  }
  expect(RBRACE);

  // Try to fill in missing data from rules text
  applyRulesTextFallback(card);
  return card;
}

// Parse the"cards object (card name -> definition map)
void Parser::parseCards(unordered_map<string, CardDef> &cards) {
  if (g_debug) {
    cout << "[PARSER] Parsing 'cards' object" << '\n';
  }

  expect(LBRACE, "cards object");

  while (tok.peekNext().type != RBRACE) {
    Token nameToken = tok.getNext();
    if (nameToken.type != STRING) {
      error("Expected card name");
    }
    expect(COLON);
    cards[nameToken.str] = parseCardDef(nameToken.str);
    if (tok.peekNext().type == COMMA) {
      tok.getNext();
    }
  }
  expect(RBRACE);
}

auto Parser::parsePermanent() -> Permanent {
  // Parse a permanent on the battlefield

  Permanent perm;
  expect(LBRACE, "permanent");

  while (tok.peekNext().type != RBRACE) {
    string key = tok.getNext().str;
    expect(COLON);

    if (key == "id") {
      perm.id = tok.getNext().str;
    } else if (key == "name") {
      perm.cardName = tok.getNext().str;
    } else if (key == "controller") {
      perm.controller = tok.getNext().str;
    } else if (key == "tapped") {
      Token tappedToken = tok.getNext();
      if (tappedToken.type != TRUE && tappedToken.type != FALSE) {
        error("Expected boolean for tapped status");
      }
      perm.tapped = (tappedToken.type == TRUE);
    } else {
      skip(key);
    }

    if (tok.peekNext().type == COMMA) {
      tok.getNext();
    }
  }
  expect(RBRACE);
  return perm;
}

auto Parser::parseBoard() -> Board {
  // Parse one player's board state

  Board board;
  expect(LBRACE, "board");

  while (tok.peekNext().type != RBRACE) {
    string key = tok.getNext().str;
    expect(COLON);

    if (key == "life") {
      board.life = tok.getNext().num;
    } else if (key == "player") {
      board.player = tok.getNext().str;
    } else if (key == "permanents") {
      expect(LBRACKET);
      while (tok.peekNext().type != RBRACKET) {
        board.permanents.push_back(parsePermanent());
        if (tok.peekNext().type == COMMA) {
          tok.getNext();
        }
      }
      expect(RBRACKET);
    } else {
      skip(key);
    }

    if (tok.peekNext().type == COMMA) {
      tok.getNext();
    }
  }
  expect(RBRACE);
  return board;
}

void Parser::parseBoards(unordered_map<PlayerID, Board> &boards) {
  // Parse the "boards" object

  if (g_debug) {
    cout << "[PARSER] Parsing 'boards' object" << '\n';
  }

  expect(LBRACE, "boards");

  while (tok.peekNext().type != RBRACE) {
    string playerId = tok.getNext().str;
    expect(COLON);
    Board board = parseBoard();
    board.player = playerId;
    boards[playerId] = board;
    if (tok.peekNext().type == COMMA) {
      tok.getNext();
    }
  }
  expect(RBRACE);
}

auto Parser::parseStackItem() -> StackItem {
  // Parse one item on the stack (spell or ability)

  StackItem item;
  expect(LBRACE, "stack item");

  while (tok.peekNext().type != RBRACE) {
    string key = tok.getNext().str;
    expect(COLON);

    if (key == "id") {
      item.id = tok.getNext().str;
    } else if (key == "kind") {
      item.kind = tok.getNext().str;
    } else if (key == "sourceName") {
      item.sourceName = tok.getNext().str;
    } else if (key == "sourceId") {
      item.sourceId = tok.getNext().str;
    } else if (key == "abilityIndex") {
      item.abilityIndex = tok.getNext().num;
    } else if (key == "controller") {
      item.controller = tok.getNext().str;
    } else if (key == "targetId") {
      item.targetId = tok.getNext().str;
    } else if (key == "targetStackId") {
      item.targetStackId = tok.getNext().str;
    } else if (key == "targetPlayer") {
      item.targetPlayer = tok.getNext().str;
    } else {
      skip(key);
    }

    if (tok.peekNext().type == COMMA) {
      tok.getNext();
    }
  }
  expect(RBRACE);
  return item;
}

void Parser::parseStack(vector<StackItem> &stack) {
  // Parse the "stack" array

  if (g_debug) {
    cout << "[PARSER] Parsing 'stack' array" << '\n';
  }

  expect(LBRACKET, "stack");

  while (tok.peekNext().type != RBRACKET) {
    stack.push_back(parseStackItem());
    if (tok.peekNext().type == COMMA) {
      tok.getNext();
    }
  }
  expect(RBRACKET);
}

auto Parser::parse() -> GameInput {
  // Main entry point: parse the entire JSON input

  if (g_debug) {
    cout << "[PARSER] Starting..." << '\n';
  }

  GameInput input;

  expect(LBRACE, "root object");

  while (tok.peekNext().type != RBRACE && tok.peekNext().type != END_OF_FILE) {
    Token keyToken = tok.getNext();
    if (keyToken.type != STRING) {
      error("Expected key string");
    }
    string key = keyToken.str;
    expect(COLON);

    if (key == "cards") {
      parseCards(input.cards);
    } else if (key == "activePlayer") {
      input.activePlayer = tok.getNext().str;
    } else if (key == "priorityPlayer") {
      input.priorityPlayer = tok.getNext().str;
    } else if (key == "currentPhase") {
      input.currentPhase = tok.getNext().str;
    } else if (key == "turnNumber") {
      tok.getNext();
    } else if (key == "boards") {
      parseBoards(input.boards);
    } else if (key == "stack") {
      parseStack(input.stack);
    } else {
      skip(key);
    }

    if (tok.peekNext().type == COMMA) {
      tok.getNext();
    }
  }
  expect(RBRACE);

  // Default priority to active player if not specified
  if (input.priorityPlayer.empty()) {
    input.priorityPlayer = input.activePlayer;
  }

  return input;
}

#include "ability_parser.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <vector>

using namespace std;

namespace {

enum class AbilityTokenType { WORD, NUMBER, COMMA, END };

struct AbilityToken {
  AbilityTokenType type = AbilityTokenType::END;
  string text;
  int value = 0;
};

class AbilityTokenizer {
  string input;
  size_t pos = 0;
  optional<AbilityToken> cached;

  static auto toUpper(string inputStr) -> string {
    transform(inputStr.begin(), inputStr.end(), inputStr.begin(),
              [](unsigned char inChar) -> char {
                return static_cast<char>(toupper(inChar));
              });
    return inputStr;
  }

  void skipWhitespace() {
    while (pos < input.size() &&
           isspace(static_cast<unsigned char>(input[pos])) != 0) {
      pos++;
    }
  }

  auto current() const -> char {
    return (pos < input.size()) ? input[pos] : '\0';
  }

  auto readWhile(const function<bool(char)> &predicate) -> string {
    string out;
    while (pos < input.size() && predicate(input[pos])) {
      out.push_back(input[pos]);
      pos++;
    }
    return out;
  }

  auto readNumber() -> AbilityToken {
    string numStr = readWhile([](char digitChar) -> bool {
      return isdigit(static_cast<unsigned char>(digitChar)) != 0;
    });
    return {AbilityTokenType::NUMBER, numStr, stoi(numStr)};
  }

  auto readWord() -> AbilityToken {
    string word = readWhile([](char letter) -> bool {
      if (isspace(static_cast<unsigned char>(letter)) != 0) {
        return false;
      }
      if (letter == ',' || letter == ';' || letter == '.' || letter == '(' ||
          letter == ')') {
        return false;
      }
      return true;
    });
    return {AbilityTokenType::WORD, toUpper(word), 0};
  }

public:
  explicit AbilityTokenizer(string src) : input(src) {}

  auto next() -> AbilityToken {
    if (cached.has_value()) {
      AbilityToken tok = *cached;
      cached.reset();
      return tok;
    }

    skipWhitespace();
    char currentChar = current();
    if (currentChar == '\0') {
      return {AbilityTokenType::END, "", 0};
    }

    if (currentChar == ',' || currentChar == ';') {
      pos++;
      return {AbilityTokenType::COMMA, string(1, currentChar), 0};
    }
    if (currentChar == '.' || currentChar == '(' || currentChar == ')') {
      pos++;
      return next();
    }
    if (isdigit(static_cast<unsigned char>(currentChar)) != 0) {
      return readNumber();
    }
    return readWord();
  }

  auto peek() -> AbilityToken {
    if (!cached.has_value()) {
      cached = next();
    }
    return *cached;
  }
};

// ----------------------------- Helpers ----------------------------- //

auto hasWord(const vector<AbilityToken> &tokens, const string &word) -> bool {
  return any_of(tokens.begin(), tokens.end(),
                [&](const AbilityToken &tok) -> bool {
                  return tok.type == AbilityTokenType::WORD && tok.text == word;
                });
}

auto hasWord(const vector<string> &words, const string &word) -> bool {
  return any_of(
      words.begin(), words.end(),
      [&](const string &candidate) -> bool { return candidate == word; });
}

auto firstNumber(const vector<AbilityToken> &tokens, int defaultVal = 1)
    -> int {
  for (const auto &tok : tokens) {
    if (tok.type == AbilityTokenType::NUMBER) {
      return tok.value;
    }
    if (tok.type == AbilityTokenType::WORD) {
      if (all_of(tok.text.begin(), tok.text.end(), [](char digit) -> bool {
            return isdigit(static_cast<unsigned char>(digit)) != 0;
          })) {
        return stoi(tok.text);
      }
    }
  }
  return defaultVal;
}

auto parseBuffWord(const string &word) -> optional<pair<int, int>> {
  static const regex buffRe(R"(^\+?(-?\d+)\s*/\s*\+?(-?\d+)$)");
  smatch match;
  if (regex_match(word, match, buffRe)) {
    int powerDelta = stoi(match[1]);
    int toughnessDelta = stoi(match[2]);
    return make_pair(powerDelta, toughnessDelta);
  }
  return nullopt;
}

auto detectTarget(const vector<string> &words, TargetType fallback)
    -> TargetType {
  if (hasWord(words, "EACH") && hasWord(words, "OPPONENT")) {
    return TargetType::EACH_OPPONENT;
  }
  if (hasWord(words, "ANY") && hasWord(words, "TARGET")) {
    return TargetType::ANY_TARGET;
  }

  if (hasWord(words, "TARGET")) {
    if (hasWord(words, "CREATURE")) {
      return TargetType::CREATURE;
    }
    if (hasWord(words, "PLAYER")) {
      return TargetType::PLAYER;
    }
    if (hasWord(words, "OPPONENT")) {
      return TargetType::OPPONENT;
    }
    if (hasWord(words, "PERMANENT")) {
      return TargetType::PERMANENT;
    }
    if (hasWord(words, "SPELL")) {
      return TargetType::SPELL;
    }
  }

  if (hasWord(words, "OPPONENT")) {
    return TargetType::OPPONENT;
  }
  if (hasWord(words, "PLAYER")) {
    return TargetType::PLAYER;
  }
  if (hasWord(words, "CREATURE")) {
    return TargetType::CREATURE;
  }
  if (hasWord(words, "SPELL")) {
    return TargetType::SPELL;
  }

  return fallback;
}

auto parseTriggerScope(const vector<AbilityToken> &subjectTokens)
    -> TriggerScope {
  if (hasWord(subjectTokens, "ANOTHER") && hasWord(subjectTokens, "CREATURE")) {
    return TriggerScope::ANOTHER_CREATURE;
  }
  if (hasWord(subjectTokens, "CREATURE") && hasWord(subjectTokens, "YOU") &&
      (hasWord(subjectTokens, "CONTROL") ||
       hasWord(subjectTokens, "CONTROLS"))) {
    return TriggerScope::CREATURE_YOU_CONTROL;
  }
  if (hasWord(subjectTokens, "CREATURE") &&
      hasWord(subjectTokens, "OPPONENT")) {
    return TriggerScope::CREATURE_OPPONENT_CONTROLS;
  }
  if (hasWord(subjectTokens, "CREATURE")) {
    return TriggerScope::ANY_CREATURE;
  }
  if (hasWord(subjectTokens, "PLAYER") || hasWord(subjectTokens, "OPPONENT")) {
    return TriggerScope::ANY_PLAYER;
  }
  return TriggerScope::SELF;
}

auto eventFromToken(const AbilityToken &token, AbilityTokenizer &tok)
    -> optional<TriggerEvent> {
  if (token.type != AbilityTokenType::WORD) {
    return nullopt;
  }

  const string &word = token.text;
  if (word == "ENTERS" || word == "ENTER" || word == "ETB" || word == "ETBS") {
    if (tok.peek().type == AbilityTokenType::WORD &&
        tok.peek().text == "BATTLEFIELD") {
      tok.next();
    }
    return TriggerEvent::ENTERS_BATTLEFIELD;
  }
  if (word == "DIES" || word == "DIE") {
    return TriggerEvent::DIES;
  }
  if (word == "ATTACKS" || word == "ATTACK") {
    return TriggerEvent::ATTACKS;
  }
  if (word == "BLOCKS" || word == "BLOCK") {
    return TriggerEvent::BLOCKS;
  }
  if (word == "CASTS" || word == "CAST") {
    return TriggerEvent::SPELL_CAST;
  }

  if (word == "DEALS") {
    if (tok.peek().type == AbilityTokenType::WORD &&
        tok.peek().text == "COMBAT") {
      tok.next();
      if (tok.peek().type == AbilityTokenType::WORD &&
          tok.peek().text == "DAMAGE") {
        tok.next();
      }
      return TriggerEvent::DEALS_COMBAT_DAMAGE;
    }
    if (tok.peek().type == AbilityTokenType::WORD &&
        tok.peek().text == "DAMAGE") {
      tok.next();
    }
    return TriggerEvent::DEALS_DAMAGE;
  }

  if (word == "BECOMES") {
    if (tok.peek().type == AbilityTokenType::WORD && tok.peek().text == "THE") {
      tok.next();
    }
    if (tok.peek().type == AbilityTokenType::WORD &&
        tok.peek().text == "TARGET") {
      tok.next();
      return TriggerEvent::BECOMES_TARGET;
    }
  }

  if (word == "BEGINNING") {
    if (tok.peek().type == AbilityTokenType::WORD && tok.peek().text == "OF") {
      tok.next();
    }
    if (tok.peek().type == AbilityTokenType::WORD &&
        tok.peek().text == "UPKEEP") {
      tok.next();
      return TriggerEvent::BEGINNING_OF_UPKEEP;
    }
  }

  if (word == "END") {
    if (tok.peek().type == AbilityTokenType::WORD && tok.peek().text == "OF") {
      tok.next();
    }
    if (tok.peek().type == AbilityTokenType::WORD &&
        (tok.peek().text == "TURN" || tok.peek().text == "STEP")) {
      tok.next();
      return TriggerEvent::END_OF_TURN;
    }
  }

  return nullopt;
}

auto parseTriggerClause(AbilityTokenizer &tok) -> optional<TriggerCondition> {
  tok.next();
  vector<AbilityToken> subject;
  optional<TriggerEvent> event;
  while (tok.peek().type != AbilityTokenType::END) {
    AbilityToken nextTok = tok.next();
    if (nextTok.type == AbilityTokenType::COMMA) {
      break;
    }
    auto candidate = eventFromToken(nextTok, tok);
    if (candidate.has_value()) {
      event = candidate;
      break;
    }
    subject.push_back(nextTok);
  }
  if (!event.has_value()) {
    return nullopt;
  }
  TriggerCondition cond;
  cond.event = *event;
  cond.scope = parseTriggerScope(subject);
  return cond;
}

auto extractWords(const vector<AbilityToken> &phrase) -> vector<string> {
  vector<string> words;
  for (const auto &tok : phrase) {
    if (tok.type == AbilityTokenType::WORD) {
      words.push_back(tok.text);
    }
  }
  return words;
}

auto maybeDestroyEffect(const vector<string> &words) -> optional<Effect> {
  if (!hasWord(words, "DESTROY")) {
    return nullopt;
  }
  Effect eff;
  eff.type = EffectType::DESTROY;
  eff.target = detectTarget(words, TargetType::CREATURE);
  return eff;
}

auto maybeCounterEffect(const vector<string> &words) -> optional<Effect> {
  if (!hasWord(words, "COUNTER")) {
    return nullopt;
  }
  Effect eff;
  eff.type = EffectType::COUNTERSPELL;
  eff.target = TargetType::SPELL;
  return eff;
}

auto maybeBounceEffect(const vector<string> &words) -> optional<Effect> {
  if (!hasWord(words, "RETURN")) {
    return nullopt;
  }
  Effect eff;
  eff.type = EffectType::BOUNCE;
  eff.target = detectTarget(words, TargetType::PERMANENT);
  return eff;
}

auto maybeDamageEffect(const vector<string> &words, int num)
    -> optional<Effect> {
  if (!hasWord(words, "DEAL") && !hasWord(words, "DEALS")) {
    return nullopt;
  }
  Effect eff;
  eff.type = EffectType::DEAL_DAMAGE;
  eff.value = num;
  eff.target = detectTarget(words, TargetType::ANY_TARGET);
  return eff;
}

auto maybeDrawEffect(const vector<string> &words, int num)
    -> optional<Effect> {
  if (!hasWord(words, "DRAW")) {
    return nullopt;
  }
  Effect eff;
  eff.type = EffectType::DRAW_CARDS;
  eff.value = num;
  eff.target = TargetType::NONE;
  return eff;
}

auto maybeGainLifeEffect(const vector<string> &words, int num)
    -> optional<Effect> {
  if (!hasWord(words, "GAIN") || !hasWord(words, "LIFE")) {
    return nullopt;
  }
  Effect eff;
  eff.type = EffectType::GAIN_LIFE;
  eff.value = num;
  eff.target = TargetType::NONE;
  return eff;
}

auto maybeLoseLifeEffect(const vector<string> &words, int num)
    -> optional<Effect> {
  if ((!hasWord(words, "LOSE") && !hasWord(words, "LOSES")) ||
      !hasWord(words, "LIFE")) {
    return nullopt;
  }
  Effect eff;
  eff.type = EffectType::LOSE_LIFE;
  eff.value = num;
  eff.target = detectTarget(words, TargetType::OPPONENT);
  return eff;
}

auto maybeBuffEffects(const vector<string> &words)
    -> optional<vector<Effect>> {
  for (const auto &word : words) {
    auto buff = parseBuffWord(word);
    if (!buff.has_value()) {
      continue;
    }

    int powerDelta = buff->first;
    int toughnessDelta = buff->second;

    if (powerDelta == toughnessDelta) {
      Effect eff;
      if (powerDelta >= 0) {
        eff.type = EffectType::ADD_COUNTERS;
        eff.value = powerDelta;
      } else {
        eff.type = EffectType::REMOVE_COUNTERS;
        eff.value = -powerDelta;
      }
      eff.target = TargetType::CREATURE;
      return vector<Effect>{eff};
    }

    Effect powerEff;
    powerEff.type = EffectType::CHANGE_POWER;
    powerEff.value = powerDelta;
    powerEff.target = TargetType::CREATURE;

    Effect toughEff;
    toughEff.type = EffectType::CHANGE_TOUGHNESS;
    toughEff.value = toughnessDelta;
    toughEff.target = TargetType::CREATURE;

    vector<Effect> out;
    if (powerEff.value != 0) {
      out.push_back(powerEff);
    }
    if (toughEff.value != 0) {
      out.push_back(toughEff);
    }
    return out;
  }
  return nullopt;
}

auto maybeSearchLandEffect(const vector<string> &words)
    -> optional<Effect> {
  if (!hasWord(words, "SEARCH") || !hasWord(words, "LAND")) {
    return nullopt;
  }
  Effect eff;
  eff.type = EffectType::SEARCH_LAND;
  eff.target = TargetType::NONE;
  return eff;
}

auto parseEffectPhrase(const vector<AbilityToken> &phrase) -> vector<Effect> {
  vector<string> words = extractWords(phrase);
  if (words.empty()) {
    return {};
  }

  int num = firstNumber(phrase, 1);

  if (auto eff = maybeDestroyEffect(words)) {
    return {*eff};
  }
  if (auto eff = maybeCounterEffect(words)) {
    return {*eff};
  }
  if (auto eff = maybeBounceEffect(words)) {
    return {*eff};
  }
  if (auto eff = maybeDamageEffect(words, num)) {
    return {*eff};
  }
  if (auto eff = maybeDrawEffect(words, num)) {
    return {*eff};
  }
  if (auto eff = maybeGainLifeEffect(words, num)) {
    return {*eff};
  }
  if (auto eff = maybeLoseLifeEffect(words, num)) {
    return {*eff};
  }
  if (auto buffEffects = maybeBuffEffects(words)) {
    return *buffEffects;
  }
  if (auto eff = maybeSearchLandEffect(words)) {
    return {*eff};
  }

  return {};
}

auto collectEffectPhrase(AbilityTokenizer &tok) -> vector<AbilityToken> {
  vector<AbilityToken> phrase;
  AbilityToken start = tok.peek();
  while (start.type == AbilityTokenType::COMMA) {
    tok.next();
    start = tok.peek();
  }
  if (start.type == AbilityTokenType::END) {
    return phrase;
  }

  phrase.push_back(tok.next());
  while (tok.peek().type != AbilityTokenType::END) {
    if (tok.peek().type == AbilityTokenType::COMMA) {
      tok.next();
      break;
    }
    if (tok.peek().type == AbilityTokenType::WORD && tok.peek().text == "AND") {
      tok.next();
      break;
    }
    phrase.push_back(tok.next());
  }
  return phrase;
}

}

// ----------------------------- Entry Point ----------------------------- //

auto parseAbilityText(const string &text) -> AbilityParseResult {
  AbilityParseResult result;

  string lowerText = text;
  transform(lowerText.begin(), lowerText.end(), lowerText.begin(),
            [](unsigned char lowerChar) -> char {
              return static_cast<char>(tolower(lowerChar));
            });
  if (lowerText.find("may") != string::npos) {
    result.isMay = true;
  }

  AbilityTokenizer tok(text);
  AbilityToken first = tok.peek();
  if (first.type == AbilityTokenType::WORD &&
      (first.text == "WHEN" || first.text == "WHENEVER" ||
       first.text == "AT")) {
    result.trigger = parseTriggerClause(tok);
    if (tok.peek().type == AbilityTokenType::COMMA) {
      tok.next();
    }
  }

  while (tok.peek().type != AbilityTokenType::END) {
    vector<AbilityToken> phrase = collectEffectPhrase(tok);
    if (phrase.empty()) {
      break;
    }
    auto effs = parseEffectPhrase(phrase);
    result.effects.insert(result.effects.end(), effs.begin(), effs.end());
  }

  return result;
}

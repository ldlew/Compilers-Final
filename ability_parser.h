#ifndef ABILITY_PARSER_H
#define ABILITY_PARSER_H

#include "types.h"

#include <optional>
#include <string>
#include <vector>

using namespace std;

struct AbilityParseResult {
  optional<TriggerCondition> trigger;
  vector<Effect> effects;
  bool isMay = false;
};

auto parseAbilityText(const string &text) -> AbilityParseResult;

#endif

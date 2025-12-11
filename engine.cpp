#include "engine.h"
#include <algorithm>
#include <iostream>

using namespace std;

Engine::Engine(GameInput input) : state(input) {
  // Record starting life totals
  for (const auto &[playerId, board] : state.boards) {
    output.finalLife[playerId] = board.life;
  }
}

// ----------------------------- Helpers ----------------------------- //

auto Engine::getCardDef(const string &name) const -> const CardDef * {
  // Look up a card definition by name

  auto it = state.cards.find(name);
  if (it != state.cards.end()) {
    return &it->second;
  }
  return nullptr;
}

auto Engine::findPermanent(const ObjectID &objectId) -> Permanent * {
  // Find a permanent on any player's battlefield

  for (auto &[playerId, board] : state.boards) {
    for (auto &perm : board.permanents) {
      if (perm.id == objectId) {
        return &perm;
      }
    }
  }
  return nullptr;
}

auto Engine::getTurnOrder(const PlayerID &player) const -> int {
  // Active player gets 0, everyone else 1 (for APNAP)

  if (player == state.activePlayer) {
    return 0;
  }
  return 1;
}

auto Engine::hasKeyword(const string &cardName, const string &keyword) const -> bool {
  // Check if a card has a specific keyword ability

  const CardDef *card = getCardDef(cardName);
  if (card == nullptr) {
    return false;
  }

  // Search through the card's keywords
  for (const string &kw : card->keywords) {
    if (kw == keyword) {
      return true;
    }
  }
  return false;
}

// ----------------------------- Priority + Validation ----------------------------- //

auto Engine::validatePriority(const StackItem &item) -> bool {
  // Ensure the item's controller currently has priority.

  if (item.controller != state.priorityPlayer) {
    output.errors.push_back("PRIORITY ERROR: " + item.controller + " cannot cast " + item.sourceName + " - priority belongs to " + state.priorityPlayer);
    return false;
  }
  return true;
}

// ----------------------------- Trigger System ----------------------------- //
auto Engine::triggerMatches(const TriggerCondition &trig, const GameEvent &event, const Permanent &source) -> bool {
  // Decide whether a trigger should fire for an event.

  if (trig.event != event.type) {                     // Event type must match
    return false;
  }

  switch (trig.scope) {
  case TriggerScope::SELF:                            // Only triggers for THIS permanent
    return source.id == event.objectId;

  case TriggerScope::ANY_CREATURE:                    // Triggers for any creature
    return true;

  case TriggerScope::ANOTHER_CREATURE:                // Triggers for any creature EXCEPT this one
    return source.id != event.objectId;

  case TriggerScope::CREATURE_YOU_CONTROL:            // Triggers for creatures controlled by the same player
    return event.controller == source.controller;

  case TriggerScope::CREATURE_OPPONENT_CONTROLS:      // Triggers for creatures controlled by opponents
    return event.controller != source.controller;

  case TriggerScope::ANY_PLAYER:                      // Always triggers
    return true;
  }
  return false;
}

auto Engine::findTriggersForEvent(const GameEvent &event) -> vector<PendingTrigger> {
  // Find all triggers that fire from an event across all boards.

  if (g_debug) {
    cout << "[ENGINE] Checking triggers for event type "
         << static_cast<int>(event.type) << " on " << event.objectId << '\n';
  }

  vector<PendingTrigger> triggers;

  // Check every permanent on every battlefield
  for (const auto &[pid, board] : state.boards) {
    for (const auto &perm : board.permanents) {
      const CardDef *card = getCardDef(perm.cardName);
      if (card == nullptr) {
        continue;
      }

      // Check each triggered ability on this card
      for (size_t i = 0; i < card->triggeredAbilities.size(); i++) {
        const auto &ability = card->triggeredAbilities[i];

        if (triggerMatches(ability.trigger, event, perm)) {
          // This ability triggers + creates a pending one
          PendingTrigger pt;
          pt.sourceId = perm.id;
          pt.sourceName = perm.cardName;
          pt.abilityIndex = static_cast<int>(i);
          pt.controller = perm.controller;
          pt.text = ability.text;
          pt.isActivePlayer = (perm.controller == state.activePlayer);
          pt.turnOrder = getTurnOrder(perm.controller);
          triggers.push_back(pt);
        }
      }
    }
  }
  return triggers;
}

auto Engine::orderAPNAP(vector<PendingTrigger> triggers) -> vector<PendingTrigger> {
  // Sort triggers in APNAP order; stable_sort preserves relative order for same-player triggers
  stable_sort(triggers.begin(), triggers.end(),
              [](const PendingTrigger &a, const PendingTrigger &b) {
                return a.turnOrder < b.turnOrder;
              });
  return triggers;
}

void Engine::addTriggersToStack(const vector<PendingTrigger> &triggers) {
  // Put triggered abilities onto the stack.

  if (g_debug) {
    cout << "[ENGINE] Adding " << triggers.size() << " triggers to stack.\n";
  }

  for (const auto &trig : triggers) {
    StackItem item;
    item.id = "trig_" + to_string(++triggerCount);
    item.kind = "TRIGGERED_ABILITY";
    item.sourceName = trig.sourceName;
    item.sourceId = trig.sourceId;
    item.abilityIndex = trig.abilityIndex;
    item.controller = trig.controller;
    state.stack.push_back(item);
  }
}

// ----------------------------- Destruction Handling ----------------------------- //
void Engine::destroyPermanent(const ObjectID &objectId, vector<GameEvent> &events) {
  // Remove a permanent from the battlefield and emit a DIES event.

  for (auto &[playerId, board] : state.boards) {
    for (auto it = board.permanents.begin(); it != board.permanents.end(); ++it) {
      if (it->id == objectId) {
        // Check for indestructible
        if (hasKeyword(it->cardName, "INDESTRUCTIBLE")) {
          return;
        }

        // Create a DIES event for death triggers
        GameEvent dieEvent;
        dieEvent.type = TriggerEvent::DIES;
        dieEvent.objectId = it->id;
        dieEvent.cardName = it->cardName;
        dieEvent.controller = it->controller;
        events.push_back(dieEvent);

        // Record the destruction and remove from battlefield
        output.destroyedPermanents.push_back(objectId);
        board.permanents.erase(it);
        return;
      }
    }
  }
}

// ----------------------------- Effect Handling ----------------------------- //

// Handle DEAL_DAMAGE
void Engine::resolveDealDamageEffect(const StackItem &item, const Effect &effect, ResolutionStep &step) {
  // Damage to a player
  if (!item.targetPlayer.empty()) {
    state.boards[item.targetPlayer].life -= effect.value;
    step.description += item.sourceName + " deals " + to_string(effect.value) + " damage to " + item.targetPlayer + ". ";

    return;
  }

  // Damage to a creature
  if (!item.targetId.empty()) {
    Permanent *target = findPermanent(item.targetId);
    if (target == nullptr) {
      return;  // Target no longer exists
    }

    // Calculate current toughness
    const CardDef *card = getCardDef(target->cardName);
    int toughness = (card != nullptr) ? card->toughness : 0;
    toughness += target->toughnessModifier;

    // Check for deathtouch
    bool deathtouch = hasKeyword(item.sourceName, "DEATHTOUCH");

    // Mark damage on the creature
    target->damage += effect.value;
    step.description += item.sourceName + " deals " + to_string(effect.value) +
                        " damage to " + target->cardName + ". ";

    // Check if creature dies (damage >= toughness, or any damage with deathtouch)
    if (target->damage >= toughness || (deathtouch && effect.value > 0)) {
      step.description += target->cardName + " is destroyed by lethal damage. ";
      destroyPermanent(item.targetId, step.triggeredEvents);
    }
  }
}

// Handle COUNTERSPELL
void Engine::resolveCounterEffect(const StackItem &item, ResolutionStep &step) {
  if (item.targetStackId.empty()) {
    step.description += item.sourceName + " has no stack target to counter. ";
    return;
  }

  // Find and remove the target spell from the stack
  auto it = find_if(state.stack.begin(), state.stack.end(), [&](const StackItem &si) { return si.id == item.targetStackId; });

  bool removed = (it != state.stack.end());
  if (removed) {
    state.stack.erase(it);
  }

  step.description += item.sourceName + (removed ? " counters " : " fails to find ") + item.targetStackId + ". ";
}

// Handle DESTROY
void Engine::resolveDestroyEffect(const StackItem &item, ResolutionStep &step) {
  if (!item.targetId.empty()) {
    Permanent *target = findPermanent(item.targetId);
    if (target != nullptr) {
      step.description += item.sourceName + " destroys " + target->cardName + ". ";
      destroyPermanent(item.targetId, step.triggeredEvents);
    }
  }
}

// Handle ADD_COUNTERS
void Engine::resolveAddCountersEffect(const StackItem &item, const Effect &effect, ResolutionStep &step) {
  if (!item.targetId.empty()) {
    Permanent *target = findPermanent(item.targetId);
    if (target != nullptr) {
      target->powerModifier += effect.value;
      target->toughnessModifier += effect.value;
      step.description += item.sourceName + " gives " + target->cardName + " +" + to_string(effect.value) + "/+" + to_string(effect.value) + ". ";
    }
  }
}

// Handle REMOVE_COUNTERS
void Engine::resolveRemoveCountersEffect(const StackItem &item, const Effect &effect, ResolutionStep &step) {
  if (!item.targetId.empty()) {
    Permanent *target = findPermanent(item.targetId);
    if (target != nullptr) {
      target->powerModifier -= effect.value;
      target->toughnessModifier -= effect.value;
      step.description += item.sourceName + " gives " + target->cardName + " -" + to_string(effect.value) + "/-" + to_string(effect.value) + ". ";

      // Check if creature dies from 0 toughness
      const CardDef *card = getCardDef(target->cardName);
      int toughness = (card != nullptr) ? card->toughness : 0;
      toughness += target->toughnessModifier;

      if (toughness <= 0) {
        step.description += target->cardName + " is put into the graveyard (0 toughness). ";
        destroyPermanent(item.targetId, step.triggeredEvents);
      }
    }
  }
}

// Handle CHANGE_POWER
void Engine::resolveChangePowerEffect(const StackItem &item, const Effect &effect, ResolutionStep &step) {
  if (!item.targetId.empty()) {
    Permanent *target = findPermanent(item.targetId);
    if (target != nullptr) {
      target->powerModifier += effect.value;
      step.description += item.sourceName + " changes " + target->cardName + " power by " + to_string(effect.value) + ". ";
    }
  }
}

// Handle CHANGE_TOUGHNESS
void Engine::resolveChangeToughnessEffect(const StackItem &item, const Effect &effect, ResolutionStep &step) {
  if (!item.targetId.empty()) {
    Permanent *target = findPermanent(item.targetId);
    if (target != nullptr) {
      target->toughnessModifier += effect.value;
      step.description += item.sourceName + " changes " + target->cardName + " toughness by " + to_string(effect.value) + ". ";

      // Check if creature dies from 0 toughness
      const CardDef *card = getCardDef(target->cardName);
      int toughness = (card != nullptr) ? card->toughness : 0;
      toughness += target->toughnessModifier;

      if (toughness <= 0) {
        step.description += target->cardName + " is put into the graveyard (0 toughness). ";
        destroyPermanent(item.targetId, step.triggeredEvents);
      }
    }
  }
}

// Handle BOUNCE
void Engine::resolveBounceEffect(const StackItem &item, ResolutionStep &step) {
  if (!item.targetId.empty()) {
    Permanent *target = findPermanent(item.targetId);
    if (target != nullptr) {
      step.description += item.sourceName + " returns " + target->cardName + " to its owner's hand. ";

      // Remove from all boards
      for (auto &[playerId, board] : state.boards) {
        auto newEnd = remove_if(board.permanents.begin(), board.permanents.end(), [&](const Permanent &p) { return p.id == item.targetId; });
        board.permanents.erase(newEnd, board.permanents.end());
      }
    }
  }
}

// Handle GAIN_LIFE
void Engine::resolveGainLifeEffect(const StackItem &item, const Effect &effect, ResolutionStep &step) {
  state.boards[item.controller].life += effect.value;
  step.description += item.controller + " gains " + to_string(effect.value) + " life. ";
}

// Handle LOSE_LIFE
void Engine::resolveLoseLifeEffect(const StackItem &item, const Effect &effect, ResolutionStep &step) {
  if (effect.target == TargetType::EACH_OPPONENT) {
    // Hit ALL opponents
    for (const auto &[playerId, board] : state.boards) {
      if (playerId != item.controller) {
        state.boards[playerId].life -= effect.value;
        step.description += playerId + " loses " + to_string(effect.value) + " life. ";
      }
    }
  } else {
    // Hit just one opponent
    for (const auto &[playerId, board] : state.boards) {
      if (playerId != item.controller) {
        state.boards[playerId].life -= effect.value;
        step.description += playerId + " loses " + to_string(effect.value) + " life. ";
        break;
      }
    }
  }
}

// Handle DRAW_CARDS
void Engine::resolveDrawCardsEffect(const StackItem &item, const Effect &effect, ResolutionStep &step) {
  output.cardsDrawn[item.controller] += effect.value;
  step.description += item.controller + " draws " + to_string(effect.value) + " card(s). ";
}

// ----------------------------- Main Resolution ----------------------------- //

void Engine::resolveSpell(const StackItem &item, ResolutionStep &step) {
  // Resolve a spell from the stack.

  const CardDef *card = getCardDef(item.sourceName);
  if (card == nullptr) {
    step.description = "Unknown spell: " + item.sourceName;
    return;
  }

  if (!item.targetId.empty()) {
    // Check if the target permanent still exists

    Permanent *target = findPermanent(item.targetId);

    if (target == nullptr) {
      step.description = item.sourceName + " fizzles - target no longer exists.";
      return;
    }

    // Check for hexproof
    if (hasKeyword(target->cardName, "HEXPROOF") && target->controller != item.controller) {
      step.description = item.sourceName + " fizzles - " + target->cardName + " has hexproof.";
      return;
    }

    // Check for shroud
    if (hasKeyword(target->cardName, "SHROUD")) {
      step.description = item.sourceName + " fizzles - " + target->cardName + " has shroud.";
      return;
    }
  }

  // Check if the target spell still exists
  if (card->spellTarget == TargetType::SPELL && !item.targetStackId.empty()) {
    bool exists = any_of(state.stack.begin(), state.stack.end(),
                         [&](const StackItem &si) { return si.id == item.targetStackId; });
    if (!exists) {
      step.description = item.sourceName + " fizzles - target spell no longer exists.";
      return;
    }
  }

  // Apply each effect of the spell
  for (const auto &effect : card->spellEffects) {
    switch (effect.type) {
    case EffectType::DEAL_DAMAGE:
      resolveDealDamageEffect(item, effect, step);
      break;
    case EffectType::COUNTERSPELL:
      resolveCounterEffect(item, step);
      break;
    case EffectType::DESTROY:
      resolveDestroyEffect(item, step);
      break;
    case EffectType::ADD_COUNTERS:
      resolveAddCountersEffect(item, effect, step);
      break;
    case EffectType::CHANGE_POWER:
      resolveChangePowerEffect(item, effect, step);
      break;
    case EffectType::CHANGE_TOUGHNESS:
      resolveChangeToughnessEffect(item, effect, step);
      break;
    case EffectType::REMOVE_COUNTERS:
      resolveRemoveCountersEffect(item, effect, step);
      break;
    case EffectType::BOUNCE:
      resolveBounceEffect(item, step);
      break;
    default:
      step.description += item.sourceName + " resolves. ";
      break;
    }
  }
}

void Engine::resolveTriggeredAbility(const StackItem &item, ResolutionStep &step) {
  // Resolve a triggered ability from the stack.

  const CardDef *card = getCardDef(item.sourceName);

  // Validate we can find the ability
  if (card == nullptr ||
      item.abilityIndex >= static_cast<int>(card->triggeredAbilities.size())) {
    step.description = item.sourceName + "'s ability resolves.";
    return;
  }

  const auto &ability = card->triggeredAbilities[item.abilityIndex];
  step.description = item.sourceName + "'s trigger: ";

  // Apply each effect of the ability
  for (const auto &effect : ability.effects) {
    switch (effect.type) {
    case EffectType::GAIN_LIFE:
      resolveGainLifeEffect(item, effect, step);
      break;
    case EffectType::LOSE_LIFE:
      resolveLoseLifeEffect(item, effect, step);
      break;
    case EffectType::DRAW_CARDS:
      resolveDrawCardsEffect(item, effect, step);
      break;
    case EffectType::DEAL_DAMAGE:
      resolveDealDamageEffect(item, effect, step);
      break;
    case EffectType::ADD_COUNTERS:
      resolveAddCountersEffect(item, effect, step);
      break;
    case EffectType::REMOVE_COUNTERS:
      resolveRemoveCountersEffect(item, effect, step);
      break;
    case EffectType::CHANGE_POWER:
      resolveChangePowerEffect(item, effect, step);
      break;
    case EffectType::CHANGE_TOUGHNESS:
      resolveChangeToughnessEffect(item, effect, step);
      break;
    default:
      break;
    }
  }
}

auto Engine::resolveTop() -> ResolutionStep {
  // Resolve the topmost item on the stack.

  ResolutionStep step;

  if (state.stack.empty()) {
    return step;
  }

  // Pop the top of the stack (LIFO - last in, first out)
  StackItem item = state.stack.back();
  state.stack.pop_back();

  if (g_debug) {
    cout << "[ENGINE] Resolving top: " << item.kind << " (" << item.sourceName << ")\n";
  }

  // After something resolves, active player gets priority
  state.priorityPlayer = state.activePlayer;

  // Resolve based on what type of thing it is
  if (item.kind == "SPELL") {
    resolveSpell(item, step);
  } else if (item.kind == "TRIGGERED_ABILITY") {
    resolveTriggeredAbility(item, step);
  }

  return step;
}

// Main simulation loop
auto Engine::run() -> Output {
  if (g_debug) {
    cout << "[ENGINE] Starting...\n";
  }

  // Keep resolving until the stack is empty
  while (!state.stack.empty()) {
    // Resolve the top item
    ResolutionStep step = resolveTop();

    // Check for any triggers that fired from events during resolution
    vector<PendingTrigger> allTriggers;
    for (const auto &event : step.triggeredEvents) {
      auto triggers = findTriggersForEvent(event);
      // Add to our collection
      for (const auto &t : triggers) {
        allTriggers.push_back(t);
      }
    }

    // If there are triggers, sort by APNAP and add to stack
    if (!allTriggers.empty()) {
      auto ordered = orderAPNAP(allTriggers);
      step.newTriggers = ordered;
      addTriggersToStack(ordered);
    }

    // Record this step in our output
    output.steps.push_back(step);
  }

  // Record final life totals
  for (const auto &[playerId, board] : state.boards) {
    output.finalLife[playerId] = board.life;
  }

  return output;
}

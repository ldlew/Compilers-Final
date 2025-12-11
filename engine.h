/*
  The main logic after the tokenizer/parser --- resolves the stack, tracks triggers,
  and other state changes.
*/

#ifndef ENGINE_H
#define ENGINE_H

#include "types.h"

using namespace std;

class Engine {
  // Simulates stack resolution (LIFO, checks triggers after each resolution)

  GameInput state;                // Current game state (modified as we resolve)
  Output output;                  // Results we're building up
  int triggerCount = 0;           // Ror generating trigger IDs

  auto getCardDef(const string &name) const -> const CardDef *;
  auto findPermanent(const ObjectID &objectId) -> Permanent *;
  auto getTurnOrder(const PlayerID &player) const -> int;
  auto hasKeyword(const string &cardName, const string &keyword) const -> bool;


  auto validatePriority(const StackItem &item) -> bool;


  static auto triggerMatches(const TriggerCondition &trig, const GameEvent &event, const Permanent &source) -> bool;
  auto findTriggersForEvent(const GameEvent &event) -> vector<PendingTrigger>;
  static auto orderAPNAP(vector<PendingTrigger> triggers) -> vector<PendingTrigger>;
  void addTriggersToStack(const vector<PendingTrigger> &triggers);

  auto resolveTop() -> ResolutionStep;
  void resolveSpell(const StackItem &item, ResolutionStep &step);
  void resolveTriggeredAbility(const StackItem &item, ResolutionStep &step);
  void destroyPermanent(const ObjectID &objectId, vector<GameEvent> &events);

  void resolveDealDamageEffect(const StackItem &item, const Effect &effect, ResolutionStep &step);
  void resolveCounterEffect(const StackItem &item, ResolutionStep &step);
  void resolveDestroyEffect(const StackItem &item, ResolutionStep &step);
  void resolveAddCountersEffect(const StackItem &item, const Effect &effect, ResolutionStep &step);
  void resolveRemoveCountersEffect(const StackItem &item, const Effect &effect, ResolutionStep &step);
  void resolveChangePowerEffect(const StackItem &item, const Effect &effect, ResolutionStep &step);
  void resolveChangeToughnessEffect(const StackItem &item, const Effect &effect, ResolutionStep &step);
  void resolveBounceEffect(const StackItem &item, ResolutionStep &step);
  void resolveGainLifeEffect(const StackItem &item, const Effect &effect, ResolutionStep &step);
  void resolveLoseLifeEffect(const StackItem &item, const Effect &effect, ResolutionStep &step);
  void resolveDrawCardsEffect(const StackItem &item, const Effect &effect, ResolutionStep &step);

public:
  explicit Engine(GameInput input);
  
  // Run until the stack is empty
  auto run() -> Output;
};

#endif

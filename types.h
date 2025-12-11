/*
  Core data structure for the project --- I use regex for JSON punctuation + handling, but
  got a little nervous doing that for something of this scope haha, prefer an OOP approach.
*/

#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <vector>
#include <unordered_map>

using namespace std;

// Debug flag
extern bool g_debug;

using PlayerID = string;
using ObjectID = string;

enum class EffectType {
  // What an ability/spell does (for now, will add more)
  DEAL_DAMAGE,
  GAIN_LIFE,
  LOSE_LIFE,
  DRAW_CARDS,
  COUNTERSPELL,
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
  BOUNCE
};

enum class TargetType {
  // What kind of object is being targeted
  NONE,
  ANY_TARGET,
  CREATURE,
  PLAYER,
  OPPONENT,
  EACH_OPPONENT,
  CONTROLLER,
  PERMANENT,
  SPELL
};

struct Effect {
  // A struct to scope an entire ability (type=DEAL_DAMAGE, value=3, target=CREATURE)
  EffectType type = EffectType::DEAL_DAMAGE;
  int value = 0;
  TargetType target = TargetType::NONE;
};

// ----------------------------- Triggered Abilities ----------------------------- //

enum class TriggerEvent {
  // Causes for a triggered ability (when 'x' happens...)
  ENTERS_BATTLEFIELD,
  DIES,
  ATTACKS,
  BLOCKS,
  DEALS_DAMAGE,
  DEALS_COMBAT_DAMAGE,
  BEGINNING_OF_UPKEEP,
  END_OF_TURN,
  SPELL_CAST,
  BECOMES_TARGET
};

enum class TriggerScope {
  // If a trigger has a scope (when 'this creature' enters the battlefield)
  SELF,
  ANY_CREATURE,
  ANOTHER_CREATURE,
  CREATURE_YOU_CONTROL,
  CREATURE_OPPONENT_CONTROLS,
  ANY_PLAYER
};

// The full "when" clause of a triggered ability
struct TriggerCondition {
  TriggerEvent event = TriggerEvent::ENTERS_BATTLEFIELD;
  TriggerScope scope = TriggerScope::SELF;
};

struct TriggeredAbility {
  TriggerCondition trigger;                 // Struct to scope the triggered ability (when X, Y happens)
  vector<Effect> effects;                   // The 'y' in the `when x, y`
  bool isMay = false;                       // Sometimes abilities can be optionally triggered.
  string text;                              // Rules text.
};

struct PendingTrigger {
  // A trigger that's waiting to go on the stack (for APNAP)
  ObjectID sourceId;
  string sourceName;
  PlayerID controller;
  int abilityIndex = 0;
  string text;
  bool isActivePlayer = false;
  int turnOrder = 0;
};

// ----------------------------- Card/Type Templates ----------------------------- //

struct CardDef {
  // Card blueprints
  string name;
  vector<string> types;
  vector<string> subtypes;
  vector<string> keywords;
  string rulesText;

  int power = 0;
  int toughness = 0;

  TargetType spellTarget = TargetType::NONE;
  vector<Effect> spellEffects;

  vector<TriggeredAbility> triggeredAbilities;
};

struct Permanent {
  // A card on the battlefield
  ObjectID id;
  string cardName;
  PlayerID controller;

  bool tapped = false;
  int damage = 0;
  int powerModifier = 0;
  int toughnessModifier = 0;
  int counters = 0;
};

struct StackItem {
  // Something on the stack (spell or ability) waiting to resolve
  ObjectID id;
  string kind;

  string sourceName;                        // Source of the ability
  ObjectID sourceId;                        // For abilities: which permanent it came from
  int abilityIndex = 0;                     // Which ability on the card (if multiple)
  PlayerID controller;

  ObjectID targetId;                        // Target permanent
  PlayerID targetPlayer;                    // Target player
  ObjectID targetStackId;                   // For counterspells: what is being countered
};

// ----------------------------- Board & Game State ----------------------------- //

struct Board {
  // One player's side of the battlefield
  PlayerID player;
  static constexpr int kStartingLife = 20;
  int life = kStartingLife;
  vector<Permanent> permanents;
};

struct GameEvent {
  // Something that happened that might trigger abilities
  TriggerEvent type;
  ObjectID objectId;
  string cardName;
  PlayerID controller;
};

struct GameInput {
  // The complete input state
  unordered_map<string, CardDef> cards;     // Card database
  PlayerID activePlayer;                    // Whose turn it is
  PlayerID priorityPlayer;                  // Who can act right now
  string currentPhase;

  unordered_map<PlayerID, Board> boards;    // Each player's battlefield
  vector<StackItem> stack;                  // The stack to resolve
};

struct ResolutionStep {
  // What happened when one stack item resolved (for output)
  string description;
  vector<GameEvent> triggeredEvents;        // Events that happened
  vector<PendingTrigger> newTriggers;       // New triggers that fired
};

struct Output {
  // Final result
  bool valid = true;
  vector<string> errors;

  vector<ResolutionStep> steps;             // Play-by-play of what happened

  unordered_map<PlayerID, int> finalLife;
  vector<ObjectID> destroyedPermanents;
  unordered_map<PlayerID, int> cardsDrawn;
};

#endif

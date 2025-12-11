# MTG Stack Validator

## Brief Introduction
My partner and I are addicted to the card-game Magic the Gathering. MTG is an incredibly dense game (the
rulebook itself is over 900 pages long), and some situations aren't exactly explicitly covered. This has
lead to many an argument, and several hours of angry Googling to justify "no, no, it works like THIS."

And so I got the idea halfway through the semester to build a tool that I've always wanted to exist. MTG has
a game-zone known as "the stack" where, when players cast spells, they are resolved LIFO. However, it gets
messy with passing priorities, board-triggers, layers --- eventually, you have no idea what triggers when.

And so I decided I'm going to build a tool (and will continue building this out after the semester is over).

Quick note: this is my first time really using clang with C++ so formatting might be a little strange in places /
different from the Quilt program layout. Lemme know if you have questions!

---

## Scope of the Project
Since I have to complete this project in a few weeks, I am, of course, limiting the scope to a few rules:

- Stack resolves LIFO.
- Target legality. Spells will fizzle if the target permanent does not exist, or cannot be targeted (hexproof / shroud).
- Triggered abilities (if/when/whenever). Finds trigger condition, produces a pending trigger, which will push to the stack APNAP.
- Only a few supported spell effects right now.
- Each pop produces a 'ResolutionStep' with details.

The parsed input will be in a JSON format as it's derived from a web interface, so it's a pseudo-parser for JSON as well.

---

## How to Run
Use the web interface (run index.html locally with 'Live Server' extension, or the like) to generate a valid JSON object.
Did not have time to get the API stuff working, so for now, copy-and-paste it into ./data/input.json (or just leave it default).

Then...
```bash
make run

# For linting
make lint

# Linting individual example
make lint tokenizer
```
---

## Overview
`types.h` Defines shared enums/structs (cards, triggers, effects, boards, stack items, output)
`tokenizer` Tokenizes the JSON-formatted and MTG keywords
`parser` Walks tokens to build the AST + calls the ability parser for text
`ability_parser` Converts card rules into triggers / effects / targets
`engine` Resolves the stack LIFO, checks targets, applies APNAP ordering for triggers, records each step
`main` Loads input.json, invokes parser/engine, and prints all the states

---
#include "engine.h"
#include "parser.h"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

bool g_debug = false;

auto readFile(const string &filename) -> string {
  // Read entire file into a string (empty if it fails).

  ifstream fileStream(filename);
  if (!fileStream) {
    return "";
  }

  // Read entire file into stringstream, then return as string
  stringstream buf;
  buf << fileStream.rdbuf();
  return buf.str();
}

namespace {
  // Output formatting

  void printErrors(const Output &out) {
    if (out.errors.empty()) {
      return;
    }

    cout << "ERRORS:\n";
    for (const auto &err : out.errors) {
      cout << "  ! " << err << '\n';
    }
    cout << '\n';
  }

  void printSteps(const Output &out) {
    // Print the step-by-step resolution log

    if (out.steps.empty()) {
      cout << "Stack was empty, nothing to resolve.\n";
      return;
    }

    int stepNum = 1;
    for (const auto &step : out.steps) {
      cout << "STEP " << stepNum++ << ": " << step.description << '\n';

      // Show any triggers that fired during this step
      if (!step.newTriggers.empty()) {
        cout << "  >> TRIGGERS DETECTED (APNAP order):\n";

        for (const auto &trigger : step.newTriggers) {
          cout << "     - " << trigger.sourceName << " [" << trigger.controller;
          if (trigger.isActivePlayer) {
            cout << ", Active Player";
          } else {
            cout << ", Non-Active Player";
          }
          cout << "]\n";
          cout << "       \"" << trigger.text << "\"\n";
        }
      }
      cout << '\n';
    }
  }

  void printFinalState(const Output &out) {
    // Print the final game state after resolution

    cout << "FINAL STATE\n";

    // Life totals
    for (const auto &[player, life] : out.finalLife) {
      cout << "  " << player << ": " << life << " life\n";
    }

    // Cards drawn
    for (const auto &[player, cards] : out.cardsDrawn) {
      if (cards > 0) {
        cout << "  " << player << " drew " << cards << " card(s)\n";
      }
    }

    // Destroyed permanents
    if (!out.destroyedPermanents.empty()) {
      cout << "  Destroyed: ";
      for (size_t i = 0; i < out.destroyedPermanents.size(); i++) {
        if (i > 0) {
          cout << ", ";
        }
        cout << out.destroyedPermanents[i];
      }
      cout << '\n';
    }
  }
}

void printOutput(const Output &out) {
  // Print the full simulation results

  cout << "STACK RESOLUTION\n\n";
  printErrors(out);
  printSteps(out);
  printFinalState(out);
}

auto main(int argc, char *argv[]) -> int {
  // Entry point.

  try {
    // Default input file
    string filename = "data/input.json";

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
      string arg = argv[i];
      if (arg == "--debug" || arg == "-d") {
        g_debug = true;
        cout << "[DEBUG] Debug mode enabled.\n\n";
      } else {
        filename = arg;
      }
    }

    // Read the input file
    setFilename(filename);
    string json = readFile(filename);
    if (json.empty()) {
      cerr << "Error: Could not read " << filename << '\n';
      return 1;
    }

    // Parse the JSON
    Parser parser(json);
    GameInput input = parser.parse();

    // Print some info about what we parsed
    cout << "Parsed " << input.cards.size() << " card definitions\n";
    cout << "Active player: " << input.activePlayer << '\n';
    cout << "Priority: " << input.priorityPlayer << '\n';
    if (!input.currentPhase.empty()) {
      cout << "Current Phase: " << input.currentPhase << '\n';
    }
    cout << "Stack size: " << input.stack.size() << '\n';
    cout << '\n';

    // Run
    Engine engine(input);
    Output out = engine.run();

    // Print
    printOutput(out);

  } catch (const exception &e) {
    // Other errors
    cerr << "Error: " << e.what() << '\n';
    return 1;
  }

  return 0;
}

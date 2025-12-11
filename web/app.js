let CARDS = {};

const state = { p1: [], p2: [], stack: [] };
let permCount = 0;

const elements = {};

const isCreature = (name) => CARDS[name]?.types?.includes("CREATURE");
const isSpell = (name) => {
  const types = CARDS[name]?.types || [];
  return types.includes("INSTANT") || types.includes("SORCERY");
};

function cacheElements() {
  elements.activePlayer = document.getElementById("activePlayer");
  elements.p1Life = document.getElementById("p1_life");
  elements.p2Life = document.getElementById("p2_life");
  elements.p1Card = document.getElementById("p1_card");
  elements.p2Card = document.getElementById("p2_card");
  elements.stackController = document.getElementById("stack_controller");
  elements.stackSpell = document.getElementById("stack_spell");
  elements.stackTarget = document.getElementById("stack_target");
  elements.p1Perms = document.getElementById("p1_perms");
  elements.p2Perms = document.getElementById("p2_perms");
  elements.stack = document.getElementById("stack");
  elements.output = document.getElementById("output");
  elements.generateBtn = document.getElementById("generateBtn");
  elements.status = document.getElementById("status");
}

async function loadCards() {
  try {
    let response = await fetch("/cards");
    if (!response.ok) {
      response = await fetch("../data/cards.json");
    }
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    const data = await response.json();
    CARDS = data.cards || {};
    populateSelects();
    render();
    if (elements.status) {
      elements.status.textContent = "Cards loaded. Build stack then run the C++ engine for results.";
    }
  } catch (err) {
    if (elements.status) {
      elements.status.textContent = `Failed to load cards: ${err.message}`;
    }
  }
}

function populateSelects() {
  const creatures = Object.keys(CARDS).filter(isCreature);
  const spells = Object.keys(CARDS).filter(isSpell);

  [elements.p1Card, elements.p2Card].forEach((select) => {
    select.innerHTML = '<option value="">Add creature...</option>';
    creatures.forEach((name) => {
      select.innerHTML += `<option value="${name}">${name}</option>`;
    });
  });

  elements.stackSpell.innerHTML = '<option value="">Cast spell...</option>';
  spells.forEach((name) => {
    elements.stackSpell.innerHTML += `<option value="${name}">${name}</option>`;
  });
}

function updateTargets() {
  const all = [...state.p1, ...state.p2];
  elements.stackTarget.innerHTML = '<option value="">No target</option><option value="p1">Player 1</option><option value="p2">Player 2</option>';
  all.forEach((perm) => {
    elements.stackTarget.innerHTML += `<option value="${perm.id}">${perm.name} (${perm.id})</option>`;
  });
  state.stack.forEach((entry) => {
    elements.stackTarget.innerHTML += `<option value="stack:${entry.id}">Stack item: ${entry.id} (${entry.sourceName})</option>`;
  });
}

function addPerm(player) {
  const name = (player === "p1" ? elements.p1Card : elements.p2Card).value;
  if (!name) {
    return;
  }
  const id = `${player}_${++permCount}`;
  state[player].push({ id, name, controller: player });
  render();
}

function removePerm(player, id) {
  state[player] = state[player].filter((perm) => perm.id !== id);
  render();
}

function addSpell() {
  const spell = elements.stackSpell.value;
  if (!spell) {
    return;
  }
  const target = elements.stackTarget.value;
  const controller = elements.stackController.value || elements.activePlayer.value;

  state.stack.push({
    id: `spell_${state.stack.length + 1}`,
    kind: "SPELL",
    sourceName: spell,
    controller,
    targetId: target.startsWith("stack:") ? "" : (target && target !== "p1" && target !== "p2" ? target : ""),
    targetStackId: target.startsWith("stack:") ? target.replace("stack:", "") : "",
    targetPlayer: target === "p1" || target === "p2" ? target : ""
  });
  render();
}

function removeSpell(index) {
  state.stack.splice(index, 1);
  render();
}

function renderPermanents(player, container) {
  container.innerHTML = state[player].map((perm) => {
    const card = CARDS[perm.name];
    const stats = card && card.power !== undefined && card.toughness !== undefined
      ? `${card.power}/${card.toughness}`
      : "";
    const triggerHtml = card?.text ? `<div class="trigger-info">⚡ ${card.text}</div>` : "";

    return `<div class="item">
      <div>
        <span class="item-name">${perm.name}</span>
        <span class="item-id">${perm.id}</span>
        <span class="item-info">${stats}</span>
        ${triggerHtml}
      </div>
      <button class="btn btn-remove" data-player="${player}" data-id="${perm.id}">×</button>
    </div>`;
  }).join("");
}

function renderStack() {
  elements.stack.innerHTML = state.stack.map((entry, index) => {
    const targetDesc = entry.targetStackId ? `stack:${entry.targetStackId}` : (entry.targetId || entry.targetPlayer || "no target");
    return `<div class="item stack-item">
      <div>
        <span class="item-name">${index + 1}. ${entry.controller} casts ${entry.sourceName}</span>
        <span class="item-info">→ ${targetDesc}</span>
      </div>
      <button class="btn btn-remove" data-stack-index="${index}">×</button>
    </div>`;
  }).join("");
}

function render() {
  renderPermanents("p1", elements.p1Perms);
  renderPermanents("p2", elements.p2Perms);
  renderStack();
  updateTargets();
}

function buildJson() {
  const usedCards = new Set();
  [...state.p1, ...state.p2].forEach((perm) => usedCards.add(perm.name));
  state.stack.forEach((entry) => usedCards.add(entry.sourceName));

  const cards = {};
  usedCards.forEach((name) => { if (CARDS[name]) { cards[name] = CARDS[name]; } });

  return {
    cards,
    activePlayer: elements.activePlayer.value,
    boards: {
      p1: { player: "p1", life: parseInt(elements.p1Life.value, 10), permanents: state.p1 },
      p2: { player: "p2", life: parseInt(elements.p2Life.value, 10), permanents: state.p2 }
    },
    stack: state.stack
  };
}

function generate() {
  const json = buildJson();
  elements.output.textContent = JSON.stringify(json, null, 2);
  return json;
}

function bindEvents() {
  elements.p1Card.closest(".row").querySelector(".btn-add").addEventListener("click", () => addPerm("p1"));
  elements.p2Card.closest(".row").querySelector(".btn-add").addEventListener("click", () => addPerm("p2"));
  document.getElementById("castSpellBtn").addEventListener("click", addSpell);
  elements.generateBtn.addEventListener("click", generate);

  elements.p1Perms.addEventListener("click", (event) => {
    const button = event.target.closest("[data-player][data-id]");
    if (!button) {
      return;
    }
    removePerm(button.dataset.player, button.dataset.id);
  });

  elements.p2Perms.addEventListener("click", (event) => {
    const button = event.target.closest("[data-player][data-id]");
    if (!button) {
      return;
    }
    removePerm(button.dataset.player, button.dataset.id);
  });

  elements.stack.addEventListener("click", (event) => {
    const button = event.target.closest("[data-stack-index]");
    if (!button) {
      return;
    }
    removeSpell(Number(button.dataset.stackIndex));
  });
}

function init() {
  cacheElements();
  bindEvents();
  loadCards();
  if (elements.status) {
    elements.status.textContent = "Generate JSON, then run the C++ engine to see stack results in the terminal.";
  }
}

document.addEventListener("DOMContentLoaded", init);

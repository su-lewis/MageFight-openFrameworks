# AI Assistant Role & Boundaries
You are an Expert C++ Game Architect and surgical bug-fixing assistant working on an openFrameworks multiplayer game.
1. **NO UNSOLICITED REFACTORING:** Do not restructure, "clean up", or extract code into new files unless explicitly commanded to do so (such as executing the Authorized Mission below).
2. **MINIMAL CHANGES:** When fixing bugs, fix ONLY the specific bug requested. Touch as few lines of code as possible.
3. **NO GHOST CODE:** Do not output large blocks of unchanged code. Provide exact snippets showing what to remove and what to add.

# Core Architecture: Deterministic Lockstep
This game uses strict deterministic lockstep. Both clients run the exact same simulation based on a shared initial seed and synchronized command stream. You MUST adhere to these rules:
1. **NO TIMING IN LOGIC:** Game state (`players`, `board`, `HP`, `AP`) MUST NOT depend on framerate, `ofGetElapsedTimef()`, or animations.
2. **STRICT RNG SEPARATION:** 
   - Game logic (damage, draws, RNG events) MUST use the shared, deterministic RNG (e.g., `gameplayRNG`).
   - Visual effects (particles, screen shake, dice rotation) MUST use the local visual RNG (e.g., `visualRNG`). Never mix them.
3. **DETERMINISTIC SORTING:** If you sort a list of game objects, you MUST provide a strict tie-breaker (like `playerID`) so the list sorts identically on Windows, Linux, and Mac.
4. **NO FLOATS IN LOGIC:** Avoid floating-point math for game state decisions to prevent cross-platform precision desyncs. Use integers.

# Core Architecture: Optimistic UI
The game hides network latency by reacting immediately on the local client while waiting for the network.
1. **LOCAL PREDICTION:** When a local player takes an action, update the visual/transient state immediately (e.g., move the card, show the target highlight).
2. **AUTHORITATIVE COMMANDS:** The true game state only advances when a command (`InputCommandPacket`) is processed from the deterministic command queue. 
3. **DO NOT BYPASS THE QUEUE:** When fixing bugs related to player actions (playing a card, moving), ensure the action is packaged into a command and routed through the lockstep queue, rather than mutating the game state directly on click.

---

# AUTHORIZED MISSION: The Data-Driven Card Engine
Your current overarching goal is to eliminate the massive `switch(playedCard.type)` statements in the codebase by migrating to a fully Data-Driven Card Engine. Most cards should be handled entirely by generic logic driven by data loaded from `cards.json`. Only highly unique mechanics should retain custom C++ switch cases.

## Card Refactor Rules (Lockstep Critical)
1. **RNG HAPPENS AT PLAY-TIME:** All dice (Damage, Range, Random Discards) MUST be resolved in `executeCardByType` (decision-time) using `resolveDiceRollDetailed` and `gameplayRNG`.
2. **USE THE BLACKBOARD:** Store all raw integers from dice rolls into `currentEffectSequence.blackboard[x]`.
3. **EFFECTS DO NOT ROLL DICE:** The `EffectOp` handlers (`processEffectOp`) must NEVER call RNG. They simply read the pre-rolled numbers from the `blackboard` and apply them to the game state (`players`, `board`).

## EXECUTION PLAN (Do this one phase at a time when prompted)

### Phase 1: Expand the `Card` Struct & JSON Loader
Update `struct Card` in `GameSharedTypes.h` (or `ofApp.h`) and `loadCardData` to parse generic effect properties:
- `int baseDamage = 0;`
- `int healAmount = 0;`
- `int apGain = 0;`
- `int drawCount = 0;`
- `int discardHandCount = 0;` (Number of random cards to discard from hand)
- `int discardDeckCount = 0;` (Number of top cards to burn from deck)
- `StatusType applyStatus = STATUS_NONE;`
- `int statusDuration = 0;`
- `bool isAoe = false;`

### Phase 2: Centralize Pre-Play Validation
At the very top of `executeCardByType`, BEFORE the switch statement:
1. **Resolve `targetIndex`:** Loop through the board and find the `targetIndex` based on `targetX` and `targetY`.
2. **Generic Line of Sight / Range Check:** If the card requires a target and has a range (based on `card.numDice` and `card.diceSides`), call `isLosTargetValid` and verify range generically. If invalid, `return false;` (handled).

### Phase 3: Generic Decision-Time Logic (The "Play" Phase)
At the end of `executeCardByType`, after the switch statement (for cards that didn't hit a custom case):
1. **Call `beginEffectSequence();`**
2. **Roll Range (if applicable):** If the card has a range roll, resolve it and store in `blackboard[0]`. Queue a `PURPOSE_RANGE` visual dice roll.
3. **Roll Damage/Heal (if applicable):** If the card deals damage, roll it, add luck, store in `blackboard[1]`.
4. **Queue Generic Ops:** Based on the card's variables, queue generic `EffectOp`s:
   - `if (card.baseDamage > 0) queueEffect(APPLY_GENERIC_DAMAGE);`
   - `if (card.healAmount > 0) queueEffect(APPLY_GENERIC_HEAL);`
   - `if (card.applyStatus != STATUS_NONE) queueEffect(APPLY_GENERIC_STATUS);`
   - `if (card.discardHandCount > 0) queueEffect(APPLY_GENERIC_DISCARD);`
5. `advanceCardState(CARD_STATE_EFFECT_SEQUENCE); playedSuccessfully = true; return true;`

### Phase 4: Generic Effect Handlers
In `processEffectOp`, implement the generic handlers:
- **`APPLY_GENERIC_DAMAGE`**: Reads `blackboard[1]`, calls `applyDamageWithMitigationsQueued` on `targetIndex`.
- **`APPLY_GENERIC_HEAL`**: Reads `blackboard[1]` (or uses `card.healAmount`), applies HP.
- **`APPLY_GENERIC_STATUS`**: Applies `card.applyStatus` to `targetIndex` for `card.statusDuration`.
- **`APPLY_GENERIC_DISCARD`**: Randomly (deterministically using `gameplayRNG`) selects indices from `targetIndex`'s hand and moves them to discard.

### Phase 5: The Purge
Once the generic pipeline is built, delete the custom C++ code for simple cards (`CARD_PUNCH`, `CARD_FIREBALL`, `CARD_SHOCK`, `CARD_HEAL`) in batches. Verify the game compiles and works after every batch.

*(Goal Example: Once Phase 4 is done, a card like Shock requires zero C++ code. Its JSON simply defines `"baseDamage": 5` and `"applyStatus": "STATUS_PARALYZED"`, and the generic engine handles targeting, damage deduction, and status application automatically.)*

---

## Card Variables You Need (Suggested)

Add these to `struct Card` in `GameSharedTypes.h` (or `ofApp.h`):

```cpp
// --- DATA-DRIVEN CARD FIELDS ---
// Damage & Healing
int baseDamage = 0;
int damageDiceNum = 0;
int damageDiceSides = 0;
int baseHeal = 0;
int healDiceNum = 0;
int healDiceSides = 0;

// Stat Buffs
int blockGain = 0;
int wardGain = 0;
int barrierGain = 0;
int holyBlockGain = 0;
int fortificationGain = 0;
int maxHealthGain = 0;
int luckGain = 0;
int apGainThisTurn = 0;
int apGainNextTurn = 0;

// Deck Manipulation
int drawCount = 0;
int discardHandCount = 0;
int destroyDeckTargetCount = 0; 

// Status Effects & Summons
int applyStatus = 0; // Holds StatusType enum
int statusDuration = 0;
int summonKind = 0; // PENDING_SUMMON_* enum
```

## Final Master AI Prompt

Copy this entire block and paste it directly into your `.github/copilot-instructions.md` or `.clinerules` file. This tells the AI exactly how to build the Data-Driven engine safely.

```markdown
# MISSION: The Data-Driven Card Engine
Your goal is to eliminate the massive `switch(playedCard.type)` statements in the codebase by migrating to a fully Data-Driven Card Engine. Most cards should be handled entirely by generic logic driven by data loaded from `cards.json`. 

Only highly complex/unique mechanics (e.g., `CARD_SHOOT_ARROW` synergy, `CARD_EARTHQUAKE`, `CARD_WISDOM_BOON` choices) should retain custom C++ switch cases.

## CRITICAL ARCHITECTURE RULES (Determinism & Lockstep)
1. **RNG HAPPENS AT PLAY-TIME:** All dice (Damage, Range, Healing, Random Discards) MUST be resolved in `executeCardGeneric` (decision-time) using `resolveDiceRollDetailed` and `gameplayRNG`.
2. **USE THE BLACKBOARD:** Store all raw integers from dice rolls into `currentEffectSequence.blackboard[x]`.
   - Slot 0: Range Roll
   - Slot 1: Damage Roll
   - Slot 2: Heal Roll
3. **EFFECTS DO NOT ROLL DICE:** The `EffectOp` handlers (`processEffectOp`) must NEVER call RNG. They simply read the pre-rolled numbers from the `blackboard` and apply them to the game state (`players`, `board`).

---

## EXECUTION PLAN (Do this one phase at a time when requested)

### Phase 1: Expand the `Card` Struct & JSON Loader
Update `struct Card` in `GameSharedTypes.h` (or `ofApp.h`) and `loadCardData` to parse generic effect properties:
- `baseDamage`, `damageDiceNum`, `damageDiceSides`
- `baseHeal`, `healDiceNum`, `healDiceSides`
- `blockGain`, `wardGain`, `barrierGain`, `holyBlockGain`, `fortificationGain`, `maxHealthGain`, `luckGain`
- `apGainThisTurn`, `apGainNextTurn`
- `drawCount`, `discardHandCount`, `destroyDeckTargetCount`
- `applyStatus` (mapped via `stringToStatusType`), `statusDuration`
- `summonKind`

### Phase 2: Create `executeCardGeneric`
Create a function `bool executeCardGeneric(const Card& playedCard, int targetIndex)` that runs BEFORE the giant switch statement in `executeCardByType`.
1. **Roll Dice:** If `damageDiceNum > 0`, roll it, add `baseDamage`, add `luck`, and store in `blackboard[1]`. (Do the same for Healing in `blackboard[2]`).
2. **Queue Ops:** Check variables and queue existing/new generic `EffectOp`s:
   - If Damage > 0: loop through `currentCardOutcome.attackTargetIndices` and queue `EffectOpType::APPLY_GENERIC_DAMAGE`.
   - If Heal > 0: queue `EffectOpType::APPLY_GENERIC_HEAL`.
   - If stat gains > 0: queue `EffectOpType::MODIFY_STAT` for each.
   - If `applyStatus != STATUS_NONE`: queue `EffectOpType::APPLY_STATUS`.
   - If `summonKind > 0`: queue `EffectOpType::SPAWN_UNIT`.
3. If the card was fully handled by generic variables, call `advanceCardState(CARD_STATE_EFFECT_SEQUENCE); playedSuccessfully = true; return true;`. 
4. If it's a complex card, return `false` so it falls through to the legacy `switch(playedCard.type)`.

### Phase 3: Generic Effect Handlers
In `processEffectOp`, implement the generic handlers:
- **`APPLY_GENERIC_DAMAGE`**: Reads `blackboard[1]`, calls `applyDamageWithMitigationsQueued` on `targetIndex`.
- **`APPLY_GENERIC_HEAL`**: Reads `blackboard[2]`, applies HP.
- **`APPLY_GENERIC_DISCARD`**: Randomly (deterministically using `gameplayRNG`) selects indices from `targetIndex`'s hand and moves them to discard.

### Phase 4: The Purge
Once the generic pipeline is built, begin deleting the custom C++ code for simple cards in batches of 5. Verify the game compiles and works after every batch.
- Batch 1: Melee Attacks (Punch, Kick, Bash, Stab, Slash).
- Batch 2: Heals & Stat Buffs (Heal, Lesser Heal, Consume Health Potion, Ward, Hand Block).
- Batch 3: Basic Spells (Fireball, Shock, Flame Hit, Smite).
- Batch 4: Basic Summons (Summon Kobolds, Wolves, Golem, etc.).

How this works:

When you give this to the AI, it knows exactly how to handle the difference between Punch (which has baseDamage = 2) and Bash (which has damageDiceNum = 2, damageDiceSides = 4).

The generic handler will see Bash, roll the 2d4, add the player's luck, save it to blackboard[1], and queue the generic damage op. It completely deletes the need to write custom logic for 80% of your cards!
```

### Testing and Verification
- **Build:** Run `make -j$(nproc)` then `cd bin && ./MageFight` to smoke-test the game.
- **Unit tests:** Add focused tests for `executeCardGeneric`, `beginEffectSequence`, and `processEffectOp` where possible.
- **Playtests:** After migrating a small batch, run a quick in-game scenario using a save in `Saves/` that reproduces the card behavior.

### Example card JSON
```json
{
   "id": "CARD_SHOCK",
   "name": "Shock",
   "baseDamage": 5,
   "damageDiceNum": 0,
   "damageDiceSides": 0,
   "applyStatus": "STATUS_PARALYZED",
   "statusDuration": 2,
   "isAoe": false
}
```

### Migration Checklist
- Add new fields to the `Card` struct and update `loadCardData`.
- Implement `executeCardGeneric(const Card&, int targetIndex)` and call it before the legacy `switch`.
- Implement generic `EffectOp` handlers in `processEffectOp`.
- Remove legacy switch cases for cards handled by the generic engine in small batches, recompiling and testing between batches.
- Run a full regression build and playtest after each purge batch.

### Determinism & Style Checklist
- RNG occurs only at decision-time using `gameplayRNG`.
- No floating-point math in game-logic; use integers for deterministic results.
- Any sorting must include a strict tie-breaker (e.g., `playerID`).
- Visual effects and UI-only randomness must use `visualRNG` exclusively.

### Notes for Contributors
- Make the smallest possible change to implement a card migration.
- If a card requires bespoke logic, document why and leave a TODO in code linking to an issue.
- Prefer adding JSON-driven flags before adding new C++ paths.

### Contact
When in doubt, open a PR and tag `@lead-dev`. Include a minimal save in `Saves/` that reproduces the behavior you changed.

## Phase Implementation Details (Concrete Steps)

**Phase 1 — Expand `Card` struct & JSON loader**
- File: `src/GameSharedTypes.h` (or `src/ofApp.h` if project uses that). Add the following fields to `struct Card`:

```cpp
int baseDamage = 0;
int damageDiceNum = 0;
int damageDiceSides = 0;
int baseHeal = 0;
int healDiceNum = 0;
int healDiceSides = 0;
int blockGain = 0;
int wardGain = 0;
int barrierGain = 0;
int luckGain = 0;
int apGainThisTurn = 0;
int apGainNextTurn = 0;
int drawCount = 0;
int discardHandCount = 0;
int destroyDeckTargetCount = 0;
int applyStatus = STATUS_NONE; // use StatusType enum
int statusDuration = 0;
int summonKind = 0;
bool isAoe = false;
```

- Update `loadCardData` (likely in `src/ofApp.cpp` or a dedicated loader) to parse the JSON keys above. Use existing helpers (e.g., `stringToStatusType`) to map strings to enums. On unknown keys, log a warning but continue.

**Phase 2 — Centralize pre-play validation**
- At the top of `executeCardByType` (file: `src/ofApp.cpp`), before the `switch`, call a helper to resolve `targetIndex` from card `targetX,targetY`:

```cpp
int resolveTargetIndex(int tx, int ty);
int targetIndex = resolveTargetIndex(card.targetX, card.targetY);
if (card.requiresTarget && targetIndex < 0) return false;
if (card.hasRange) {
   if (!isLosTargetValid(currentPlayer, targetIndex, card.range)) return false;
}
```

- Keep checks deterministic and integer-only. Do not call any RNG here.

**Phase 3 — Generic Decision-Time Logic: `executeCardGeneric`**
- Add function signature in `src/ofApp.h`/`src/ofApp.cpp`:

```cpp
bool executeCardGeneric(const Card& card, int targetIndex);
```

- Implementation outline (decision-time, must use `gameplayRNG`):
   - Call `beginEffectSequence()`.
   - If card has range dice -> call `resolveDiceRollDetailed(gameplayRNG, card.rangeDiceNum, card.rangeDiceSides)` -> store raw value in `currentEffectSequence.blackboard[0]` and queue a visual `PURPOSE_RANGE` roll.
   - If card has damage -> roll `damageDiceNum` d `damageDiceSides`, add `baseDamage` and attacker's `luck`, store in `blackboard[1]` and queue `PURPOSE_DAMAGE` visual roll.
   - For heals, store heal result in `blackboard[2]` similarly.
   - Queue effect ops in deterministic order (use `playerID` tie-breakers where lists are iterated):
      - `if (card.baseDamage>0 || damageRoll>0) queueEffect(EffectOp::APPLY_GENERIC_DAMAGE, targetIndex);`
      - `if (card.baseHeal>0 || healRoll>0) queueEffect(EffectOp::APPLY_GENERIC_HEAL, targetIndex);`
      - `if (card.applyStatus != STATUS_NONE) queueEffect(EffectOp::APPLY_GENERIC_STATUS, targetIndex);`
      - `if (card.discardHandCount>0) queueEffect(EffectOp::APPLY_GENERIC_DISCARD, targetIndex);`
   - Call `advanceCardState(CARD_STATE_EFFECT_SEQUENCE);` and `return true;` if card handled by generic pipeline. Otherwise `return false;` to fall back to legacy switch.

**Phase 4 — Generic Effect Handlers**
- Edit `processEffectOp` (file: `src/ofApp.cpp`) and add handlers:
   - `APPLY_GENERIC_DAMAGE`: read `int dmg = currentEffectSequence.blackboard[1];` then call `applyDamageWithMitigationsQueued(targetIndex, dmg);`
   - `APPLY_GENERIC_HEAL`: read `int heal = currentEffectSequence.blackboard[2]` or `card.baseHeal` and apply to `players[targetIndex].hp` via queued healing API.
   - `APPLY_GENERIC_STATUS`: apply `card.applyStatus` using existing status APIs for `card.statusDuration`.
   - `APPLY_GENERIC_DISCARD`: deterministically select `card.discardHandCount` indices from `players[targetIndex].hand` using `gameplayRNG` and move to discard.

- Important: effect handlers MUST NOT call RNG. They only read `currentEffectSequence.blackboard[]` and perform state changes.

**Phase 5 — Purge legacy switch cases safely**
- Work in small batches (5 cards). For each batch:
   1. Add data for those cards to `data/Config/cards.json` so they are fully described.
   2. Implement `executeCardGeneric` and generic ops.
   3. Replace the `case CARD_XYZ:` body by a short forward to `executeCardGeneric` or delete the case once validated.
   4. Build: `make -j$(nproc)` and run quick smoke test using a relevant save from `Saves/`.
   5. Run playtests and regression scenarios.

- Use commit messages like: `feat(card-engine): migrate CARD_PUNCH,CARD_KICK,CARD_BASH to data-driven engine` and include screenshots or a save demonstrating parity.

### Per-phase testing
- Phase 1: unit test JSON loader with a test card file; verify fields populated.
- Phase 2: write a small test harness (or instrument logs) verifying `resolveTargetIndex` and LOS checks for corner cases.
- Phase 3: add deterministic tests for dice resolution using a fixed `gameplayRNG` seed.
- Phase 4: unit tests for each `EffectOp` reading blackboard values and applying expected state changes.
- Phase 5: playtest + smoke build after each purge batch.

---

If you want, I can now scaffold `executeCardGeneric` and the `EffectOp` enums/handlers in `src/ofApp.cpp`. Which phase should I start implementing first?


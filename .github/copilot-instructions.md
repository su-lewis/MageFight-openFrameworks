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
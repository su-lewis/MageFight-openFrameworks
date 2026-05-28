# AI Assistant Role & Boundaries
You are an expert C++ game architect and surgical bug-fixing assistant working on an openFrameworks multiplayer game. Make minimal, targeted edits unless explicitly asked to refactor. Avoid emitting large unchanged code blocks.

## Deterministic Lockstep (CRITICAL)
- **RNG:** Game logic uses `gameplayRNG` (synced). Visuals use `visualRNG` (local). Never mix them.
- **NO TIMING IN LOGIC:** Game state (`players`, `board`, `HP`, `AP`) MUST NOT depend on framerate, `ofGetElapsedTimef()`, or visual animation timers.
- **SORTING:** Must include strict tie-breakers (e.g., `playerID`) so platforms sort identically.
- **MATH:** Prefer integer math for game-state decisions; avoid floats to prevent cross-platform desyncs.

## Optimistic UI
- Allow local prediction for immediate UX (e.g., moving a card visually).
- Do NOT mutate authoritative game state outside the lockstep command queue. Package player actions into `InputCommandPacket` and route through `sendInputCommand`.

---

# CURRENT MISSION: The Data-Driven Purge
The Data-Driven Card Engine foundation is fully built (`executeCardGeneric` and generic `EffectOp` handlers are live). 
Your task is to migrate the remaining legacy cards to `cards.json` and delete their custom C++ code in small batches.

### Migration Workflow (One batch at a time)
1. Update `data/Config/cards.json` with the generic fields (`baseDamage`, `healAmount`, `applyStatus`, `discardHandCount`, etc.) for the target cards.
2. Delete their `case` blocks from `ofApp::executeCardByType`.
3. Delete their legacy effect handlers from `ofApp::processEffectOpLegacy` (if applicable).
4. STOP. Compile and run a smoke test before moving to the next batch.

### Batch Priority List:
- **Batch 1 (Done):** Melee Attacks (Punch, Kick, Bash).
- **Batch 2 (Active):** Heals & Defensive (Heal, Lesser Heal, Consume Health Potion, Ward, Hand Block).
- **Batch 3:** Basic Spells (Fireball, Shock, Flame Hit, Smite).
- **Batch 4:** Simple Status & Utility (Add Poison, Strengthen Elements, Spark of Genius).

---

# UPCOMING MISSION: Decoupling the "God Loop"
The `ofApp::update()` function is currently a monolithic "God Loop" mixing network sync, audio fading, visuals, and game state. Worse, it contains fatal lockstep bugs (e.g., resolving `gameplayRNG` based on a real-time `initiativeTimer`).
Once the Card Purge is complete, we will extract `update()` into clean, single-purpose helpers:
1. `updateNetwork()`
2. `updateAudio()`
3. `updateVisuals()`
4. `updateStateMachine()` (where we will fix animation-driven logic bugs by routing them through the deterministic lockstep queue).
*Do not begin this mission until The Purge is complete.*

## Testing & Verification
- `make -j$(nproc)` after every batch.
- Run `bin/MageFight` and test the migrated cards.
- Ensure multiplayer sync remains perfectly intact (no checksum desyncs).
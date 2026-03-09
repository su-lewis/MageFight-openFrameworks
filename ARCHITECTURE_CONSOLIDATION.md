# Architecture Consolidation Guide: From Hybrid to Unified Card Interactions

**Status**: Phase 1 Complete ✅ | Phases 2-3 Ready for Implementation  
**Commit**: Post-refactor checkpoint with unified state machine foundation

---

## Overview

This document tracks the migration from a **hybrid architecture** (scattered per-card flags and handlers) to a **unified, state-machine-driven architecture** for all card interactions.

### Problem Statement (Before Phase 1)

- **20+ per-card state flags**: `isTargetingBurst`, `isBurstMenuOpen`, `isDispelTargeting`, etc.
- **Fragmented handlers** across 3 sections of input logic:
  - Menu interaction phase (lines 12230-13020)
  - Targeting click phase (lines 12230-13020 + 13092+)
  - Drag-to-play phase (lines 14690-15030)
- **Per-card draw functions** called conditionally in main draw loop (lines 9351-9836)
- **Scaling issues**: Adding a new card with targeting/menu requires:
  - 2 new bool flags in header
  - 2-3 input handler blocks
  - 1 new draw call
  - Manual consistency checks

### Solution (Unified Architecture)

Single, centralized `CardInteractionState` enum with uniform handling:

```cpp
enum CardInteractionState {
    CARD_INTERACTION_IDLE,       // No card interaction active
    CARD_INTERACTION_TARGETING,  // Waiting for target click on board
    CARD_INTERACTION_MENU,       // Waiting for menu button choice
    CARD_INTERACTION_STATUS,     // Waiting for status selection (Dispel)
    CARD_INTERACTION_PLACING     // Waiting for placement click (Wolf/Kobold)
};

// Single set of state variables per interaction (not per-card)
CardInteractionState cardInteractionState;
int interactingCardIndex;        // -1 if idle
int interactingCardType;         // CARD_NONE if idle
int interactionTargetIndex;      // -1 if not targeting/menu
std::string interactionMenuChoice; // "" if menu not chosen
bool interactionNeedsStatusSelect; // For Dispel status phase
```

---

## Phase 1: Foundation ✅ (COMPLETE)

### Completed in This Commit

1. **Added CardInteractionState enum** (`src/ofApp.h` lines 197-203)
   - 5 states covering all card interaction phases
   - Clear semantic meaning vs. per-card bool flags

2. **Added unified state variables** (`src/ofApp.h` lines 1458-1464)
   - `cardInteractionState`
   - `interactingCardIndex`, `interactingCardType`
   - `interactionTargetIndex`
   - `interactionMenuChoice`
   - `interactionNeedsStatusSelect`

3. **Implemented state management functions** (`src/ofApp.cpp` lines 16351-16422)
   - `updateCardInteractionState()` - Transitions state and caches card info
   - `resetCardInteraction()` - Returns to IDLE and clears all interaction flags
   - `handleCardDragToPlay()` - Placeholder for Phase 2
   - `handleCardTargetClick()` - Placeholder for Phase 2
   - `handleCardMenuClick()` - Placeholder for Phase 2
   - `drawActiveCardInteractionUI()` - Placeholder for Phase 2

4. **Refactored cancelAllTargeting()** (`src/ofApp.cpp` lines 16459-16483)
   - Now calls `resetCardInteraction()` to avoid duplication
   - Legacy flags still reset for safety during transition

5. **Verified compilation** - Clean build with no new errors

### Key Design Decisions

- **Backward compatibility**: All legacy per-card flags remain untouched
- **Gradual migration**: New code can use unified state; old code continues working
- **Safety**: `resetCardInteraction()` resets both new AND legacy flags
- **Logging**: Each state transition logged for debugging

---

## Phase 2: Consolidate Input Handlers (READY)

### Scope

Merge **790 lines** of per-card menu/targeting handlers (lines 12230-13020) into `handleCardMenuClick()` and `handleCardTargetClick()`.

### Cards Affected

**Menu-Choice Cards** (After target click, show menu):
- Burst of Light (damage/heal choice)
- Wisdom Boon (damage/self-heal choice)
- Double-Handed (Punch/Block choice)
- Dispel (Barrier/Purge + status selection)
- Giant Magic Hand (Push/Pull choice)

**Auto-Targeting Cards** (Click target, immediately execute):
- Heal / Lesser Heal (target click → execute immediately)
- Death (target click → execute immediately)
- Chain Lightning (target click → execute immediately)

**Special Cards** (Custom interaction flow):
- Amnesia (target self/adjacent → dice roll)
- Teleport (dice roll first → target click)
- Tortoise Form Shell Spike (adjacent target damage)
- Renewed Inspiration (hand card multi-selection)

### Implementation Strategy

```cpp
void ofApp::handleCardTargetClick(int gridX, int gridY) {
    if (cardInteractionState != CARD_INTERACTION_TARGETING) return;
    
    // Find target at grid position
    int targetIndex = findPlayerAtPosition(gridX, gridY);
    if (targetIndex == -1) return;
    
    interactionTargetIndex = targetIndex;
    
    // Dispatch based on card type
    switch (interactingCardType) {
        case CARD_BURST_OF_LIGHT:
            // Transition to MENU state; draw will show Burst menu
            updateCardInteractionState(CARD_INTERACTION_MENU, 
                interactingCardIndex, CARD_BURST_OF_LIGHT);
            break;
        
        case CARD_HEAL:
        case CARD_LESSER_HEAL:
            // Auto-execute immediately after target
            executeHealCard(interactingCardIndex, targetIndex);
            resetCardInteraction();
            break;
        
        // ... other cards ...
    }
}

void ofApp::handleCardMenuClick(const std::string & buttonId) {
    if (cardInteractionState != CARD_INTERACTION_MENU) return;
    
    interactionMenuChoice = buttonId;
    
    // Dispatch based on card type + button ID
    switch (interactingCardType) {
        case CARD_BURST_OF_LIGHT:
            if (buttonId == "damage") {
                executeBurstDamage(interactingCardIndex, interactionTargetIndex);
            } else if (buttonId == "heal") {
                executeBurstHeal(interactingCardIndex, interactionTargetIndex);
            }
            resetCardInteraction();
            break;
        
        // ... other cards ...
    }
}
```

### Old Code Location → New Consolidation

| Cards | Old Lines | New Home |
|-------|-----------|----------|
| Dispel | 12230-12339 | `handleCardTargetClick(CARD_DISPEL)` → status select |
| Train | 12341-12376 | Execute directly (no targeting) |
| Wisdom Boon | 12378-12438 | `handleCardMenuClick(CARD_WISDOM_BOON)` |
| Burst | 12440-12506 | `handleCardMenuClick(CARD_BURST_OF_LIGHT)` |
| Double-Handed | 12508-12606 | `handleCardMenuClick(CARD_DOUBLE_HANDED)` |
| Magic Hand | 12608-12639 | `handleCardMenuClick(CARD_GIANT_MAGIC_HAND)` |
| Amnesia | 12641-12730 | `handleCardTargetClick(CARD_AMNESIA)` |
| Renewed Inspiration | 12732-13019 | Keep separate (complex multi-select) |
| Tortoise Spike | 13021-13090 | `handleCardTargetClick(CARD_TORTOISE_FORM)` |
| Amnesia Targeting | 13092-13125 | Merge with Amnesia menu handler |
| Teleport Targeting | 13127-13200+ | Keep separate (post-dice targeting) |

---

## Phase 3: Consolidate Draw Functions (READY)

### Scope

Unify **~30 draw calls** across lines 9351-9836 into single `drawActiveCardInteractionUI()`.

### Current Fragmented Approach

```cpp
// Lines 9351-9836
if (isDispelMenuOpen || isDispelTargeting || isDispelStatusSelectOpen) drawDispelUI();
if (isWisdomBoonMenuOpen) drawWisdomBoonUI();
if (isDoubleHandedMenuOpen) drawDoubleHandedUI();
if (isAmnesiaMenuOpen) drawAmnesiaMenuUI();
if (isBurstMenuOpen) drawBurstUI();
if (isTargetingBurst) drawInstructionText("Choose a target");
// ... many more per-card draws ...
```

### Unified Approach

```cpp
void ofApp::drawActiveCardInteractionUI() {
    if (cardInteractionState == CARD_INTERACTION_IDLE) return;
    
    switch (interactingCardType) {
        case CARD_BURST_OF_LIGHT:
            if (cardInteractionState == CARD_INTERACTION_TARGETING) {
                drawInstructionText("Choose a target");
            } else if (cardInteractionState == CARD_INTERACTION_MENU) {
                drawBurstMenuUI();
            }
            break;
        
        case CARD_DOUBLE_HANDED:
            if (cardInteractionState == CARD_INTERACTION_TARGETING) {
                drawInstructionText("Choose self or adjacent unit");
            } else if (cardInteractionState == CARD_INTERACTION_MENU) {
                drawDoubleHandedMenuUI();
            }
            break;
        
        case CARD_DISPEL:
            if (cardInteractionState == CARD_INTERACTION_TARGETING) {
                drawInstructionText("Choose self or adjacent unit");
            } else if (cardInteractionState == CARD_INTERACTION_MENU) {
                drawDispelMenuUI();
            } else if (cardInteractionState == CARD_INTERACTION_STATUS) {
                drawDispelStatusSelectionUI();
            }
            break;
        
        // ... other cards ...
    }
    
    // Draw target highlights if in targeting phase
    if (cardInteractionState == CARD_INTERACTION_TARGETING) {
        drawTargetHighlights();
    }
}

// In main draw loop:
drawActiveCardInteractionUI();  // Replaces all per-card draw calls
```

### Integration Points

1. **Main draw loop** (`draw()` function):
   - Remove ~20 individual `if (is*MenuOpen)` checks
   - Call single `drawActiveCardInteractionUI()`

2. **Target highlight system**:
   - Still uses `calculateTargetHighlights(interactingCardIndex)`
   - Draw phase unified, highlight calculation remains modular

3. **Tooltip system**:
   - Move tooltip for active card interaction to unified draw
   - Keep per-card tooltip logic if needed

---

## Phase 4: Consolidate Drag-to-Play Handlers (READY)

### Scope

Merge **340 lines** of per-card drag handlers (lines 14690-15030) into `handleCardDragToPlay()`.

### Current Fragmented Approach

```cpp
// Lines 14690-15030
if (playedCard.type == CARD_MAGIC_BOLT) {
    isTargetingMagicBolt = true;
    magicBoltCardIndex = draggedCardIndex;
    // ...
} else if (playedCard.type == CARD_DOUBLE_HANDED) {
    // ... different setup logic ...
} else if (playedCard.type == CARD_AMNESIA) {
    // ... yet another different setup ...
} 
// ... 20+ cards with unique per-card logic ...
```

### Unified Approach

```cpp
void ofApp::handleCardDragToPlay(int cardIndex) {
    if (cardIndex < 0 || cardIndex >= (int)players[currentPlayerIndex].hand.size()) return;
    
    Card & card = players[currentPlayerIndex].hand[cardIndex];
    Player & caster = players[currentPlayerIndex];
    
    // Validate AP cost
    if (currentAP < card.cost) {
        spawnFloatingText(gridToWorld(caster.x, caster.y), "Not enough AP", ofColor::red);
        return;
    }
    
    // Dispatch initialization based on card type
    switch (card.type) {
        case CARD_BURST_OF_LIGHT:
        case CARD_MAGIC_BOLT:
        case CARD_DEATH:
        case CARD_CHAIN_LIGHTNING:
        case CARD_HEAL:
        case CARD_LESSER_HEAL:
            // These cards enter TARGETING state
            updateCardInteractionState(CARD_INTERACTION_TARGETING, cardIndex, card.type);
            calculateTargetHighlights(cardIndex);
            break;
        
        case CARD_DOUBLE_HANDED:
        case CARD_AMNESIA:
        case CARD_DISPEL:
            // These cards check for adjacent units
            if (!hasAdjacentUnit(caster)) {
                // Auto-target self → Menu
                updateCardInteractionState(CARD_INTERACTION_MENU, cardIndex, card.type);
                interactionTargetIndex = currentPlayerIndex;
            } else {
                // Must choose target first
                updateCardInteractionState(CARD_INTERACTION_TARGETING, cardIndex, card.type);
                calculateTargetHighlights(cardIndex);
            }
            break;
        
        case CARD_TELEPORT:
            // Special: Dice roll before targeting
            updateCardInteractionState(CARD_INTERACTION_TARGETING, cardIndex, card.type);
            int rollResult = startDiceRoll(card.numDice, card.diceSides, PURPOSE_RANGE, "Teleport: Range");
            interactionDiceRoll = rollResult;
            break;
        
        case CARD_TRAIN:
        case CARD_BLOCKING_BOON:
        case CARD_WISDOM_BOON:
            if (!hasAdjacentUnit(caster)) {
                // Auto-execute on self
                executeCard(cardIndex, caster.x, caster.y);
                resetCardInteraction();
            } else {
                // Must target adjacent
                updateCardInteractionState(CARD_INTERACTION_TARGETING, cardIndex, card.type);
                calculateTargetHighlights(cardIndex);
            }
            break;
        
        default:
            // General fallback for TARGET_NON_SELF cards
            updateCardInteractionState(CARD_INTERACTION_TARGETING, cardIndex, card.type);
            calculateTargetHighlights(cardIndex);
            break;
    }
    
    draggedCardIndex = -1;
    selectedCardIndex = -1;
}
```

### Interaction Flow Patterns (Identified from Old Code)

1. **Simple Target → Execute** (TARGETING → execute immediately)
   - Heal, Lesser Heal, Death, Chain Lightning, Flail
   - Pattern: Click target → apply effect → done

2. **Target → Menu Choice** (TARGETING → MENU)
   - Burst of Light, Wisdom Boon, Double-Handed
   - Pattern: Click target → show menu → choose option → execute

3. **Target → Status Select** (TARGETING → STATUS)
   - Dispel
   - Pattern: Click target → show Barrier/Purge menu → if Purge, show status list → choose → execute

4. **Dice Roll First** (DICE → TARGETING → execute)
   - Teleport
   - Pattern: Roll range → click destination within range → execute

5. **Auto-Self or Target**
   - Amnesia, Double-Handed, Dispel
   - Pattern: If no adjacent units → auto-target self; else → enter targeting

6. **Direct Execute** (No interaction)
   - Train, Blocking Boon (if auto-self)
   - Pattern: Drag → validate → execute immediately

---

## Implementation Checklist

- [ ] **Phase 2: Menu/Target Handlers**
  - [ ] Implement full `handleCardTargetClick()` logic
  - [ ] Implement full `handleCardMenuClick()` logic
  - [ ] Replace all lines 12230-13020 with centralized dispatcher
  - [ ] Test each card type individually

- [ ] **Phase 3: Draw Functions**
  - [ ] Implement `drawActiveCardInteractionUI()` fully
  - [ ] Replace all draw calls in lines 9351-9836
  - [ ] Test menu and targeting UI rendering
  - [ ] Verify instruction text displays correctly

- [ ] **Phase 4: Drag-to-Play**
  - [ ] Implement full `handleCardDragToPlay()` logic
  - [ ] Replace all lines 14690-15030
  - [ ] Test each card's drag interaction
  - [ ] Verify state transitions

- [ ] **Phase 5: Cleanup & Deprecation**
  - [ ] Remove old per-card bool flags from header
  - [ ] Update comment documentation
  - [ ] Remove legacy handler code
  - [ ] Final comprehensive test

---

## Legacy Flag Deprecation Map

Once phases 2-4 are complete, these per-card flags can be safely removed:

### Targeting Flags (Remove after Phase 4)
```cpp
bool isTargetingDeath = false;         // → cardInteractionState
bool isTargetingHeal = false;          // → cardInteractionState
bool isTargetingPunch = false;         // → cardInteractionState
bool isTargetingChainLightning = false;// → cardInteractionState
bool isTargetingTeleport = false;      // → cardInteractionState + interactionDiceRoll
bool isTargetingBurst = false;         // → cardInteractionState
bool isTargetingDoubleHanded = false;  // → cardInteractionState
bool isTargetingAmnesia = false;       // → cardInteractionState
bool isTargetingTortoiseDamage = false;// → cardInteractionState
bool isTargetingMagicBolt = false;     // → cardInteractionState
```

### Menu Flags (Remove after Phase 2/3)
```cpp
bool isDispelMenuOpen = false;         // → cardInteractionState
bool isDispelTargeting = false;        // → cardInteractionState
bool isDispelStatusSelectOpen = false; // → cardInteractionState
bool isWisdomBoonMenuOpen = false;     // → cardInteractionState
bool isBurstMenuOpen = false;          // → cardInteractionState
bool isDoubleHandedMenuOpen = false;   // → cardInteractionState
bool isAmnesiaMenuOpen = false;        // → cardInteractionState
```

### Pending Indices (Remove after phases 2-4)
```cpp
int magicBoltCardIndex = -1;           // → interactingCardIndex
int pendingBurstCardIndex = -1;        // → interactingCardIndex
int deathCardIndex = -1;               // → interactingCardIndex
int healCardIndex = -1;                // → interactingCardIndex
int chainLightningCardIndex = -1;      // → interactingCardIndex
int pendingTeleportCardIndex = -1;     // → interactingCardIndex
int pendingDoubleHandedCardIndex = -1; // → interactingCardIndex
int pendingAmnesiaCardIndex = -1;      // → interactingCardIndex
int hellhoundCardIndex = -1;           // → interactingCardIndex
```

### Pending Targets/Choices (Remove after phases 2-3)
```cpp
int pendingBurstTargetIndex = -1;      // → interactionTargetIndex
int burstChoice = -1;                  // → interactionMenuChoice ("damage"/"heal")
int pendingDispelTargetIndex = -1;     // → interactionTargetIndex
std::string pendingDoubleHandedChoice; // → interactionMenuChoice ("Punch"/"Block")
```

---

## Testing Strategy

### Per-Phase Validation

**Phase 2 (Menu/Target)**:
- [ ] Burst of Light: Target → Menu → Choose → Execute
- [ ] Wisdom Boon: Target → Menu → Choose → Execute
- [ ] Double-Handed: Auto-self or Target → Menu → Execute
- [ ] Dispel: Target → Menu → Status Select → Execute
- [ ] Heal: Target → Auto-Execute

**Phase 3 (Draw)**:
- [ ] Menu UI renders in correct position
- [ ] Instruction text displays for each targeting phase
- [ ] Target highlights appear/disappear correctly
- [ ] No duplicate rendering (old + new code)

**Phase 4 (Drag)**:
- [ ] Drag card → Enter correct state
- [ ] Drag adjacency check works (auto-self)
- [ ] Drag dice roll (Teleport) works
- [ ] No state conflicts during drag-to-play

**Phase 5 (Cleanup)**:
- [ ] Full game loop test (all cards playable)
- [ ] Multiplayer packet sending still works
- [ ] No console warnings about legacy flags
- [ ] Performance unchanged

---

## Future Improvements

1. **Card Metadata System**
   - Define interaction behavior in Card struct instead of switch statements
   - Reduces code duplication for similar card types

2. **Reusable Menu/Targeting Templates**
   - Generic "menu choose" behavior for multi-choice cards
   - Generic "target click" behavior for single-target cards

3. **Macro System for Common Patterns**
   - Template for cards with adjacent-only targeting
   - Template for cards with auto-self-if-no-adjacent

4. **State Machine Visualization**
   - Debug UI showing current interaction state
   - Trace state transitions for each card

---

## References

- **Phase 1 commit**: Unified enum + state vars + manager functions
- **Legacy code location**: Lines 9351-9836 (draw), 12230-13020 (menu/target), 14690-15030 (drag)
- **Header additions**: ofApp.h lines 197-203 (enum), 1458-1464 (vars), 1857-1862 (functions)
- **Implementation**: ofApp.cpp lines 16351-16422 (manager functions)

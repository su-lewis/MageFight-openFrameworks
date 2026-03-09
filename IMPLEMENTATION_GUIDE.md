# Implementation Guide: Phases 2-4 (Ready for Execution)

This guide provides exact code locations, patterns, and refactoring steps for completely unifying the card interaction system.

---

## Phase 2: Consolidate Input Handlers (1130 LOC → ~300 LOC)

### Target: Merge lines 12230-13020 and 14690-15030 into 4 centralized functions

#### Step 1: Analyze Card Patterns

**Pattern A: Target → Auto-Execute** (1 target click = immediate effect)
```
Cards: Heal, Lesser Heal, Death, Chain Lightning
Flow: Enter TARGETING → Click target → Execute → Reset

Current Implementation: ofApp.cpp lines 14884-14895 (Heal/Lesser Heal)
Old Code Pattern:
  if (playedCard.type == CARD_HEAL || playedCard.type == CARD_LESSER_HEAL) {
      isTargetingHeal = true;
      healCardIndex = draggedCardIndex;
      calculateTargetHighlights(healCardIndex);
      return;
  }
```

**Pattern B: Target → Menu Choice** (1 target click → show menu → choose option → execute)
```
Cards: Burst of Light, Wisdom Boon, Double-Handed
Flow: Enter TARGETING → Click target → Enter MENU → Choose option → Execute → Reset

Current Implementation: 
  ofApp.cpp lines 14843-14863 (Burst targeting trigger)
  ofApp.cpp lines 12440-12506 (Burst menu handler)
Old Code Pattern:
  if (playedCard.type == CARD_BURST_OF_LIGHT) {
      pendingBurstCardIndex = draggedCardIndex;
      isTargetingBurst = true;
      calculateTargetHighlights(pendingBurstCardIndex);
  }
  // Then in menu phase:
  if (isBurstMenuOpen && button == OF_MOUSE_BUTTON_LEFT) {
      if (burstBtnDamage.inside(x, y) || burstBtnHeal.inside(x, y)) {
          // Execute based on choice
      }
  }
```

**Pattern C: Target → Status Selection** (1 target → menu → choose status → execute)
```
Cards: Dispel
Flow: Enter TARGETING → Click target → Enter MENU → Choose mode → If Purge, STATUS → Choose status → Execute

Current Implementation:
  ofApp.cpp lines 14928-14964 (Dispel trigger)
  ofApp.cpp lines 12230-12339 (Dispel menu + status)
```

**Pattern D: Auto-Self or Target** (No adjacent = auto-self; else = target selection)
```
Cards: Amnesia, Double-Handed, Dispel
Flow: Drag → Check adjacent → If none, auto-self→menu; else TARGETING

Current Implementation:
  ofApp.cpp lines 14770-14790 (Double-Handed)
  ofApp.cpp lines 12508-12606 (Double-Handed menu/targeting handler)
```

#### Step 2: Implement handleCardDragToPlay()

**Location**: Replace lines 14690-15030 with centralized dispatch

**Template**:
```cpp
void ofApp::handleCardDragToPlay(int cardIndex) {
    if (cardIndex < 0 || cardIndex >= (int)players[currentPlayerIndex].hand.size()) {
        draggedCardIndex = -1;
        return;
    }
    
    Card & card = players[currentPlayerIndex].hand[cardIndex];
    Player & caster = players[currentPlayerIndex];
    
    // Validate AP
    if (currentAP < card.cost) {
        spawnFloatingText(gridToWorld(caster.x, caster.y), "Insufficient AP", ofColor::red);
        draggedCardIndex = -1;
        return;
    }
    
    // Clear previous interaction
    resetCardInteraction();
    
    // Card-type dispatcher
    switch (card.type) {
        
        // ===== PATTERN A: Target → Auto-Execute =====
        case CARD_HEAL:
        case CARD_LESSER_HEAL:
        case CARD_DEATH:
        case CARD_CHAIN_LIGHTNING:
            // Validate non-self targeting if required
            if (card.type == CARD_DEATH && !hasValidNonSelfTargetForHandIndex(cardIndex)) {
                spawnFloatingText(gridToWorld(caster.x, caster.y), "No valid targets", ofColor::red);
                return;
            }
            updateCardInteractionState(CARD_INTERACTION_TARGETING, cardIndex, card.type);
            calculateTargetHighlights(cardIndex);
            break;
        
        // ===== PATTERN B: Target → Menu =====
        case CARD_BURST_OF_LIGHT:
        case CARD_WISDOM_BOON:
            updateCardInteractionState(CARD_INTERACTION_TARGETING, cardIndex, card.type);
            calculateTargetHighlights(cardIndex);
            break;
        
        // ===== PATTERN D: Auto-Self or Target =====
        case CARD_DOUBLE_HANDED:
        case CARD_AMNESIA:
            if (!hasAdjacentUnit(caster)) {
                // No adjacent unit: auto-self → menu
                updateCardInteractionState(CARD_INTERACTION_MENU, cardIndex, card.type);
                interactionTargetIndex = currentPlayerIndex;
                // Will draw menu immediately
            } else {
                // Has adjacent: must target first
                updateCardInteractionState(CARD_INTERACTION_TARGETING, cardIndex, card.type);
                calculateTargetHighlights(cardIndex);
            }
            break;
        
        case CARD_DISPEL:
            if (!hasAdjacentUnit(caster)) {
                updateCardInteractionState(CARD_INTERACTION_MENU, cardIndex, card.type);
                interactionTargetIndex = currentPlayerIndex;
            } else {
                updateCardInteractionState(CARD_INTERACTION_TARGETING, cardIndex, card.type);
                calculateTargetHighlights(cardIndex);
            }
            break;
        
        // ===== TELEPORT: Special (dice first) =====
        case CARD_TELEPORT:
            int rollResult = startDiceRoll(card.numDice, card.diceSides, PURPOSE_RANGE, "Teleport: Range");
            updateCardInteractionState(CARD_INTERACTION_TARGETING, cardIndex, card.type);
            interactionDiceRoll = rollResult;
            currentAP -= card.cost;
            caster.playedCardsPile.push_back(card);
            updatePlayerAP(caster, currentAP);
            break;
        
        // ===== MAGIC BOLT: Non-self targeting =====
        case CARD_MAGIC_BOLT:
        case CARD_CHAIN_LIGHTNING:
            if (!hasValidNonSelfTargetForHandIndex(cardIndex)) {
                spawnFloatingText(gridToWorld(caster.x, caster.y), "No valid non-self targets", ofColor::red);
                return;
            }
            updateCardInteractionState(CARD_INTERACTION_TARGETING, cardIndex, card.type);
            calculateTargetHighlights(cardIndex);
            break;
        
        // ===== DEFAULT: General non-self targeting =====
        default:
            if (card.targeting != TARGET_SELF) {
                updateCardInteractionState(CARD_INTERACTION_TARGETING, cardIndex, card.type);
                calculateTargetHighlights(cardIndex);
            }
            break;
    }
    
    draggedCardIndex = -1;
    selectedCardIndex = -1;
}
```

#### Step 3: Implement handleCardTargetClick()

**Location**: Extract from lines 12230-13020 + 14690-15030

**Template**:
```cpp
void ofApp::handleCardTargetClick(int gridX, int gridY) {
    if (cardInteractionState != CARD_INTERACTION_TARGETING) return;
    if (gridX < 0 || gridX >= BOARD_WIDTH || gridY < 0 || gridY >= BOARD_HEIGHT) return;
    
    // Find target at position
    int targetIndex = -1;
    for (size_t i = 0; i < players.size(); i++) {
        if (players[i].x == gridX && players[i].y == gridY) {
            targetIndex = (int)i;
            break;
        }
    }
    
    if (targetIndex == -1 || !board[gridX][gridY].isTargetable) return;
    
    Player & caster = players[currentPlayerIndex];
    Card & card = caster.hand[interactingCardIndex];
    interactionTargetIndex = targetIndex;
    
    // Card-type dispatcher
    switch (interactingCardType) {
        
        // ===== PATTERN A: Target → Auto-Execute =====
        case CARD_HEAL:
        case CARD_LESSER_HEAL: {
            Player & target = players[targetIndex];
            if (target.health >= target.maxHealth) {
                spawnFloatingText(gridToWorld(target.x, target.y), "Already Full HP", ofColor::gray);
                return;
            }
            executeCardByType(interactingCardIndex, interactingCardType, targetIndex);
            resetCardInteraction();
            break;
        }
        
        case CARD_DEATH:
        case CARD_CHAIN_LIGHTNING:
            executeCardByType(interactingCardIndex, interactingCardType, targetIndex);
            resetCardInteraction();
            break;
        
        // ===== PATTERN B: Target → Menu =====
        case CARD_BURST_OF_LIGHT:
        case CARD_WISDOM_BOON:
            // Transition to MENU state; will draw menu next frame
            updateCardInteractionState(CARD_INTERACTION_MENU, interactingCardIndex, interactingCardType);
            break;
        
        // ===== PATTERN D: Auto-Self or Target =====
        case CARD_DOUBLE_HANDED:
        case CARD_AMNESIA:
            // Already in target; transition to menu
            updateCardInteractionState(CARD_INTERACTION_MENU, interactingCardIndex, interactingCardType);
            break;
        
        case CARD_DISPEL:
            // Show menu with Barrier/Purge options
            updateCardInteractionState(CARD_INTERACTION_MENU, interactingCardIndex, interactingCardType);
            break;
        
        // ===== TELEPORT: Click destination within range =====
        case CARD_TELEPORT:
            if (board[gridX][gridY].isTargetable) {
                caster.x = gridX;
                caster.y = gridY;
                playerVisualPos = gridToWorld(gridX, gridY);
                spawnFloatingText(gridToWorld(gridX, gridY), "Teleport!", ofColor::cyan);
                // Remove card and handle key check
                finishPlayCard(caster, card, interactingCardIndex);
                resetCardInteraction();
            }
            break;
    }
}
```

#### Step 4: Implement handleCardMenuClick()

**Location**: Extract from lines 12230-13020

**Template**:
```cpp
void ofApp::handleCardMenuClick(const std::string & buttonId) {
    if (cardInteractionState != CARD_INTERACTION_MENU && 
        cardInteractionState != CARD_INTERACTION_STATUS) return;
    
    Player & caster = players[currentPlayerIndex];
    Player & target = players[interactionTargetIndex];
    Card & card = caster.hand[interactingCardIndex];
    
    // Card-type dispatcher
    switch (interactingCardType) {
        
        case CARD_BURST_OF_LIGHT: {
            if (buttonId == "damage") {
                applyDamageTo(target, 3, DAMAGE_HOLY, currentPlayerIndex);
            } else if (buttonId == "heal") {
                if (target.health >= target.maxHealth) {
                    spawnFloatingText(gridToWorld(target.x, target.y), "Full HP", ofColor::gray);
                } else {
                    int healAmt = std::min(3, target.maxHealth - target.health);
                    target.health += healAmt;
                    spawnFloatingText(gridToWorld(target.x, target.y), "+" + ofToString(healAmt) + " HP", ofColor::green);
                }
            }
            finishPlayCard(caster, card, interactingCardIndex);
            resetCardInteraction();
            break;
        }
        
        case CARD_DOUBLE_HANDED: {
            if (buttonId == "Punch") {
                // Add damage bonus
            } else if (buttonId == "Block") {
                // Add block bonus
            }
            finishPlayCard(caster, card, interactingCardIndex);
            resetCardInteraction();
            break;
        }
        
        case CARD_DISPEL: {
            if (buttonId == "Barrier") {
                // Roll barrier dice
            } else if (buttonId == "Purge") {
                // Enter status selection
                updateCardInteractionState(CARD_INTERACTION_STATUS, interactingCardIndex, interactingCardType);
                // Will show status selection UI next frame
            }
            break;
        }
        
        // ... other cards ...
    }
}
```

---

## Phase 3: Consolidate Draw Functions (20+ calls → 1 unified function)

### Target: Merge lines 9351-9836 into drawActiveCardInteractionUI()

**Step 1: Replace draw calls**

Old code (lines 9351-9836):
```cpp
if (isDispelMenuOpen || isDispelTargeting || isDispelStatusSelectOpen) {
    drawDispelUI();
}
if (isWisdomBoonMenuOpen) {
    drawWisdomBoonUI();
}
// ... 20+ more if-blocks ...
```

New code:
```cpp
// In main draw() function, replace all above with:
drawActiveCardInteractionUI();
```

**Step 2: Implement drawActiveCardInteractionUI()**

**Location**: Lines 16423-16500 (replace placeholder)

**Template**:
```cpp
void ofApp::drawActiveCardInteractionUI() {
    if (cardInteractionState == CARD_INTERACTION_IDLE) return;
    
    // Draw instruction text for targeting phase
    if (cardInteractionState == CARD_INTERACTION_TARGETING) {
        switch (interactingCardType) {
            case CARD_BURST_OF_LIGHT:
                drawInstructionText("Choose a target");
                break;
            case CARD_DOUBLE_HANDED:
                drawInstructionText("Choose self or adjacent target");
                break;
            case CARD_DISPEL:
                drawInstructionText("Choose self or adjacent target");
                break;
            case CARD_TELEPORT:
                drawInstructionText("Choose destination (Range: " + ofToString(interactionDiceRoll) + " ft)");
                break;
            // ... other cards ...
        }
        return;
    }
    
    // Draw menu UI for menu phase
    if (cardInteractionState == CARD_INTERACTION_MENU) {
        switch (interactingCardType) {
            case CARD_BURST_OF_LIGHT:
                drawBurstMenuUI();
                break;
            case CARD_WISDOM_BOON:
                drawWisdomBoonMenuUI();
                break;
            case CARD_DOUBLE_HANDED:
                drawDoubleHandedMenuUI();
                break;
            case CARD_DISPEL:
                drawDispelMenuUI();
                break;
            // ... other cards ...
        }
        return;
    }
    
    // Draw status selection UI for status phase (Dispel only)
    if (cardInteractionState == CARD_INTERACTION_STATUS) {
        switch (interactingCardType) {
            case CARD_DISPEL:
                drawDispelStatusSelectionUI();
                break;
            // ... other status-select cards ...
        }
        return;
    }
    
    // Draw placement UI for placement phase (Wolf/Kobold)
    if (cardInteractionState == CARD_INTERACTION_PLACING) {
        // Draw placement instructions and preview
    }
}
```

---

## Phase 4: Consolidate Drag-to-Play Handlers

### Already covered in Phase 2
The handleCardDragToPlay() implementation consolidates all 340 lines from 14690-15030.

---

## Validation Checklist

- [ ] Phase 2 compiles without errors
- [ ] Test each Pattern A card (Heal, Death, Chain Lightning)
- [ ] Test each Pattern B card (Burst, Wisdom Boon)
- [ ] Test each Pattern D card (Amnesia, Double-Handed, Dispel)
- [ ] Test Teleport (Pattern special)
- [ ] Phase 3 compiles without errors
- [ ] Verify all menus render correctly
- [ ] Verify all instruction text displays
- [ ] Verify target highlights work
- [ ] Verify no console spam from state transitions
- [ ] Test multiplayer: verify packets still sent correctly
- [ ] Run full game loop: multiple cards, multiple turns

---

## Cleanup After All Phases

Once all phases complete:
1. Remove all per-card bool flags from ofApp.h
2. Remove all per-card index variables
3. Remove old input handler code blocks
4. Update inline documentation
5. Final comprehensive test


# Architecture Audit: Hybrid State Management

## Per-Card State Flags (Header Lines 1449-1772)

### Targeting Flags (Active During Targeting Phase)
- `isTargetingDeath` (line 1449)
- `isTargetingHeal` (line 1456)
- `isTargetingPunch` (line 1460)
- `isTargetingChainLightning` (line 1602)
- `isTargetingTeleport` (line 1638)
- `isTargetingBurst` (line 1687)
- `isTargetingDoubleHanded` (line 1707)
- `isTargetingAmnesia` (line 1724)
- `isTargetingTortoiseDamage` (line 1737)
- `isTargetingMagicBolt` (line 1772)

### Menu Flags (Active During Menu Choice Phase)
- `isDispelMenuOpen` (line 1621)
- `isDispelTargeting` (line 1622)
- `isDispelStatusSelectOpen` (line 1623)
- `isWisdomBoonMenuOpen` (line 1676)
- `isBurstMenuOpen` (line 1684)
- `isDoubleHandedMenuOpen` (line 1706)
- `isAmnesiaMenuOpen` (line 1723)

### Special Action Flags
- `isPlacingWolves` (line 1741)
- `isPlacingKobolds` (line 1751)
- `isAmnesiaSelectionActive` (line 1542)

## Input Handlers with Legacy Logic

### Section A: ofApp.cpp:12230-13020 (790 lines)
Per-card menu interaction handlers:
- Dispel menu + targeting + status selection (lines 12230-12339)
- Train menu (lines 12341-12376)
- Wisdom Boon menu (lines 12378-12438)
- Burst of Light menu (lines 12440-12506)
- Double-Handed menu & targeting (lines 12508-12606)
- Giant Magic Hand menu (lines 12608-12639)
- Amnesia menu & targeting (lines 12641-12730)
- Renewed Inspiration selection (lines 12732-13019)
- Tortoise Damage targeting (lines 13021-13090)
- Amnesia targeting (lines 13092-13125)
- Teleport targeting (lines 13127-13200+)

### Section B: ofApp.cpp:14690-15030 (340 lines)
Per-card drag-to-play handlers:
- Magic Bolt targeting trigger (lines 14752-14768)
- Double Handed targeting/menu trigger (lines 14770-14790)
- Amnesia auto-self or targeting (lines 14792-14810)
- Teleport dice roll + play (lines 14812-14841)
- Burst of Light targeting trigger (lines 14843-14863)
- Death targeting trigger (lines 14865-14882)
- Heal/Lesser Heal targeting trigger (lines 14884-14895)
- Chain Lightning targeting trigger (lines 14897-14914)
- Hellhound targeting trigger (lines 14916-14926)
- Dispel auto-self/targeting/menu trigger (lines 14928-14964)
- Blocking Boon auto-self (lines 14966-14977)
- Wisdom Boon auto-self (lines 14979-14990)
- General non-self targeting fallback (lines 14992-15025)

## Draw Function Callers (Lines 9351-9836)
These draw UIs based on legacy flags:
- `drawDispelUI()` when `isDispelMenuOpen || isDispelTargeting || isDispelStatusSelectOpen`
- `drawWisdomBoonUI()` when `isWisdomBoonMenuOpen`
- `drawDoubleHandedUI()` when `isDoubleHandedMenuOpen`
- `drawAmnesiaMenuUI()` when `isAmnesiaMenuOpen`
- `drawBurstUI()` when `isBurstMenuOpen`

## Problem Statement

1. **State Duplication**: Each card has its own `isTargeting*` and `is*MenuOpen` flags
2. **Handler Fragmentation**: Card interactions split across:
   - Menu choice phase (12230-13020)
   - Targeting phase (12230-13020 + 13092+)
   - Drag-to-play phase (14690-15030)
3. **Scaling Issue**: Adding a new card with targeting/menu requires:
   - 2 new bool flags
   - 2+ input handler blocks (menu + targeting + drag trigger)
   - 1+ draw function call
4. **Inconsistency**: Different cards follow different patterns (auto-self, menu-first, target-first)

## Unified Architecture (Target State)

### Central Enum
```cpp
enum CardInteractionState {
    CARD_IDLE,
    CARD_TARGETING,      // Waiting for target click
    CARD_MENU_CHOICE,    // Waiting for menu button click
    CARD_STATUS_SELECT,  // Waiting for status selection (Dispel)
    CARD_DRAG_PENDING    // Card dragged, awaiting next action
};
```

### Unified State Vars
```cpp
CardInteractionState currentCardInteractionState;
int activeCardIndex;           // Which card is being interacted with
int targetedPlayerIndex;       // Chosen target
std::string menuChoice;        // Selected menu option (if any)
bool needsStatusSelection;     // Special: Dispel status selection flag
```

### Consolidated Input Dispatcher
```
mousePressed:
  if (isDragging):
    -> centralizedCardDragHandler(cardIndex)
  if (isClickingBoard):
    -> centralizedTargetClickHandler(gx, gy)
  if (isClickingUI):
    -> centralizedMenuClickHandler(buttonId)
```

### Unified Draw
```cpp
if (currentCardInteractionState != CARD_IDLE) {
    drawCardInteractionUI(activeCard);
}
```


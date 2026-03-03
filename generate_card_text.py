#!/usr/bin/env python3
"""
Generate effect text for all 70 MageFight cards based on game logic
"""

# Card effect texts based on code implementation
card_effects = {
    1: "Deal 2 physical damage to an adjacent unit.",
    2: "Deal 4 physical damage to an adjacent unit.",
    3: "Gain 2 physical block.",
    4: "Roll 2d4 and deal that much physical damage to an adjacent unit.",
    5: "Gain 5 ward (blocks both physical and non-physical damage).",
    6: "Roll 1d6 and deal that much piercing damage to an adjacent unit. Pierce through to a second unit in line if not blocked by a wall.",
    7: "Roll 1d6 and deal that much physical damage in a cleaving arc to up to 3 adjacent units.",
    8: "Steal the top card from an adjacent unit's deck and shuffle it into your deck.",
    9: "Roll 1d4. Target adjacent unit or yourself must remove that many cards from their deck (you choose which).",
    10: "Roll 1d20 for range (max 20ft). If successful, deal 4 arcane damage to the target tile and all units adjacent to it.",
    11: "Roll 2d6 for range (max 12ft). If successful, deal 6 fire damage to the target unit and set them on fire.",
    12: "Deal 2 electric damage to an adjacent unit. If you've played 2+ Shocks this turn, paralyze the target and gain +2 AP next turn.",
    13: "Target an adjacent wall or unit. If wall: destroy it. If unit: roll 2d10 and deal that much physical damage.",
    14: "Choose: Remove all non-physical barriers (Barrier + Holy Block) OR purge one status effect from a target.",
    15: "Roll 3d6 for movement range. Teleport to any empty tile within that distance.",
    16: "Next turn: roll d10 for AP (instead of d6) and draw an extra card.",
    17: "The next card you play this turn is copied back into your hand.",
    18: "Choose a target (adjacent or self): Deal 2 arcane damage OR grant 2 non-physical barrier.",
    19: "Roll 1d20 for range (max 20ft). If successful, deal 3 arcane damage to target (ignores walls).",
    20: "Deal 1 fire damage to an adjacent unit and set them on fire.",
    21: "Roll 2d6 and heal a target in line of sight for that amount.",
    22: "Summon a Skeleton with 1d6 HP on an adjacent empty tile.",
    23: "Summon a Golem with 1d10 HP on an adjacent empty tile. Golem type depends on elements played this turn (Electric/Fire/Rock/Standard).",
    24: "For the next 3 turns, whenever you play a Fire or Electric damage card, a copy is shuffled into your deck.",
    25: "Create a normal wall on an adjacent empty tile.",
    26: "Gain 7 Holy Block. Next turn, gain bonus dice from your minions.",
    27: "Deal 2 physical damage to an adjacent unit (plus +2 per previous hand attack this turn). Heal yourself for the actual HP damage dealt.",
    28: "Choose a target (adjacent or self): Add 2x Punch to their deck OR add 2x Hand Block to their deck.",
    29: "Convert all of target's block types into dice: Physical blocks → coin flips (heads = damage or heal), Non-physical → d20 rolls.",
    30: "Summon 2 Wolves on adjacent empty tiles.",
    31: "Gain +1 Luck for each Skeleton you control.",
    32: "Roll 1d4. Gain that many bonus turns immediately.",
    33: "Deal 2 physical damage to an adjacent unit (damage doubled if Flurry is active).",
    34: "Roll 2d20 for range (max 40ft). If successful, deal 5 arcane damage to target (flies over walls).",
    35: "Roll 1d6 and add 2. Deal that much physical damage to all adjacent units.",
    36: "Roll 2d6 for HP and 2d6 for AP. Summon a Hellhound with those stats on an adjacent empty tile.",
    37: "If target is asleep: instant death. Otherwise, roll 1d20: 1-10 = sleep for 2 turns, 11-19 = no effect, 20 = instant death.",
    38: "Roll 3d10 for HP. Summon a Demon with that much HP on an adjacent empty tile.",
    39: "Convert all your block types (Physical Block + Barrier + Ward + Holy Block) into damage. Deal that much to an adjacent unit and remove all your block.",
    40: "Choose: Gain +2 AP immediately OR draft a Class 2 card.",
    41: "Roll 2d10 for range. Deal electric damage to target and chain to nearby units within range.",
    42: "Draw an extra card next turn and draft a Class 2 card immediately.",
    43: "Gain +1 maximum HP.",
    44: "Your next physical or piercing attack this turn also deals 1d6 poison damage.",
    45: "Deal 2 physical damage (4 if Flurry already active). Activate Flurry buff (doubles hand attack damage). Draw a card (free if hand-related).",
    46: "Gain +5 maximum HP and heal 5 HP. Shuffle 2x Dispel into deck. Whenever you play block/heal/ward, deal 3 damage to an adjacent unit. Breaks if you take 10+ damage.",
    47: "Count connected walls (flood fill). Gain that much Fortification. Deal 3 physical damage to all units adjacent to connected walls.",
    48: "Deal 3 physical damage to an adjacent unit. If HP damage dealt: heal 2 HP and shuffle 1x Vampire Bite into target's deck.",
    49: "Roll 1d4. Summon that many Kobolds on adjacent empty tiles.",
    50: "Destroy an adjacent wall. Gain +6 AP next turn.",
    51: "Discard any number of cards from your hand, then draw that many cards.",
    52: "Roll 1d4 and draw that many cards.",
    53: "Roll 2d20 for range. Deal arcane damage to all units within that distance.",
    54: "Create a magic wall on an adjacent empty tile (blocks non-physical damage).",
    55: "Roll 1d4 for each unit on board. Move them that many tiles in random direction. Units take 3 damage on collision.",
    56: "Gain Regeneration and immunity to damage until you take 5+ damage in one hit (then form breaks).",
    57: "Choose: Push or Pull an adjacent unit through a wall.",
    58: "Gain +3 maximum HP and heal 1 HP.",
    59: "Roll 1d6 and heal a target in line of sight for that amount.",
    60: "Transform an adjacent wall into a unit. Normal wall → 5 HP unit. Magic wall → 7 HP unit.",
    61: "Summon a Kobold King with HP equal to (number of your Kobolds + 1) on an adjacent empty tile.",
    62: "Summon a 1 HP Assistant on an adjacent empty tile. Assistant grants you +1 Luck aura.",
    63: "Draft cards based on your max HP: 16-20 = Class 1, 21-25 = Class 2, 26-30 = Class 3, >30 = instant win.",
    64: "Gain +1 permanent Luck.",
    65: "Gain +2 AP immediately and +2 AP next turn. Kick costs 0 AP for 2 turns.",
    66: "Heal to maximum HP and remove all status effects (Fire, Poison, Paralysis, Sleep).",
    67: "Deal 5 holy damage to an adjacent unit.",
    68: "Choose a target in line of sight: Deal 3 holy damage OR heal 3 HP.",
    69: "Roll 2d20 for range (max 40ft). If successful, roll 1d6 and deal that much piercing damage.",
    70: "Summon a 5 HP Faerie with Regeneration on an adjacent empty tile. Faerie rolls 1d4 for AP each turn."
}

# Read cards.md
with open('/home/lewis/of_workspace/openFrameworks/apps/myApps/MageFight/cards.md', 'r') as f:
    content = f.read()

# Replace each card's effect text
import re

for card_id, effect_text in card_effects.items():
    # Find the card section
    pattern = rf'(## {card_id}\..*?Effect Text:\s*)()(.*?)(---|\Z)'
    
    def replacer(match):
        return f'{match.group(1)}{effect_text}\n{match.group(4)}'
    
    content = re.sub(pattern, replacer, content, flags=re.DOTALL)

# Write back
with open('/home/lewis/of_workspace/openFrameworks/apps/myApps/MageFight/cards.md', 'w') as f:
    f.write(content)

print("Successfully updated all 70 card effect texts in cards.md")

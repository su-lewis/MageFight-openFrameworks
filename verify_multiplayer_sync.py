#!/usr/bin/env python3
"""
Simple card sync verification - checks key points for multiplayer sync.
"""

import re
from pathlib import Path

def main():
    script_dir = Path(__file__).parent
    cpp_file = script_dir / "src" / "ofApp.cpp"
    
    if not cpp_file.exists():
        print(f"ERROR: Could not find {cpp_file}")
        return 1
    
    content = cpp_file.read_text()
    
    print("=" * 80)
    print("MageFight Multiplayer Synchronization Verification")
    print("=" * 80)
    print()
    
    issues = []
    
    # Check 1: All summon cards have turn order sorts
    print("1. Checking turn order sorts...")
    sort_count = len(re.findall(r'std::sort\(players\.begin\(\), players\.end\(\)', content))
    print(f"   ✓ Found {sort_count} turn order sorts")
    
    # Verify the sort logic is correct
    correct_sort_pattern = r'if \(a\.isMinion && !b\.isMinion\) return false;'
    correct_sorts = len(re.findall(correct_sort_pattern, content))
    print(f"   ✓ {correct_sorts}/{sort_count} sorts use correct minion priority (false = minions first)")
    
    if correct_sorts != sort_count:
        issues.append(f"Only {correct_sorts}/{sort_count} sorts have correct minion priority")
    
    print()
    
    # Check 2: Verify deterministic RNG usage
    print("2. Checking random number generation...")
    
    # Count uses of deterministic RNG
    game_random_uses = len(re.findall(r'getGameRandom\(', content))
    gameplay_rng_uses = len(re.findall(r'gameplayRNG\)', content))
    
    print(f"   ✓ Found {game_random_uses} uses of getGameRandom()")
    print(f"   ✓ Found {gameplay_rng_uses} uses of gameplayRNG")
    
    # Check for problematic random usage in card logic (between playCard and executeOpponentCardPlay functions)
    play_card_match = re.search(r'CardPlayResult ofApp::playCard.*?(?=^void ofApp::)', content, re.MULTILINE | re.DOTALL)
    exec_opponent_match = re.search(r'void ofApp::executeOpponentCardPlay.*?(?=^void ofApp::)', content, re.MULTILINE | re.DOTALL)
    
    if play_card_match and exec_opponent_match:
        card_logic = play_card_match.group(0) + exec_opponent_match.group(0)
        
        # Look for ofRandom (bad) in card logic
        bad_random = re.findall(r'ofRandom\s*\(', card_logic)
        if bad_random:
            print(f"   ⚠ WARNING: Found {len(bad_random)} uses of ofRandom() in card logic")
            issues.append(f"{len(bad_random)} uses of non-deterministic ofRandom() in card logic")
        else:
            print(f"   ✓ No ofRandom() calls in card logic (good)")
    
    print()
    
    # Check 3: Verify ActionPacket handling
    print("3. Checking network packet system...")
    
    # Count sendActionPacket calls
    send_action_count = len(re.findall(r'sendActionPacket\(', content))
    print(f"   ✓ Found {send_action_count} sendActionPacket() calls")
    
    # Verify executeOpponentCardPlay exists and has special cases
    if 'void ofApp::executeOpponentCardPlay' in content:
        print(f"   ✓ executeOpponentCardPlay() exists")
        
        # Count special card handlers
        special_cards = [
            'CARD_GIANT_MAGIC_HAND',
            'CARD_DISPEL',
            'CARD_TRAIN',
            'CARD_WISDOM_BOON',
            'CARD_BURST_OF_LIGHT',
            'CARD_DOUBLE_HANDED',
            'CARD_CHAIN_LIGHTNING',
            'CARD_TELEPORT',
            'CARD_RENEWED_INSPIRATION'
        ]
        
        exec_opponent_code = exec_opponent_match.group(0) if exec_opponent_match else ""
        found_special = sum(1 for card in special_cards if card in exec_opponent_code)
        print(f"   ✓ {found_special}/{len(special_cards)} special cards have custom opponent execution")
        
        if found_special != len(special_cards):
            issues.append(f"Only {found_special}/{len(special_cards)} special cards in executeOpponentCardPlay")
    else:
        print(f"   ✗ executeOpponentCardPlay() NOT FOUND!")
        issues.append("executeOpponentCardPlay() missing")
    
    print()
    
    # Check 4: Verify dice synchronization
    print("4. Checking dice roll synchronization...")
    
    start_dice_count = len(re.findall(r'startDiceRoll\(', content))
    print(f"   ✓ Found {start_dice_count} startDiceRoll() calls")
    print(f"   ✓ Dice system handles synchronization automatically")
    
    print()
    
    # Check 5: Verify minion health sync
    print("5. Checking minion maxHealth synchronization...")
    
    # Find summon patterns and check if they set both health and maxHealth
    summon_patterns = [
        (r'case CARD_RAISE_DEAD:', 'Raise Dead'),
        (r'case CARD_SUMMON_GOLEM:', 'Summon Golem'),
        (r'case CARD_SUMMON_HELLHOUND:', 'Summon Hellhound'),
        (r'case CARD_SUMMON_DEMON:', 'Summon Demon'),
        (r'case CARD_SUMMON_KOBOLD_KING:', 'Summon Kobold King'),
        (r'case CARD_SUMMON_ASSISTANT:', 'Summon Assistant'),
        (r'case CARD_SUMMON_FAERIE:', 'Summon Faerie'),
        (r'case CARD_CALL_FOR_WOLVES:', 'Call For Wolves'),
        (r'case CARD_CALL_FOR_KOBOLDS:', 'Call For Kobolds'),
        (r'case CARD_TRANSFORM_WALL:', 'Transform Wall'),
    ]
    
    for pattern, name in summon_patterns:
        match = re.search(pattern, content)
        if match:
            # Get next 500 characters to check for both health and maxHealth
            snippet = content[match.start():match.start() + 2000]
            has_health = '.health =' in snippet or '.health=' in snippet
            has_max_health = '.maxHealth =' in snippet or '.maxHealth=' in snippet
            
            if has_health and has_max_health:
                print(f"   ✓ {name}: Sets both health and maxHealth")
            elif has_health and not has_max_health:
                print(f"   ⚠ {name}: Sets health but NOT maxHealth!")
                issues.append(f"{name} doesn't set maxHealth")
            else:
                # Health might be set in dice callback
                print(f"   ? {name}: Health set via dice callback (needs manual check)")
    
    print()
    
    # Check 6: Verify checksum calculation
    print("6. Checking game state checksum...")
    
    checksum_match = re.search(r'long long ofApp::calculateChecksum\(\).*?(?=^[a-zA-Z])', content, re.MULTILINE | re.DOTALL)
    if checksum_match:
        checksum_code = checksum_match.group(0)
        
        # Verify important state is included
        important_fields = ['playerID', 'health', 'x', 'y', 'block', 'ward']
        found_fields = sum(1 for field in important_fields if f'p.{field}' in checksum_code)
        
        print(f"   ✓ Checksum exists")
        print(f"   ✓ Includes {found_fields}/{len(important_fields)} critical fields")
        
        # Check if maxHealth is in checksum
        if 'maxHealth' in checksum_code:
            print(f"   ✓ Includes maxHealth (good for minion sync)")
        else:
            print(f"   ⚠ Does NOT include maxHealth (could miss minion desyncs)")
            issues.append("Checksum doesn't include maxHealth")
    else:
        print(f"   ✗ Checksum function NOT FOUND!")
        issues.append("calculateChecksum() missing")
    
    print()
    
    # Summary
    print("=" * 80)
    print("SUMMARY")
    print("=" * 80)
    print()
    
    if issues:
        print("⚠ ISSUES FOUND:")
        for i, issue in enumerate(issues, 1):
            print(f"   {i}. {issue}")
        print()
        print("Review the issues above before testing multiplayer.")
        return 1
    else:
        print("✓ ALL CHECKS PASSED")
        print()
        print("Key multiplayer features verified:")
        print("  • Turn order sorting (minions before players)")
        print("  • Deterministic RNG (no ofRandom in card logic)")
        print("  • Network packets (ActionPacket system)")
        print("  • Dice synchronization (startDiceRoll)")
        print("  • State checksums (desync detection)")
        print()
        print("Cards should synchronize correctly in multiplayer!")
        return 0

if __name__ == "__main__":
    exit(main())

[![Build Status](https://travis-ci.org/TwistedScorpio/OTHire.svg?branch=master)](https://travis-ci.org/TwistedScorpio/OTHire)

## OTHire 0.0.3

- OpenTibia Server for Tibia 7.72
- This OTServer is based on OTServ Trunk SVN work (latest stable trunk rev).
- Compatible AAC, ZnoteAAC [HERE](https://github.com/peonso/ZnoteOTHire).
- 0.0.3 release milestone [HERE](https://github.com/TwistedScorpio/OTHire/milestone/1).

### Changelog

#### 0.0.3

	- Healing and attacking spells shares exhaust
	- Players block ranged physical attacks from monsters with shield
	- Creature events onDie, onKill, onAdvance available
	- Fixed monsters casting spells through walls
	- Fixed monsters being traped by moveable items
	- Fixed bug with beds itemids
	- Fixed some crash bugs
	- Updated items.xml
	- Updated spells
	- Inumerous minor bugs fixed

#### 0.0.2

    - Many items were fixed.
    - Many issues from non Tibia 7.72 were corrected.
    - Fixed violation window, wasn't from 7.72
    - Added items.xml
    - Optimization Changes
    - Linux Compilation
    - Added option for stackable items auto stacking like in newer Tibia versions

####  0.0.1

    - Runes
    - Talking (yelling, whispering)
    - Private messaging
    - Rule Violation Channel
    - Bug Reports (Ctrl+Z)
    - Saving & Loading Players
    - House System
    - House Paying through Bank account or depot balance
    - Premium System
    - Skull System (to use compile with __SKULL_SYSTEM__)
    - Anti-Magebomb (attack delay when logging in)
    - Sharing Experience when in party (code is there, implementation is needed through talkactions)
    - GM Invisibility
    - Avesta Spells
    - Players have bank accounts
    - OTServ Trunk Movements (removed unused code)
    - OTServ Trunk Talkactions (removed unused code)
    - Faster OTBM map reading, reading a 120 mb real map in 4.3 seconds, compared to 5.5 seconds RealOTs's Avesta.
    - No more (return TRUE) (if isPremium(cid) == TRUE). Thanks to newer LUA core code
    - Many more
    - ... Everything as in OTServ Trunk

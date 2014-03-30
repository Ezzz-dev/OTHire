OTHire Beta [![Build Status](https://travis-ci.org/TwistedScorpio/OTHire.svg?branch=master)](https://travis-ci.org/TwistedScorpio/OTHire)
======

OpenTibia Server for Tibia 7.72

This OTServer is based on OTServ Trunk SVN work (latest stable trunk rev).

The change log can be found to the bottom.

NOTE: This server was downgraded in a day, so some things might be left behind and will be worked on later on.

Important
======

You will need to use your own items.xml (7.6) - Server Data holds none as of yet

Features
======

  0.0.1
  
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
    
Change Log
======

  0.0.1
  
    - Downgraded from protocol 8.61 to 7.72
    - Removed unused code from 7.72 (partial; Share Experience in Party still included in sources)
    - Removed unused magic effects
    - Removed unused projectiles
    - Removed Trade-Rookgaard
    - Trade channel now able for everyone even if no vocation
    - Added magic effect when players are logging in
    - Items now from Avesta (might need some fixes in duplicated item ids)
    - Removed unused options from configuration file
    - Removed walk through code
    - Added spells from Avesta
    - Added monsters from Avesta
    - Fixed a bug with players health bar not being shown due to a bad code behavior made by a noob developer
    - Fixed an issue with cancel target packet
    - Fixed an issue with creature stack order (reversed)
    - Fixed an issue with logging in magic effect shown in wrong player
    - Removed outfits
    - Removed quests
    - Outfits handled through sources using Avesta code
    - Removed unused talkactions
    - Removed unused code from movements
    - Added Avesta Spells
    
Known Bugs / Non old Tibia behavior
======

  0.0.1
  
    - Can walk over 2 or more parcels
    - Blood splash is yellow
    
None as of yet.

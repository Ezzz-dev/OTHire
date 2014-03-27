OTHire Beta
======

OpenTibia Server for Tibia 7.72

This OTServer is based on OTServ Trunk SVN work (latest stable trunk rev).

The change log can be found to the bottom.

THIS README ISN'T COMPLETE

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
    - Invisibility for characters
    
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
    
Known Bugs
======

  0.0.1
  
    - Cannot Be Seen isn't working accurately, newer releases this will be removed!
    
None as of yet.

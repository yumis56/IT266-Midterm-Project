# Yumi's "mymod" mod

## Setup
1. Copy the mymod folder into your Quake 4 directory

    - The full path should look something like this: `C:\Program Files (x86)\Steam\steamapps\common\Quake 4\mymod`

This will contain:
```
+-- Quake4Config.cfg
+-- game001.pk4
    +-- binary.conf
    +-- Gamex86.dll
    +-- guis/
        +-- hud.gui
        +-- wristcomm.gui
```


### Shortcut
Create a shortcut for your Quake4.exe and set the target as:

  `"C:\Program Files (x86)\Steam\steamapps\common\Quake 4\Quake4.exe" +set fs_game mymod +disconnect`

- This will automatically launch the mod and also skip the 3 cutscenes at game launch

## The mymod Guide

## Key Inputs & features
**H**: We don't need any mission objectives! Use this key to view the in-game guide for mymod.

**Tab**: Auto-target!
  - No enemies: target/untarget the player with a blue highlight
  - No target, has enemies: target the first enemy
  - In-combat: keep pressing tab to target the next enemy!

**Mouse1 (left-click)**: untarget current target. This is to prevent accidental un-targeting while in combat.

**1**: Fire charged blaster attack! Can be done while auto-attacking, or to initiate auto-target -> charged blaster -> auto-attack

||Debug||
**G**: Spawn an enemy grunt to test out the new features!


## Highlights Summary

### Targeting System
- Able to target an enemy (highlight red)
- Able to cycle through multiple enemies
- Able to target player if no enemies exist (highlight blue)
- If a target enemy dies, it automatically moves to the next enemy or to the player
- Doing a special action (like "1" for charged blaster) will automatically target enemy when applicable

### Target Display
- Enemy health bar and name is now displayed
- Name and health updates properly and does not reset upon un-targeting or selecting another target

### Auto-Attack
- Player will only attack if their target is a valid enemy (not NULL or themselves)
- Auto-attack is initiated upon targeting (Tab) or using a special action (like 1), and then finding a valid enemy target
- Auto-attack will continue
- Auto-attacks will resume smoothly after any special actions are executed

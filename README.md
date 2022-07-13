# gba-drum

pour mettre un "o" dans le screen patern :

A + up (A + up + right s'il y a un bug de l'emulateur)

Update : A

Certaines combinaisons de touches sur emulateur peuvent poser problème, 
dans ce cas modifier la touche select par une autre touche du clavier.

This is a basic drum/pattern sequencer for the Nintendo GameBoy Advance. Compiled with devkit pro, using the libgba and maxmod libraries. Tested on visualboyadvance and no$gba & on actual gba with various flash cartridges.

What works:
-pattern sequencing & playback

-song/chain sequencing & playback

-in-program sample swapping

-song mode / live mode looping

-saving/loading

-pitch shifting of samples

-panning of samples

-volume of samples

-copy/paste patterns and orders

-random pattern generation


Update Maz Hoot :

- sync mode for Volcas with Y cable (all sounds are mono when it's activated)
- b for random sample (randomise pan too) random a sample at each plays
- a for enter a sample ('o') (alternate with o and O)
- b+up/down for change bpm. Bpm shown also on pattern screen
- "IA" modes : Random, Bpm

I couldn't figure out how make tolmdym sync works. The volca sync I added, works with :
When sync don't work, it seems to be a question of volume. It's just that the console can't make a sync sound enough loud
- DS Lite (Volca works, akai rythm wolf don't work)
- GBA SP (Volca works, akai rythm wolf don't work)
- GBA (don't work, but maybe it was because battery was very low)
- gameboy player on gamecube (Volca and akai rhythm wolf works)

# tolmdyn-gba-drum


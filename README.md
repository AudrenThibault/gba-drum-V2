# gba-drum

## the project is currently abandonned because I didn't succeed have both pulse/noise channel sounds from the gba and samples and that was my reason to work on this project. That's why the actual version is on nintendo ds, whith only samples but many more options :
https://github.com/AudrenThibault/root-drum-machine

## Build :
There must be wav files inside maxmod_data for compilation works
make clean
make

Pour compiler avec libgba ET libtonc (pour avoir les samples ET les sons gameboy), j'ai fait PLEINS de modifs dans les dossier /opt/devkitpro/libgba/include et /opt/devkitpro/libtonc/include pour empêcher tous les conflits et autre

Quand je veux inclure un truc, regarder si le fichier est pas déjà inclus via tonc_libgba.h qui est dans /opt/devkitpro/libtonc/include

## Create a file named maxmod_data with your .wav samples

put "o" : A  //sample
put "R" : B  //random note

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


Update Audren :

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


# webNEZ

Copyright (C) 2018 Juergen Wothke

This is a JavaScript/WebAudio plugin of "NEZplug(++)" used to emulate music of various consoles (Famicom, 
Game Gear, Game Boy, MSX, ZX Spectrum, Hudson, etc. This plugin is designed to work with my generic WebAudio 
ScriptProcessor music player (see separate project). 

It supposedly emulates the following sound chips:

        Richo     NES-APU(RP2A03H)             [Famicom(NES)]
        Konami    VRC6(053328 VRC VI)          Castle vania 3, Madara, ...
        Konami    VRC7(053982 VRC VII)         Lagrange Point
        Nintendo  MMC5                         Just Breed, Yakuman Tengoku
        Richo     FDS(RP2C33A)                 Zelda(FDS), Dabada(FDS), ...
        Namco     NAMCOT106                    D.D.S.2, KingOfKings, ...
        Sunsoft   FME7                         Gimmick!

        GI        PSG(AY-3-8910)               [MSX] [ZX Spectrum]
        Yamaha    SSG(YM2149)                  [Amstrad CPC]
        Yamaha    FM-PAC(YM2413)               Aleste, Xak, ...
        Yamaha    MSX-AUDIO(Y8950)             Labyrinth, Xevious, ...
        Konami    SCC(051649 SCC-I2212)        Nemesis2, Space Manbow, ...
        Konami    SCC+(052539 SCC-I2312)       SD Snatcher, ...
        Konami    MajutushiDAC                 Majutushi

        TI        SMS-SNG(SN76496)             [SEGA-MKIII(SMS)]
        Yamaha    FM-UNIT(YM2413)              Shinobi, Tri Formation, ...
        Sega      GG-SNG                       [Game Gear]

        Richo     DMG-SYSTEM                   [Game Boy]

        Hudson    HES-PSG(HuC6280)             [PC-Engine(TG16)]


This version should now play NSF and KSS, GBR, GBS, HES, AY and also SGC files.

A live demo of this program can be found here: http://www.wothke.ch/webNEZ/


## Credits
The project is based on: http://offgao.net/program/nezplug++.html and https://github.com/BouKiCHi/nezplay 


## Project

I had first tried to use NEZplug++ 0.9.4.8 + 3 + 24.00 (see http://offgao.net/program/nezplug++.html) 
but even after fixing the obvious problems reported by CLANG, the resulting code would still crash with 
"segmentation faults" somewhere in the waveform generation logic.. I did not feel like migrating/fixing 
some obviously outdated codebase (especially since the project's http://nezplug.sourceforge.net/
page seems to be down).

When I found https://github.com/BouKiCHi/nezplay (NEZplug Version 0.9.5.3 - apparently there are
more recent versions than the above) I decided to give it another try and indeed not only does that 
version compile properly, it also works right out of the box.

I then copied the m_sgc.c from older NEZplug++ and patched it into the nezplay version (to 
add some "Sega Master System","Game Gear","Coleco Vision" support). It seems to play .sgc files 
now but I don't know if there might be "finer points" that I have missed. This version now runs on
defaults without use of separate settings (or separate ROM files - I don't know if that scenario
even exists). If somebody else wants to do respective "fine tuning" he is cordially invited to
do that work. I also patched the code to set meaningful display infos (title, artist, copyright)
for SGC songs.

All the "Web" specific additions (i.e. the whole point of this project) are contained in the 
"emscripten" subfolder.


## Howto build

You'll need Emscripten (http://kripken.github.io/emscripten-site/docs/getting_started/downloads.html). The make script 
is designed for use of emscripten version 1.37.29 (unless you want to create WebAssembly output, older versions might 
also still work).

The below instructions assume that the webNEZ project folder has been moved into the main emscripten 
installation folder (maybe not necessary) and that a command prompt has been opened within the 
project's "emscripten" sub-folder, and that the Emscripten environment vars have been previously 
set (run emsdk_env.bat).

The Web version is then built using the makeEmscripten.bat that can be found in this folder. The 
script will compile directly into the "emscripten/htdocs" example web folder, were it will create 
the backend_nez.js library. (To create a clean-build you have to delete any previously built libs in the 
'built' sub-folder!) The content of the "htdocs" can be tested by first copying it into some 
document folder of a web server. 


## Depencencies

The current version requires version 1.03 (older versions will not
support WebAssembly or may have problems skipping playlist entries) 
of my https://github.com/wothke/webaudio-player.

This project comes without any music files, so you'll also have to get your own and place them
in the htdocs/music folder (you can configure them in the 'songs' list in index.html).


## License

This library is free software; you can redistribute it and/or modify it
under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or (at
your option) any later version. This library is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA

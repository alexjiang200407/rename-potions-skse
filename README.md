## Description
CommonLibSSE Template for my mods. Using Po3's CommonLibSSE fork.

## Runtime Requirements

* SKSE
* Address Library for SKSE (for both 1.6.x and 1.5.x).
* Windows OS
* SkyUI

## Build Requirements

* [Visual Studio 2022](https://visualstudio.microsoft.com/vs/).
* CMake Tools for Visual Studio (Installed via Visual Studio Installer).
* [vcpkg](https://github.com/microsoft/vcpkg).
* [pdbcopy](https://support.microsoft.com/en-au/topic/pdbcopy-tool-1eb343a1-52e1-816c-451c-e569cef6b297) (optional).

## Environment Variables
* Set VCPKG_ROOT to installation directory of VCPKG
* Set SkyrimModsPath to Skyrim mods directory (usually the mods directory for your mod manager).
* Add pdbcopy directory into PATH if you have pdbcopy installed and want to strip private symbols for release.

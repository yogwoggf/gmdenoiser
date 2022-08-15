# gmdenoiser 🧽

[![VisTrace EXTENSION](https://github.com/Derpius/VisTrace/blob/branding/extension.svg?raw=true)](https://github.com/Derpius/VisTrace)

gmdenoiser is a binary module that adds OpenImageDenoise to the GMod Lua environment. This is useful mainly for Monte-Carlo light transport simulations, but can be used with anything else.

# Platforms 🖥

gmdenoiser currently only supports 64-bit Windows. Multiplatform support is currently not an objective.

# Installing 📂

gmdenoiser can be built from source or downloaded from the Releases. There is some setup involved. From the binaries (linked in Building), there's a `bin` folder. Copy all of the dlls inside that bin folder to your root GarrysMod folder. (the one in `steamapps/common`)

# Building 🏗

To build, you'll need:

-   CMake 3.20.0 or above
-   Ninja (installed in path)
-   Visual Studio 2019 (the C++ compiler has to be installed obviously, untested if newer versions work)

Make sure to clone the repository recursively, but you will be missing OpenImageDenoise. This is because OpenImageDenoise is a massive library, so this project uses the prebuilt binaries.
You can get the binaries [here](https://github.com/OpenImageDenoise/oidn/releases/download/v1.4.3/oidn-1.4.3.x64.vc14.windows.zip), download the windows version and make sure to rename the inner folder "oidn" and copy it to the `libs` folder.

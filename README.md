# gmdenoiser 🧽

[![VisTrace EXTENSION](https://github.com/Derpius/VisTrace/blob/branding/extension.svg?raw=true)](https://github.com/Derpius/VisTrace)

gmdenoiser is a binary module that adds OpenImageDenoise to the GMod Lua environment. This is useful mainly for Monte-Carlo light transport simulations, but can be used with anything else.

# Platforms 🖥

gmdenoiser currently only supports 64-bit Windows. Multiplatform support is currently not an objective.

# Installing 📂

gmdenoiser can be built from source or downloaded from the Releases. There is some setup involved. From the [binaries](https://github.com/OpenImageDenoise/oidn/releases/download/v1.4.3/oidn-1.4.3.x64.vc14.windows.zip), there's a `bin` folder. Copy all of the dlls inside that bin folder to your root GarrysMod folder. (the one in `steamapps/common`)

# API 🛠

gmdenoiser adds 1 simple function to the VisTrace RenderTarget type.
```lua
local rt = vistrace.CreateRenderTarget(...)
rt:Denoise({
	-- Pass in your auxillary buffers, if any.
	Albedo = albedo,
	Normal = normal,

	-- If you do pass any in, you must tell the denoiser if its noisy or not.
	AlbedoNoisy = false,
	NormalNoisy = false,

	-- HDR and sRGB are mutally exclusive, you cannot enable both.
	HDR = true,
	sRGB = false
})

-- You're also allowed to pass nothing.
-- Doing this will produce a very bad output.
rt:Denoise({})

-- You can pass some parameters, but here are the base rules that will allow for a successful denoise.
-- Any auxillary image (Albedo, Normal) will require their Noisy boolean to be filled out.
-- If you only pass Albedo, then you need to pass in AlbedoNoisy as well. Likewise with Normal.

-- HDR and sRGB cannot be enabled at the same time.
```

Note: Denoising as a LDR image might be beneficial because OpenImageDenoise's documentation states that HDR images are interpreted in physical units. The accuracy of these physical units (they're automatically inferred) may lower quality of the output.
To activate gmdenoiser, you simply need to `require` the binary. You can do this whenever you want, but if you do it before VisTrace loads, it will wait until VisTrace loads.

# Building 🏗

To build, you'll need:

-   CMake 3.20.0 or above
-   Ninja (installed in path)
-   Visual Studio 2019 (the C++ compiler has to be installed obviously, untested if newer versions work)

Make sure to clone the repository recursively, but you will be missing OpenImageDenoise. This is because OpenImageDenoise is a massive library, so this project uses the prebuilt binaries.
You can get the binaries [here](https://github.com/OpenImageDenoise/oidn/releases/download/v1.4.3/oidn-1.4.3.x64.vc14.windows.zip), download the windows version and make sure to rename the inner folder "oidn" and copy it to the `libs` folder.

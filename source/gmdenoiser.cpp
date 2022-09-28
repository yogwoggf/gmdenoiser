#include <cstdio>

#include "GarrysMod/Lua/Interface.h"
#include "OpenImageDenoise/oidn.h"
#include "vistrace.h"

using namespace GarrysMod::Lua;
using namespace VisTrace;
#define PUSH_CFUNC(func, name) LUA->PushCFunction(func); LUA->SetField(-2, name);

#pragma region VisTrace Extension
LUA_FUNCTION(IRenderTarget_Denoise) {
	LUA->CheckType(1, VType::RenderTarget); // RT
	LUA->CheckType(2, Type::Table); // Info

	// Information structure:
	// RT Albedo = null
	// RT Normal = null
	// bool AlbedoNoisy = false, but required to know if albedo is filled out
	// bool NormalNoisy = false, but required to know if normal is filled out
	// bool HDR
	// bool sRGB Can't be enabled if HDR is enabled
	// Anything with no default value must be filled out (for the users sake to provide the best possible output
	
	
	IRenderTarget* color = *LUA->GetUserType<IRenderTarget*>(1, VType::RenderTarget);
	if (!color->IsValid()) {
		LUA->ThrowError("RenderTarget is invalid!");
	}

	if (color->GetFormat() != RTFormat::RGBFFF) {
		LUA->ThrowError("RenderTarget is not in RGBFFF!");
	}

	IRenderTarget* albedo = nullptr;
	IRenderTarget* normal = nullptr;

	bool albedoNoisy = false, normalNoisy = false;
	bool hdr = false, sRGB = false;

	// Read info
	LUA->GetField(-1, "Albedo");
	if (LUA->IsType(-1, VType::RenderTarget)) {
		albedo = *LUA->GetUserType<IRenderTarget*>(-1, VType::RenderTarget);
		LUA->Pop(); // Pop off the albedo pointer
		if (!albedo->IsValid()) {
			LUA->ThrowError("Albedo RT is invalid!");
		}
	}
	else {
		LUA->Pop();
	}
	
	LUA->GetField(-1, "Normal");
	if (LUA->IsType(-1, VType::RenderTarget)) {
		normal = *LUA->GetUserType<IRenderTarget*>(-1, VType::RenderTarget);
		LUA->Pop(); // Pop off the normal pointer
		if (!normal->IsValid()) {
			LUA->ThrowError("Normal RT is invalid!");
		}
	}
	else {
		LUA->Pop();
	}

	LUA->GetField(-1, "AlbedoNoisy");
	if (!LUA->IsType(-1, Type::Bool)) {
		// It's not filled out.
		// If albedo is filled out, require this to be filled out aswell
		if (albedo) {
			LUA->ThrowError("AlbedoNoisy must be filled out!");
		}
		else {
			LUA->Pop(); // Fall back to default value
		}
	}
	else {
		albedoNoisy = LUA->GetBool();
		LUA->Pop();
	}

	LUA->GetField(-1, "NormalNoisy");
	if (!LUA->IsType(-1, Type::Bool)) {
		// It's not filled out.
		// If albedo is filled out, require this to be filled out aswell
		if (normal) {
			LUA->ThrowError("NormalNoisy must be filled out!");
		}
		else {
			LUA->Pop(); // Fall back to default value
		}
	}
	else {
		normalNoisy = LUA->GetBool();
		LUA->Pop();
	}

	LUA->GetField(-1, "HDR");
	hdr = LUA->GetBool();
	LUA->Pop();

	LUA->GetField(-1, "sRGB");
	sRGB = LUA->GetBool();
	LUA->Pop();

	// Pop off information table
	LUA->Pop();

	if (hdr && sRGB) {
		// Cant have both.
		LUA->ThrowError("HDR and sRGB are both enabled. This is not possible!");
	}
	
	uint16_t width = color->GetWidth();
	uint16_t height = color->GetHeight();

	OIDNDevice dev = oidnNewDevice(OIDN_DEVICE_TYPE_DEFAULT);
	oidnCommitDevice(dev);

	// Past this point, throwing errors may cause leaks if you don't release the OIDN resources!

	// Prefiltering
	if (albedo) {
		if (albedo->GetFormat() != RTFormat::RGBFFF) {
			oidnReleaseDevice(dev);
			LUA->ThrowError("The albedo buffer must be in the format RGBFFF!");
		}

		if (albedo->GetWidth() != width || albedo->GetHeight() != height) {
			oidnReleaseDevice(dev);
			LUA->ThrowError("The albedo buffer must be the same size as the color buffer!");
		}

		if (albedoNoisy) {
			// Perform a prefilter.
			OIDNFilter albedoPrefilter = oidnNewFilter(dev, "RT");
			float* albedoBuffer = reinterpret_cast<float*>(albedo->GetRawData(0));

			oidnSetSharedFilterImage(albedoPrefilter, "albedo", albedoBuffer, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
			oidnSetSharedFilterImage(albedoPrefilter, "output", albedoBuffer, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
			
			oidnCommitFilter(albedoPrefilter);
			oidnExecuteFilter(albedoPrefilter);

			const char* errMsg;
			if (oidnGetDeviceError(dev, &errMsg) != OIDN_ERROR_NONE) {
				char formatted_error[512];
				snprintf(formatted_error, 512, "Error while prefiltering albedo: %s", errMsg);

				oidnReleaseFilter(albedoPrefilter);
				oidnReleaseDevice(dev);
				LUA->ThrowError(formatted_error);
			}

			oidnReleaseFilter(albedoPrefilter);
		}
	} 

	if (normal) {
		if (normal->GetFormat() != RTFormat::RGBFFF) {
			oidnReleaseDevice(dev);
			LUA->ThrowError("The normal buffer must be in the format RGBFFF!");
		}

		if (normal->GetWidth() != width || normal->GetHeight() != height) {
			oidnReleaseDevice(dev);
			LUA->ThrowError("The normal buffer must be the same size as the color buffer!");
		}

		if (normalNoisy) {
			// Perform a prefilter.
			OIDNFilter normalPrefilter = oidnNewFilter(dev, "RT");
			float* normalBuffer = reinterpret_cast<float*>(normal->GetRawData(0));

			oidnSetSharedFilterImage(normalPrefilter, "normal", normalBuffer, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
			oidnSetSharedFilterImage(normalPrefilter, "output", normalBuffer, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);

			oidnCommitFilter(normalPrefilter);
			oidnExecuteFilter(normalPrefilter);

			const char* errMsg;
			if (oidnGetDeviceError(dev, &errMsg) != OIDN_ERROR_NONE) {
				char formatted_error[512];
				snprintf(formatted_error, 512, "Error while prefiltering normal: %s", errMsg);

				oidnReleaseFilter(normalPrefilter);
				oidnReleaseDevice(dev);

				LUA->ThrowError(formatted_error);
			}

			oidnReleaseFilter(normalPrefilter);
		}
	}

	float* colorBuffer = reinterpret_cast<float*>(color->GetRawData(0));

	OIDNFilter filter = oidnNewFilter(dev, "RT");
	oidnSetSharedFilterImage(filter, "color", colorBuffer, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);

	if (albedo) {
		float* albedoBuffer = reinterpret_cast<float*>(albedo->GetRawData(0));
		oidnSetSharedFilterImage(filter, "albedo", albedoBuffer, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
	}

	if (normal) {
		float* normalBuffer = reinterpret_cast<float*>(normal->GetRawData(0));
		oidnSetSharedFilterImage(filter, "normal", normalBuffer, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
	}

	oidnSetSharedFilterImage(filter, "output", colorBuffer, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0); // Self-modifying
	oidnSetFilter1b(filter, "hdr", hdr);
	oidnSetFilter1b(filter, "srgb", sRGB);
	// Ensured in our sense because of prefiltering, but not always true because the user might mess up and pass in noisy auxillary images.
	oidnSetFilter1b(filter, "cleanAux", true);
	oidnCommitFilter(filter);

	// Denoise!!
	oidnExecuteFilter(filter);

	const char* errMsg;

	if (oidnGetDeviceError(dev, &errMsg) != OIDN_ERROR_NONE) {
		// Well shit, an error occurred.

		oidnReleaseFilter(filter);
		oidnReleaseDevice(dev);

		LUA->ThrowError(errMsg);
		return 0;
	}

	oidnReleaseFilter(filter);
	oidnReleaseDevice(dev);

	// Yay, it all worked!!
	return 0;
}
#pragma endregion

VISTRACE_EXTENSION_OPEN(gmdenoiser) {
	bool worked = LUA->PushMetaTable(VType::RenderTarget);
	if (!worked) {
		// Only for this error, we will clearly specify that this is gmdenoiser. It might be unclear what is causing this error to someone who is skimming over Console.
		LUA->ThrowError("gmdenoiser: Failed to modify RenderTarget. (the extension will not function)");
	}

	// Add the functions.
	PUSH_CFUNC(IRenderTarget_Denoise, "Denoise");
	LUA->Pop(1); // Done!!
}

VISTRACE_EXTENSION_CLOSE() {}
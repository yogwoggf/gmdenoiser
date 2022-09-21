#include "GarrysMod/Lua/Interface.h"
#include "OpenImageDenoise/oidn.h"
#include "vistrace.h"

#include <cstdio>

using namespace GarrysMod::Lua;
using namespace VisTrace;
#define PUSH_CFUNC(func, name) LUA->PushCFunction(func); LUA->SetField(-2, name);

#pragma region VisTrace Extension
// Remember that this and the OIDN raw api that we created above are NOT connected.
// This is supposed to be a mindless 1 click ez-denoise button with some options for finer control.
// This is why we create certain OIDN objects and then remove them at the end of the function unlike the raw API above.
LUA_FUNCTION(IRenderTarget_Denoise) {
	LUA->CheckType(1, VType::RenderTarget); // RT
	// 2 and 3 are optional albedo and normal.ate it
	// 4 and 5 are albedoNoisy and normalNoisy
	// We need to check if we have any extra buffers we can use.

	bool albedo_noisy = LUA->GetBool(4);
	bool normal_noisy = LUA->GetBool(5);
	
	IRenderTarget* color = *LUA->GetUserType<IRenderTarget*>(1, VType::RenderTarget);
	if (!color->IsValid()) {
		LUA->ThrowError("RenderTarget is invalid!");
	}

	IRenderTarget* albedo = nullptr;
	IRenderTarget* normal = nullptr;

	if (LUA->IsType(2, VType::RenderTarget)) {
		albedo = *LUA->GetUserType<IRenderTarget*>(2, VType::RenderTarget);
		if (!albedo->IsValid()) {
			LUA->ThrowError("Albedo RT is invalid!");
		}
	}

	if (LUA->IsType(3, VType::RenderTarget)) {
		normal = *LUA->GetUserType<IRenderTarget*>(3, VType::RenderTarget);
		if (!normal->IsValid()) {
			LUA->ThrowError("Normal RT is invalid!");
		}
	}

	uint16_t width = color->GetWidth();
	uint16_t height = color->GetHeight();

	// Check the albedo and normal buffer
	OIDNDevice dev = oidnNewDevice(OIDN_DEVICE_TYPE_DEFAULT);
	oidnCommitDevice(dev);

	if (albedo) {
		if (albedo->GetFormat() != RTFormat::RGBFFF) {
			oidnReleaseDevice(dev);
			LUA->ThrowError("The albedo buffer must be in the format RGBFFF!");
		}

		if (albedo->GetWidth() != width || albedo->GetHeight() != height) {
			oidnReleaseDevice(dev);
			LUA->ThrowError("The albedo buffer must be the same size as the color buffer!");
		}

		if (albedo_noisy) {
			// Perform a prefilter.
			OIDNFilter albedo_prefilter = oidnNewFilter(dev, "RT");
			float* albedoBuffer = reinterpret_cast<float*>(albedo->GetRawData(0));

			oidnSetSharedFilterImage(albedo_prefilter, "albedo", albedoBuffer, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
			oidnSetSharedFilterImage(albedo_prefilter, "output", albedoBuffer, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
			
			oidnCommitFilter(albedo_prefilter);
			oidnExecuteFilter(albedo_prefilter);

			const char* errMsg;
			if (oidnGetDeviceError(dev, &errMsg) != OIDN_ERROR_NONE) {
				char formatted_error[512];
				snprintf(formatted_error, 512, "Error while prefiltering albedo: %s", errMsg);

				oidnReleaseFilter(albedo_prefilter);
				oidnReleaseDevice(dev);
				LUA->ThrowError(formatted_error);
			}

			oidnReleaseFilter(albedo_prefilter);
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

		if (normal_noisy) {
			// Perform a prefilter.
			OIDNFilter normal_prefilter = oidnNewFilter(dev, "RT");
			float* normalBuffer = reinterpret_cast<float*>(normal->GetRawData(0));

			oidnSetSharedFilterImage(normal_prefilter, "normal", normalBuffer, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
			oidnSetSharedFilterImage(normal_prefilter, "output", normalBuffer, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);

			oidnCommitFilter(normal_prefilter);
			oidnExecuteFilter(normal_prefilter);

			const char* errMsg;
			if (oidnGetDeviceError(dev, &errMsg) != OIDN_ERROR_NONE) {
				char formatted_error[512];
				snprintf(formatted_error, 512, "Error while prefiltering normal: %s", errMsg);

				oidnReleaseFilter(normal_prefilter);
				oidnReleaseDevice(dev);

				LUA->ThrowError(formatted_error);
			}

			oidnReleaseFilter(normal_prefilter);
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
	oidnSetFilter1b(filter, "hdr", true);
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
	// We add this specific extension API:
	// - RenderTarget:Denoise(RenderTarget? albedo, RenderTarget? normal, bool albedoNoisy, bool normalNoisy)
	//     - the "color" argument is implicitly the RenderTarget being called with denoise, and this is a self-modifying operation

	bool worked = LUA->PushMetaTable(VType::RenderTarget);
	if (!worked) {
		LUA->ThrowError("OIDN: No IRenderTarget metatable found within GMod!");
	}

	// Add our own functions
	PUSH_CFUNC(IRenderTarget_Denoise, "Denoise");
	LUA->Pop(1); // Done!!
}

VISTRACE_EXTENSION_CLOSE() {}
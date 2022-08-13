#include "GarrysMod/Lua/Interface.h"
#include "OpenImageDenoise/oidn.h"
#include "vistrace/IRenderTarget.h"

#include <Windows.h>
#include <memory>

using namespace GarrysMod::Lua;
#define PUSH_CFUNC(func, name) LUA->PushCFunction(func); LUA->SetField(-2, name);

#pragma region Filter
static int Filter_id;
static void* output_ptr = nullptr;
static int output_width = 0;
static int output_height = 0;

LUA_FUNCTION(Filter_SetSharedImage) {
	LUA->CheckType(1, Filter_id); // Filter
	LUA->CheckType(2, Type::String); // Type
	LUA->CheckType(3, Type::Table); // Data
	LUA->CheckType(4, Type::Number); // Width
	LUA->CheckType(5, Type::Number); // Height

	OIDNFilter filter = *LUA->GetUserType<OIDNFilter>(1, Filter_id);
	const char* type = LUA->GetString(2);

	if (strstr(type, "output")) {
		int width = LUA->GetNumber(4);
		int height = LUA->GetNumber(5);

		output_width = width;
		output_height = height;
		output_ptr = malloc(static_cast<size_t>(width) * static_cast<size_t>(height) * 3 * sizeof(float));
		if (!output_ptr) {
			LUA->ThrowError("Filter::SetSharedImage - couldn't allocate enough memory for the output of the denoiser!");
		}

		oidnSetSharedFilterImage(filter, "output", output_ptr, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
		return 0;
	}

	int width = LUA->GetNumber(4);
	int height = LUA->GetNumber(5);

	if (width <= 0 || height <= 0) {
		LUA->ThrowError("Filter::SetSharedImage - width or height is below zero or zero");
	}

	int tableLen = LUA->ObjLen(3);
	float* data = reinterpret_cast<float*>(malloc(static_cast<size_t>(width) * static_cast<size_t>(height) * 3 * sizeof(float)));
	if (!data) {
		LUA->ThrowError("Filter::SetSharedImage - couldn't allocate enough memory.");
	}

	for (int idx = 0; idx < tableLen; idx++) {
		LUA->PushNumber(static_cast<double>(idx) + 1);
		LUA->GetTable(3);

		int dataAtIdxType = LUA->GetType(-1);
		// We can't just throw an error if its nil and be done with it.
		// We will literally leak potentially megabytes of data

		if (dataAtIdxType != Type::Number) {
			free(data);
			// Now we can throw a error willy nilly!!
			char errMsg[512];
			snprintf(errMsg, 512, "Filter::SetSharedImage - data isnt fully numbers. type: %d. idx: %d. tableLen: %d", dataAtIdxType, idx, tableLen);
			LUA->ThrowError(errMsg);
		}

		float dataAtIdx = LUA->GetNumber(-1);
		LUA->Pop();

		data[idx] = dataAtIdx;
	}

	// We also have to you know.. put the output ptr
	// We hide this process from the user because it is very much a hard thing to implement naturally (naturally means oidn writes into a lua table)
	// This code is very error-prone and is why the vistrace extension should be used as much as possible.

	oidnSetSharedFilterImage(filter, type, reinterpret_cast<void*>(data), OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);

	return 0;
}

LUA_FUNCTION(Filter_Set1b) {
	LUA->CheckType(1, Filter_id); // Filter
	LUA->CheckType(2, Type::String); // Attribute
	LUA->CheckType(3, Type::Bool); // Value

	OIDNFilter filter = *LUA->GetUserType<OIDNFilter>(1, Filter_id);

	oidnSetFilter1b(filter, LUA->GetString(2), LUA->GetBool(3));
	return 0;
}

LUA_FUNCTION(Filter_Commit) {
	LUA->CheckType(1, Filter_id); // Filter
	OIDNFilter filter = *LUA->GetUserType<OIDNFilter>(1, Filter_id);

	oidnCommitFilter(filter);
	return 0;
}

LUA_FUNCTION(Filter_Execute) {
	LUA->CheckType(1, Filter_id); // Filter
	OIDNFilter filter = *LUA->GetUserType<OIDNFilter>(1, Filter_id);
	oidnExecuteFilter(filter);

	LUA->CreateTable();
	float* output = reinterpret_cast<float*>(output_ptr);
	if (!output) {
		LUA->Pop();
		LUA->ThrowError("Filter::Execute - output is null! (maybe lack of memory)");
	}

	for (int i = 0; i < output_width * output_height * 3; i++) {
		LUA->PushNumber(static_cast<double>(i) + 1);
		LUA->PushNumber(output[i]);
		LUA->SetTable(-3);
	}

	return 1;
}

LUA_FUNCTION(Filter_gc) {
	LUA->CheckType(1, Filter_id); // Filter
	OIDNFilter filter = *LUA->GetUserType<OIDNFilter>(1, Filter_id);

	oidnReleaseFilter(filter);
	LUA->SetUserType(1, NULL);
	return 0;
}
#pragma endregion

#pragma region Device
static int Device_id;
LUA_FUNCTION(CreateOIDNDevice) {
	OIDNDevice dev = oidnNewDevice(OIDN_DEVICE_TYPE_DEFAULT);
	oidnCommitDevice(dev);

	LUA->PushUserType_Value(dev, Device_id);
	return 1;
}

LUA_FUNCTION(Device_NewRaytracingFilter) {
	LUA->CheckType(1, Device_id);

	OIDNDevice dev = *LUA->GetUserType<OIDNDevice>(1, Device_id);
	OIDNFilter filter = oidnNewFilter(dev, "RT");

	LUA->PushUserType_Value(filter, Filter_id);
	return 1;
}

LUA_FUNCTION(Device_GetError) {
	LUA->CheckType(1, Device_id);

	OIDNDevice dev = *LUA->GetUserType<OIDNDevice>(1, Device_id);

	const char* errMsg;
	if (oidnGetDeviceError(dev, &errMsg) != OIDN_ERROR_NONE) {
		LUA->PushString(errMsg);
		return 1;
	}

	LUA->PushNil();
	return 1;
}

LUA_FUNCTION(Device_gc) {
	LUA->CheckType(1, Device_id);

	OIDNDevice dev = *LUA->GetUserType<OIDNDevice>(1, Device_id);
	LUA->SetUserType(1, NULL);

	oidnReleaseDevice(dev);
	return 0;
}

#pragma endregion

#pragma region VisTrace Extension
static int g_IRenderTargetID = 0;

// Remember that this and the OIDN raw api that we created above are NOT connected.
// This is supposed to be a mindless 1 click ez-denoise button with some options for finer control.
// This is why we create certain OIDN objects and then remove them at the end of the function unlike the raw API above.
LUA_FUNCTION(IRenderTarget_Denoise) {
	LUA->CheckType(1, g_IRenderTargetID); // RT
	// 2 and 3 are optional albedo and normal.
	// 4 and 5 are albedoNoisy and normalNoisy
	// We need to check if we have any extra buffers we can use.

	bool albedo_noisy = LUA->GetBool(4);
	bool normal_noisy = LUA->GetBool(5);


	RT::ITexture* color = *LUA->GetUserType<RT::ITexture*>(1, g_IRenderTargetID);
	if (!color->IsValid()) {
		LUA->ThrowError("RenderTarget is invalid!");
	}

	RT::ITexture* albedo = nullptr;
	RT::ITexture* normal = nullptr;

	if (LUA->IsType(2, g_IRenderTargetID)) {
		albedo = *LUA->GetUserType<RT::ITexture*>(2, g_IRenderTargetID);
		if (!albedo->IsValid()) {
			LUA->ThrowError("Albedo RT is invalid!");
		}
	}

	if (LUA->IsType(3, g_IRenderTargetID)) {
		normal = *LUA->GetUserType<RT::ITexture*>(3, g_IRenderTargetID);
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
		if (albedo->GetFormat() != RT::Format::RGBFFF) {
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
			float* albedoBuffer = reinterpret_cast<float*>(albedo->GetRawData());

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
		if (normal->GetFormat() != RT::Format::RGBFFF) {
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
			float* normalBuffer = reinterpret_cast<float*>(normal->GetRawData());

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

	float* colorBuffer = reinterpret_cast<float*>(color->GetRawData());

	OIDNFilter filter = oidnNewFilter(dev, "RT");
	oidnSetSharedFilterImage(filter, "color", colorBuffer, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);

	if (albedo) {
		float* albedoBuffer = reinterpret_cast<float*>(albedo->GetRawData());
		oidnSetSharedFilterImage(filter, "albedo", albedoBuffer, OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
	}

	if (normal) {
		float* normalBuffer = reinterpret_cast<float*>(normal->GetRawData());
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

LUA_FUNCTION(OnVistraceInit) {
	// We add this specific extension API:
	// - RenderTarget:Denoise(bool hdr, bool cleanAux, RenderTarget? albedo, RenderTarget? normal)
	//     - the "color" argument is implicitly the RenderTarget being called with denoise, and this is a self-modifying operation

	LUA->PushSpecial(SPECIAL_REG);
	LUA->GetField(-1, "VisTraceRT_id");
	g_IRenderTargetID = LUA->CheckNumber(-1);
	LUA->Pop(); // Pop off registry

	// If all of those checks failed then we have a valid IRenderTarget metatable id
	bool worked = LUA->PushMetaTable(g_IRenderTargetID);
	if (!worked) {
		LUA->ThrowError("OIDN: No IRenderTarget metatable found within GMod!");
	}

	// Add our own functions
	PUSH_CFUNC(IRenderTarget_Denoise, "Denoise");
	LUA->Pop(1); // Done!!

	return 0;
}
#pragma endregion

GMOD_MODULE_OPEN() {
	Device_id = LUA->CreateMetaTable("OIDNDevice");
		LUA->Push(-1); // Copy ourselves
		LUA->SetField(-2, "__index"); // Device.__index = Device

		PUSH_CFUNC(Device_gc, "__gc");
		PUSH_CFUNC(Device_GetError, "GetError");
		PUSH_CFUNC(Device_NewRaytracingFilter, "NewRaytracingFilter");
	LUA->Pop();

	Filter_id = LUA->CreateMetaTable("OIDNFilter");
		LUA->Push(-1); // Copy ourselves
		LUA->SetField(-2, "__index"); // Filter.__index = Filter

		PUSH_CFUNC(Filter_gc, "__gc");
		PUSH_CFUNC(Filter_Set1b, "Set1b");
		PUSH_CFUNC(Filter_SetSharedImage, "SetSharedImage");
		PUSH_CFUNC(Filter_Execute, "Execute");
		PUSH_CFUNC(Filter_Commit, "Commit");
	LUA->Pop();


	LUA->PushSpecial(SPECIAL_GLOB);
	LUA->CreateTable();
	PUSH_CFUNC(CreateOIDNDevice, "CreateDevice"); 
	LUA->SetField(-2, "oidn");
	LUA->Pop();

	// Setup our VisTrace hook
	LUA->PushSpecial(SPECIAL_GLOB);
	LUA->GetField(-1, "hook");
	LUA->GetField(-1, "Add");
	LUA->PushString("VisTraceInit"); // Type of hook
	LUA->PushString("__GMDENOISER_VT_EXTENSION"); // Name of hook
	LUA->PushCFunction(OnVistraceInit); // hook function
	LUA->Call(3, 0);
	LUA->Pop(2); // Pop hook and SPECIAL_GLOB

	return 0;
}

GMOD_MODULE_CLOSE() {
	return 0;
}
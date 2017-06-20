// WindowsCapturer.cpp : Defines the exported functions for the DLL application.
//

#include "CoreInterface.h"
#include "RenderAPI.h"
#include "TimeTools.h"
#include "Encoder.h"
#include "Muxer.h"
#include <assert.h>

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);
static void UNITY_INTERFACE_API OnRenderEvent(int eventID);

static LogCallback debugLog = NULL;
static std::string debugPath;
static Encoder *encoder = nullptr;
static Muxer *muxer = nullptr;
static RenderAPI* currentAPI = nullptr;
static UnityGfxRenderer deviceType = kUnityGfxRendererNull;
static IUnityInterfaces* unityInterfaces = nullptr;
static IUnityGraphics* unityGraphics = nullptr;

extern "C" {

	void __stdcall SetLogCallback(LogCallback callback)
	{
		debugLog = callback;

		if (debugLog != NULL) debugLog("Initialized debug log callback");
	}

	void __stdcall SetDebugPath(const char* path)
	{
		debugPath = std::string(path);

		char buffer[100];
		snprintf(buffer, 100, "Initialized debug path: %s", debugPath.c_str());

		if(debugLog != NULL ) debugLog(buffer);
	}

	int __stdcall CreateEncoder(const char* videoPath, int codec, int width, int height, int framerate, int bitrate, int iframeInterval) {
		
		CapturingCodec inputCodec = static_cast<CapturingCodec>(codec);
		encoder = new Encoder(std::string(videoPath), inputCodec, bitrate, iframeInterval, true);
		encoder->SetDebugPath(debugPath);
		encoder->SetDebugLog(debugLog);

		return 0;
	}

	int __stdcall DestroyEncoder() {
		if (encoder != nullptr)
		{
			delete encoder;
			encoder = nullptr;

			return 0;
		}

		return -1;
	}

	int __stdcall StartEncoding(int inputWidth, int inputHeight, int outputWidth, int outputHeight, int inputFramerate)
	{
		if (encoder != nullptr)
		{
			return encoder->StartEncoding(inputWidth, inputHeight, outputWidth, outputHeight, inputFramerate);
		}

		return -1;
	}

	int __stdcall StopEncoding()
	{
		if (encoder != nullptr)
		{
			return encoder->StopEncoding();
		}

		return -1;
	}

	int __stdcall EncodeFrames(int maxFrames)
	{
		if (encoder != nullptr)
		{
			return encoder->EncodeFrames(maxFrames);
		}

		return -1;
	}

	int __stdcall CreateMuxer(const char* videoFile, const char* audioFile) {
		muxer = new Muxer(std::string(videoFile), std::string(audioFile));
		muxer->SetDebugPath(debugPath);
		muxer->SetDebugLog(debugLog);

		return 0;
	}

	int __stdcall StartMuxing(const char* outputFile) {
		if (muxer != nullptr) {
			return muxer->startMuxing(std::string(outputFile));
		}

		return -1;
	}

	int __stdcall DestroyMuxer() {
		if (muxer != nullptr) {
			delete muxer;
			muxer = nullptr;
			return 0;
		}

		return -1;
	}

	void __stdcall StartCapturing(int _width, int _height)
	{
		if (currentAPI != NULL)
		{
			currentAPI->SetDebugCallback(debugLog);
			currentAPI->SetDebugPath(debugPath);
			currentAPI->SetExternalEncoder(encoder);

			currentAPI->StartCapturing(_width, _height);
		}
	}

	void  __stdcall StopCapturing()
	{
		if (currentAPI != NULL)
			currentAPI->StopCapturing();
	}

	int64_t __stdcall GetHighresTime()
	{
		return timenow_ms();
	}

	UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
	{
		return OnRenderEvent;
	}

	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* _unityInterfaces)
	{
		unityInterfaces = _unityInterfaces;
		unityGraphics = unityInterfaces->Get<IUnityGraphics>();
		unityGraphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
		OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
	}

	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
	{
		unityGraphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
		OnGraphicsDeviceEvent(kUnityGfxDeviceEventShutdown);
	}
}

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	if (eventType == kUnityGfxDeviceEventInitialize)
	{
		assert(currentAPI == NULL);
		deviceType = unityGraphics->GetRenderer();
		currentAPI = CreateRenderAPI(deviceType);
	}

	if (currentAPI != NULL)
	{
		currentAPI->ProcessDeviceEvent(eventType, unityInterfaces);
	}

	if (eventType == kUnityGfxDeviceEventShutdown)
	{
		delete currentAPI;
		currentAPI = nullptr;
		deviceType = kUnityGfxRendererNull;
	}
}

static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
	if (currentAPI != NULL)
	{
		currentAPI->ReadFrameBuffer();
	}
	else
	{
		if (debugLog != NULL) debugLog("Rendering device not set");
	}
}
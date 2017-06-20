#pragma once

#include "SystemCallbacks.h"
#include "EngineDevice/IUnityGraphics.h"
#include "Encoder.h"
#include <stddef.h>
#include <string>

struct IUnityInterfaces;

class RenderAPI
{
public:
	virtual ~RenderAPI() { }
    
    virtual void SetDebugPath(std::string debugPath) = 0;
    
    virtual void SetDebugCallback(LogCallback debugLog) = 0;
    
    virtual void SetExternalEncoder(Encoder *encoder) = 0;

	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) = 0;
    
    virtual void StartCapturing(int width, int height) = 0;
    
    virtual void StopCapturing() = 0;

	virtual void ReadFrameBuffer() = 0;
};

// Create a graphics API implementation instance for the given API type.
RenderAPI* CreateRenderAPI(UnityGfxRenderer apiType);


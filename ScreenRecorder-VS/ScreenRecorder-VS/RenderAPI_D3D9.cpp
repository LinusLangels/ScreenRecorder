#include <string>
#include <sstream>
#include <fstream>

#ifndef PLATFORMBASE_H
#include "PlatformBase.h"
#endif

#include "RenderAPI.h"
#include "TimeTools.h"
#include <atomic>
#include <d3d9.h>
#include <map>
#include "EngineDevice/IUnityGraphicsD3D9.h"

class RenderAPI_D3D9 : public RenderAPI
{
public:
	RenderAPI_D3D9();
	virtual ~RenderAPI_D3D9();

	virtual void SetDebugCallback(LogCallback debugLog);

	virtual void SetDebugPath(std::string debugPath);

	virtual void SetExternalEncoder(Encoder *encoder);

	virtual void StartCapturing(int width, int height);

	virtual void StopCapturing();

	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);

	virtual void ReadFrameBuffer();

private:
	D3DSURFACE_DESC sd;
	IDirect3DDevice9* _device;
	IDirect3DSurface9*	_pD3DSurf9;
	IDirect3DSurface9* pDestTarget = NULL;
	IDirect3DSurface9* pRenderTarget = NULL;
	BYTE* imageData;
	Encoder *m_encoder;
	LogCallback debugLog = NULL;
	std::string debugPath;
	int currentWidth;
	int currentHeight;
	int64_t startTime;
	int frameCount;
	bool isCapturing;
};

RenderAPI* CreateRenderAPI_D3D9() {

	return new RenderAPI_D3D9();
}

RenderAPI_D3D9::RenderAPI_D3D9() : _device(NULL), _pD3DSurf9(NULL)
{
}

RenderAPI_D3D9::~RenderAPI_D3D9()
{
}

void RenderAPI_D3D9::SetExternalEncoder(Encoder *encoder)
{
	m_encoder = encoder;
}

void RenderAPI_D3D9::ReadFrameBuffer()
{
	if (!isCapturing) return;

	DWORD* surfaceData;
	IDirect3DSurface9* pRenderTargetOne = NULL;

	//Get the render target surface.
	if (FAILED(_device->GetRenderTarget(0, &pRenderTarget)))
	{
		if (debugLog != NULL) debugLog("Faild to get Render Target");
		SAFE_RELEASE(pRenderTarget);
		return;
	}

	pRenderTarget->GetDesc(&sd);

	#pragma region  MSAA or dimensions 
	if (sd.MultiSampleType != D3DMULTISAMPLE_NONE || sd.Width != currentWidth || sd.Height != currentHeight)
	{
		if (FAILED(_device->CreateRenderTarget(currentWidth, currentHeight, sd.Format, D3DMULTISAMPLE_NONE, 0, TRUE, &pRenderTargetOne, NULL)))
		{
			if (debugLog != NULL) debugLog("Faild to create non multisampled surface");
			SAFE_RELEASE(pRenderTarget);
			SAFE_RELEASE(pRenderTargetOne);
			return;
		}

		if (FAILED(_device->StretchRect(pRenderTarget, NULL, pRenderTargetOne, NULL, D3DTEXF_NONE)))
		{
			if (debugLog != NULL) debugLog("Faild to StretchRect");
			SAFE_RELEASE(pRenderTarget);
			SAFE_RELEASE(pRenderTargetOne);
			return;
		}

		pRenderTarget = pRenderTargetOne;
	}
	#pragma endregion 

	#pragma region Creating offscreen
	pRenderTarget->GetDesc(&sd);

	//If the offscreen buffer has already been created, don't create it from scratch. 
	if (pDestTarget == NULL)
	if (FAILED(_device->CreateOffscreenPlainSurface(sd.Width,
		sd.Height,
		sd.Format, D3DPOOL_SYSTEMMEM,
		&pDestTarget,
		NULL)))
	{
		if (debugLog != NULL) debugLog("Faild to create offscreenSurface");
		SAFE_RELEASE(pDestTarget);
		return;
	}
	#pragma endregion 

	if (FAILED(_device->GetRenderTargetData(pRenderTarget, pDestTarget)))
	{
		if (debugLog != NULL) debugLog("Faild to get RenderTargetData.");
		
		return;
	}

	D3DLOCKED_RECT rc;
	int pitch = 0;
	SYSTEMTIME t;

	if (SUCCEEDED(pDestTarget->LockRect(&rc, NULL, 0)))
	{
		surfaceData = (DWORD*)(rc.pBits);

		for (int i = 0; i < sd.Height; i++)
		{
			for (int j = 0; j < sd.Width; j++)
			{
				int widthIndex = i* rc.Pitch / 4 + j;

				DWORD color_ABGR = surfaceData[widthIndex];
				BYTE A = (color_ABGR >> 24) & 0xff;
				BYTE B = (color_ABGR >> 16) & 0xff;
				BYTE G = (color_ABGR >> 8) & 0xff;
				BYTE R = color_ABGR & 0xff;

				int byteIndex = (i*sd.Width + j) * 4;
				
				imageData[byteIndex] = B;
				imageData[byteIndex+1] = G;
				imageData[byteIndex+2] = R;
				imageData[byteIndex+3] = A;
			}
		}

		int64_t timenow = timenow_ms();
		int64_t timeStamp = timenow - startTime;

		if (m_encoder != nullptr)
		{
			m_encoder->InsertFrame(imageData, 4, frameCount == 0 ? 0 : timeStamp);
		}

		frameCount++;

		if (FAILED(pDestTarget->UnlockRect())) 
		{
			if (debugLog != NULL) debugLog("Failed to unlock...");
		}
	}
	else
	{
		if (debugLog != NULL)  debugLog("NOT LOCKABLE");
	}

	// clean up.
	surfaceData = NULL;
	SAFE_RELEASE(pRenderTarget);
	SAFE_RELEASE(pRenderTargetOne);
}

void RenderAPI_D3D9::StartCapturing(int _width, int _height)
{
	currentWidth = _width;
	currentHeight = _height;
	startTime = timenow_ms();
	frameCount = 0;
	isCapturing = true;
	imageData = new BYTE[_width*_height*4];
}

void RenderAPI_D3D9::StopCapturing()
{
	if (imageData != nullptr) {
		delete[] imageData;
		imageData = nullptr;
	}
	
	isCapturing = false;
	SAFE_RELEASE(pDestTarget);
}

void RenderAPI_D3D9::ProcessDeviceEvent(UnityGfxDeviceEventType eventType, IUnityInterfaces* interfaces)
{
	switch (eventType)
	{
		case kUnityGfxDeviceEventInitialize:
		{
			IUnityGraphicsD3D9* d3d = interfaces->Get<IUnityGraphicsD3D9>();
			_device = d3d->GetDevice();
			break;
		}
		case kUnityGfxDeviceEventShutdown:
		{
			break;
		}
	}
}

void RenderAPI_D3D9::SetDebugCallback(LogCallback callback)
{
	debugLog = callback;
}

void RenderAPI_D3D9::SetDebugPath(std::string path)
{
	debugPath = path;
}






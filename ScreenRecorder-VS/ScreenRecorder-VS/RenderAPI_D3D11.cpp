#ifndef PLATFORMBASE_H
#include "PlatformBase.h"
#endif

#include <atomic>
#include <d3d11.h>
#include <map>
#include <atlbase.h>
#include "EngineDevice/IUnityGraphicsD3D11.h"
#include "RenderAPI.h"

class RenderAPI_D3D11 : public RenderAPI
{
public:
	RenderAPI_D3D11();
	virtual ~RenderAPI_D3D11();

	virtual void SetDebugCallback(LogCallback debugLog);

	virtual void SetDebugPath(std::string debugPath);

	virtual void SetExternalEncoder(Encoder *encoder);

	virtual void StartCapturing(int width, int height);

	virtual void StopCapturing();

	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);

	virtual void ReadFrameBuffer();

private:
	ID3D11Device* _device;
	ID3D11DeviceContext* _context;
	Encoder *m_encoder;
	LogCallback debugLog;
	std::string debugPath;
};

RenderAPI* CreateRenderAPI_D3D11() {

	return new RenderAPI_D3D11();
}

RenderAPI_D3D11::RenderAPI_D3D11(): _device(NULL),_context(NULL)
{
}

RenderAPI_D3D11::~RenderAPI_D3D11()
{
}

void RenderAPI_D3D11::ProcessDeviceEvent(UnityGfxDeviceEventType eventType, IUnityInterfaces * interfaces)
{
	switch (eventType)
	{
		case kUnityGfxDeviceEventInitialize:
		{
			IUnityGraphicsD3D11* d3d = interfaces->Get<IUnityGraphicsD3D11>();
			_device = d3d->GetDevice();
			_device->GetImmediateContext(&_context);
			break;
		}
		case kUnityGfxDeviceEventShutdown:
		{
			//Release Resources ...
			SAFE_RELEASE(_context);
			break;
		}
	}
}

void RenderAPI_D3D11::SetExternalEncoder(Encoder *encoder)
{
	m_encoder = encoder;
}

void RenderAPI_D3D11::ReadFrameBuffer()
{
	ID3D11RenderTargetView* pRenderTargetView;
	_context->OMGetRenderTargets(1, &pRenderTargetView, NULL);

	D3D11_RENDER_TARGET_VIEW_DESC renderTargetDesc;
	pRenderTargetView->GetDesc(&renderTargetDesc);

	ID3D11Resource* resource;
	pRenderTargetView->GetResource(&resource);

	D3D11_RENDER_TARGET_VIEW_DESC desc;
	pRenderTargetView->GetDesc(&desc);

	CComPtr<ID3D11Texture2D> stagingResource;

	_context->CopyResource(stagingResource, resource);

	//Create a resource with the STAGING USAGE
	//Copy data from RenderResource to StagingResource
	//Mapp the StagingResource to get the data.
}


void RenderAPI_D3D11::StopCapturing()
{
	

}
void RenderAPI_D3D11::StartCapturing(int _width, int _height)
{

}

void RenderAPI_D3D11::SetDebugCallback(LogCallback callback)
{
	debugLog = callback;
}

void RenderAPI_D3D11::SetDebugPath(std::string path)
{
	debugPath = path;
}

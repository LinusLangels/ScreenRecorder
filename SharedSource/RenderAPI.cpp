#include "RenderAPI.h"
#include "EngineDevice/IUnityGraphics.h"
#include "PlatformBase.h"

RenderAPI * CreateRenderAPI(UnityGfxRenderer apiType)
{
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(_WIN64) || defined(WINAPI_FAMILY)

	if (apiType == kUnityGfxRendererD3D9)
	{
		extern RenderAPI* CreateRenderAPI_D3D9();
		return CreateRenderAPI_D3D9();
	}
	if (apiType == kUnityGfxRendererD3D11)
	{
		extern RenderAPI* CreateRenderAPI_D3D11();
		return CreateRenderAPI_D3D11();
	}

#elif defined(__MACH__) || defined(__ANDROID__) || defined(__linux__) || defined(__QNX__) || defined(__APPLE__) || defined(__APPLE)

#if SUPPORT_OPENGL_UNIFIED
	if (apiType == kUnityGfxRendererOpenGLCore || apiType == kUnityGfxRendererOpenGLES20 || apiType == kUnityGfxRendererOpenGLES30)
	{
		extern RenderAPI* Create_OpenGLCoreCapturer(UnityGfxRenderer apiType);
		return Create_OpenGLCoreCapturer(apiType);
	}
#endif // if SUPPORT_OPENGL_UNIFIED

#if SUPPORT_OPENGL_LEGACY
	if (apiType == kUnityGfxRendererOpenGL)
	{
		extern RenderAPI* Create_OpenGL2Capturer();
		return Create_OpenGL2Capturer();
	}
#endif // if SUPPORT_OPENGL_LEGACY   
#endif
	return NULL;
}

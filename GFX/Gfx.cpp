#define GFX
#include "pch.h"          // ← première ligne, toujours
#include "gfx.h"

extern "C" void CleanScreenASM(__m256i*, int);	// ASM clearScreen
extern "C" void CleanScreenV3(void* p, unsigned long long bytes);




//************************************
void Init_Render()
{


	
}

void Clean_Render(FrameBuffer& fb)
{
	fb.Clear(MakeColor(0, 0, 24));

	//myZBuf->CleandepthBuffer(float(-pFrustum->farClippingPlane));
//	CleanScreenASM((__m256i*)(fb.frameBuffer()), (fb.Width() >> 3) * (fb.Height()));
//	CleanScreenV3(fb.m_Pixels, (fb.m_Width >> 3) * (fb.m_Height));frameBuffer
}


void RenderObject()
{
	


	//DrawCameraOrientation(myFrustum, myWorld, &myScreen);
}


void End_Render()
{

}
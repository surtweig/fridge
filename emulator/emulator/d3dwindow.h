#pragma once

#include <d3d11_1.h>
#include <d3d11_2.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <directxcolors.h>
#include "Windows.h"
#include "DDSTextureLoader.h"
#include "XCM2.h"
#include "XCM2RAM.h"

using namespace DirectX;

struct SurfaceVertex
{
	XMFLOAT3 Pos;
	XMFLOAT2 Tex;
	XMFLOAT2 Color;
};

const UINT PaletteSize = 16;

struct PaletteBuffer
{
	XMFLOAT4 palette[PaletteSize];
};

struct TextureInfo
{
public:
	ID3D11Texture2D* tex;
	ID3D11ShaderResourceView* res;
	ID3D11SamplerState* sampler;
};

const UINT VBufferNumber = 1;
const UINT VTextModeCharWidth = 6;//7;
const UINT VTextModeCharHeight = 8;//14;
const float FontUVw = (float)VTextModeCharWidth / 128.0;//((float)VTextModeCharWidth*31.0 / 256.0) / 31.0f;
const float FontUVh = (float)VTextModeCharHeight / 128.0;//((float)VTextModeCharHeight*16.0 / 256.0) / 16.0f;

class d3dwindow : public IGraphicAdapter
{
public:
	d3dwindow(UINT clientWidth, UINT clientHeight, UINT bufferWidth, UINT bufferHeight, LPCWSTR windowTitle);
	bool update();
	~d3dwindow();
    void setWindowTitle(LPCWSTR windowTitle);
	virtual void switchMode(XCM2_VIDEO_MODE mode) override;
	void rasterClear(BYTE color);
	void rasterSetPixel(UINT x, UINT y, BYTE color);
	BYTE rasterGetPixel(UINT x, UINT y);
	void textModeClear(BYTE color);
	void textModeSetChar(UINT x, UINT y, TextModeCharInfo ch);

	virtual void present() override;
	virtual void readPallette() override;
	virtual XCM2GRAM* back() override;
	virtual XCM2GRAM* sprite() override;

private:
	static LRESULT CALLBACK internal_WndProc(HWND hWnd, int msg, WORD wParam, LONG lParam);
	static HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut);

	LRESULT wndProc(HWND hWnd, int msg, WORD wParam, LONG lParam);
	HRESULT initWindow();
	HRESULT initDevice();
	void render();
	void cleanupDevice();

	void initRaster();
	void initTextMode();
	void initBuffers();
	void createBufferTexture(UINT index);
	void releaseTexture(TextureInfo* texinfo);
	void updateBufferTexture(UINT index);
	void updateSurface();
	void buildSurface();
	WORD* generateSurfaceIndices();

	HINSTANCE hInstance;
	HWND hWnd;
	UINT clientWidth;
	UINT clientHeight;
	UINT bufferWidth;
	UINT bufferHeight;
	UINT bufferSize;
	UINT textModeWidth;
	UINT textModeHeight;
	LPCWSTR title;
	UINT surfaceVCount;
	UINT surfaceICount;
	UINT maxSurfVCount;
	UINT maxSurfICount;

	UINT cycleCounter;
	bool needRebuildSurface;
	XCM2_VIDEO_MODE videoMode;
	XCM2GRAM* backBuffer;
	XCM2GRAM* spriteBuffer;

	D3D_DRIVER_TYPE                     driverType = D3D_DRIVER_TYPE_NULL;
	D3D_FEATURE_LEVEL                   featureLevel = D3D_FEATURE_LEVEL_11_0;
	ID3D11Device*                       pd3dDevice = nullptr;
	ID3D11Device1*                      pd3dDevice1 = nullptr;
	ID3D11DeviceContext*                pImmediateContext = nullptr;
	ID3D11DeviceContext1*               pImmediateContext1 = nullptr;
	IDXGISwapChain*                     pSwapChain = nullptr;
	IDXGISwapChain1*                    pSwapChain1 = nullptr;
	ID3D11RenderTargetView*             pRenderTargetView = nullptr;
	//ID3D11Texture2D*                    pDepthStencil = nullptr;
	//ID3D11DepthStencilView*             pDepthStencilView = nullptr;
	ID3D11InputLayout*                  pVertexLayout = nullptr;
	ID3D11VertexShader*                 pSurfaceVS = nullptr;
	ID3D11PixelShader*                  pSurfacePS = nullptr;
	ID3D11Buffer*                       pSurfaceVertexBuffer = nullptr;
	ID3D11Buffer*                       pSurfaceIndexBuffer = nullptr;
	PaletteBuffer                       paletteBuffer;
	ID3D11Buffer*                       pSurfacePaletteCB = nullptr;
	ID3D11Buffer*                       pSurfaceTransformCB = nullptr;
	XMMATRIX                            World;
	XMMATRIX                            View;
	XMMATRIX                            Projection;

	TextureInfo                         tFont;
	TextureInfo                         tVBuffer[VBufferNumber];
	UINT                                VisibleVBufferIndex = 0;

	BYTE*                               pRasterData;
	TextModeCharInfo*                   pTextModeData;
	SurfaceVertex*                      pSurfaceMesh;
};


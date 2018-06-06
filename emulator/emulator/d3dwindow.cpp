#include "stdafx.h"
#include "d3dwindow.h"
#include "resource.h"
#include "DDSTextureLoader.cpp"
#include <iostream>
#include <comdef.h>

using namespace DirectX;

LRESULT CALLBACK d3dwindow::internal_WndProc(HWND hWnd, int msg, WORD wParam, LONG lParam) {
	d3dwindow *c = (d3dwindow *)GetWindowLong(hWnd, GWLP_USERDATA);

	if (c == NULL)
		return DefWindowProc(hWnd, msg, wParam, lParam);

	return c->wndProc(hWnd, msg, wParam, lParam);
}

d3dwindow::d3dwindow(UINT clientWidth, UINT clientHeight, UINT bufferWidth, UINT bufferHeight, LPCWSTR windowTitle)
{
	this->clientWidth = clientWidth;
	this->clientHeight = clientHeight;
	this->bufferWidth = bufferWidth;
	this->bufferHeight = bufferHeight;
	bufferSize = bufferWidth*bufferHeight;
	title = windowTitle;
	cycleCounter = 0;
	videoMode = XCM2_VIDEO_EGA;

	initWindow();
	initRaster();
	initTextMode();
	initBuffers();

	initDevice();
}

HRESULT d3dwindow::initWindow()
{
	// Register class
	WNDCLASSEX wcex = { };
	ZeroMemory(&wcex, sizeof(WNDCLASSEX));
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = (WNDPROC)internal_WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	hInstance = GetModuleHandle(0);
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, (LPCTSTR)IDI_ICON1);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = L"d3dwindowClass";
	wcex.hIconSm = LoadIcon(wcex.hInstance, (LPCTSTR)IDI_ICON1);
	if (!RegisterClassEx(&wcex))
	{
		std::cout << "[ERROR] Failed to register window class.\n" << std::flush;
		return E_FAIL;
	}

	// Create window
	RECT rc = { 0, 0, clientWidth, clientHeight };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	hWnd = CreateWindow(L"d3dwindowClass", title,
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
		nullptr);
	if (!hWnd)
	{
		std::cout << "[ERROR] Failed to create window. ErrorCode = " << GetLastError() << "\n" << std::flush;
		return E_FAIL;
	}

	SetWindowLong(hWnd, GWLP_USERDATA, (long)this);
	ShowWindow(hWnd, SW_SHOW);

	std::cout << "Video output window created.\n" << std::flush;

	return S_OK;
}

void d3dwindow::setWindowTitle(LPCWSTR windowTitle)
{
    SetWindowText(hWnd, windowTitle);
}

//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DCompile
//
// With VS 11, we could load up prebuilt .cso files instead...
//--------------------------------------------------------------------------------------
HRESULT d3dwindow::CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
	HRESULT hr = S_OK;

	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
	// Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
	// Setting this flag improves the shader debugging experience, but still allows 
	// the shaders to be optimized and to run exactly the way they will run in 
	// the release configuration of this program.
	dwShaderFlags |= D3DCOMPILE_DEBUG;

	// Disable optimizations to further improve shader debugging
	dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ID3DBlob* pErrorBlob = nullptr;
	hr = D3DCompileFromFile(szFileName, nullptr, nullptr, szEntryPoint, szShaderModel,
		dwShaderFlags, 0, ppBlobOut, &pErrorBlob);
	if (FAILED(hr))
	{
		if (pErrorBlob)
		{
			std::cout << "[ERROR] CompileShaderFromFile: " << reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()) << std::flush;
			OutputDebugStringA(reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()));
			pErrorBlob->Release();
		}
		return hr;
	}
	if (pErrorBlob) pErrorBlob->Release();
	std::cout << "Shader " << (_bstr_t)szFileName << " " << szEntryPoint << " compiled successfully.\n" << std::flush;
	return S_OK;
}

HRESULT d3dwindow::initDevice()
{
	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};
	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDevice(nullptr, driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
			D3D11_SDK_VERSION, &pd3dDevice, &featureLevel, &pImmediateContext);

		if (hr == E_INVALIDARG)
		{
			// DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1 so we need to retry without it
			hr = D3D11CreateDevice(nullptr, driverType, nullptr, createDeviceFlags, &featureLevels[1], numFeatureLevels - 1,
				D3D11_SDK_VERSION, &pd3dDevice, &featureLevel, &pImmediateContext);
		}

		if (SUCCEEDED(hr))
			break;
	}
	if (FAILED(hr))
		return hr;

	// Obtain DXGI factory from device (since we used nullptr for pAdapter above)
	IDXGIFactory1* dxgiFactory = nullptr;
	{
		IDXGIDevice* dxgiDevice = nullptr;
		hr = pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgiDevice));
		if (SUCCEEDED(hr))
		{
			IDXGIAdapter* adapter = nullptr;
			hr = dxgiDevice->GetAdapter(&adapter);
			if (SUCCEEDED(hr))
			{
				hr = adapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&dxgiFactory));
				adapter->Release();
			}
			dxgiDevice->Release();
		}
	}
	if (FAILED(hr))
		return hr;

	// Create swap chain
	IDXGIFactory2* dxgiFactory2 = nullptr;
	hr = dxgiFactory->QueryInterface(__uuidof(IDXGIFactory2), reinterpret_cast<void**>(&dxgiFactory2));
	if (dxgiFactory2)
	{
		// DirectX 11.1 or later
		hr = pd3dDevice->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&pd3dDevice1));
		if (SUCCEEDED(hr))
		{
			(void)pImmediateContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&pImmediateContext1));
		}

		DXGI_SWAP_CHAIN_DESC1 sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.Width = width;
		sd.Height = height;
		sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.BufferCount = 1;

		hr = dxgiFactory2->CreateSwapChainForHwnd(pd3dDevice, hWnd, &sd, nullptr, nullptr, &pSwapChain1);
		if (SUCCEEDED(hr))
		{
			hr = pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(&pSwapChain));
		}

		dxgiFactory2->Release();
	}
	else
	{
		// DirectX 11.0 systems
		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory(&sd, sizeof(sd));
		sd.BufferCount = 1;
		sd.BufferDesc.Width = width;
		sd.BufferDesc.Height = height;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = hWnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;

		hr = dxgiFactory->CreateSwapChain(pd3dDevice, &sd, &pSwapChain);
	}

	// Note this tutorial doesn't handle full-screen swapchains so we block the ALT+ENTER shortcut
	dxgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

	dxgiFactory->Release();

	if (FAILED(hr))
		return hr;

	// Create a render target view
	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
	if (FAILED(hr))
		return hr;

	hr = pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr))
		return hr;

	pImmediateContext->OMSetRenderTargets(1, &pRenderTargetView, nullptr);

	// Setup the viewport
	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	pImmediateContext->RSSetViewports(1, &vp);

	// Compile the surface vertex shader
	ID3DBlob* pVSBlob = nullptr;
	hr = CompileShaderFromFile(L"vsurface.fx", "VS", "vs_4_0", &pVSBlob);
	if (FAILED(hr))
	{
		//MessageBox(nullptr,
		//		L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the surface vertex shader
	hr = pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &pSurfaceVS);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	// Define the input layout
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0}
	};
	UINT numElements = ARRAYSIZE(layout);

	// Create the input layout
	hr = pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &pVertexLayout);
	pVSBlob->Release();
	if (FAILED(hr))
		return hr;

	// Set the input layout
	pImmediateContext->IASetInputLayout(pVertexLayout);

	// Compile the pixel shader
	ID3DBlob* pPSBlob = nullptr;
	hr = CompileShaderFromFile(L"vsurface.fx", "PS", "ps_4_0", &pPSBlob);
	if (FAILED(hr))
	{
		//MessageBox(nullptr,
		//	L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK);
		return hr;
	}

	// Create the pixel shader
	hr = pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &pSurfacePS);
	pPSBlob->Release();
	if (FAILED(hr))
		return hr;

	/*SurfaceVertex vertices[] =
	{
		{ XMFLOAT3(-1.0f, -1.0f, 0.5f), XMFLOAT2(0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(-1.0f,  1.0f, 0.5f), XMFLOAT2(0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3( 1.0f,  1.0f, 0.5f), XMFLOAT2(1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3( 1.0f, -1.0f, 0.5f), XMFLOAT2(1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }
	};*/
	maxSurfVCount = textModeWidth * textModeHeight * 4;
	maxSurfICount = textModeWidth * textModeHeight * 6;

	pSurfaceMesh = (SurfaceVertex*)malloc(sizeof(SurfaceVertex) * maxSurfVCount);
	buildSurface();

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(SurfaceVertex) * maxSurfVCount;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = pSurfaceMesh;
	hr = pd3dDevice->CreateBuffer(&bd, &InitData, &pSurfaceVertexBuffer);
	if (FAILED(hr))
		return hr;

	// Set vertex buffer
	UINT stride = sizeof(SurfaceVertex);
	UINT offset = 0;
	pImmediateContext->IASetVertexBuffers(0, 1, &pSurfaceVertexBuffer, &stride, &offset);

	// Create index buffer
	// Create vertex buffer
	/*WORD indices[] =
	{
		0, 1, 2,
		0, 2, 3
	};*/
	WORD* indices = generateSurfaceIndices();

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(WORD) * maxSurfICount;
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	InitData.pSysMem = indices;
	hr = pd3dDevice->CreateBuffer(&bd, &InitData, &pSurfaceIndexBuffer);
	if (FAILED(hr))
		return hr;
	free(indices);

	// Set index buffer
	pImmediateContext->IASetIndexBuffer(pSurfaceIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

	// Set primitive topology
	pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Load font texture
	tFont.tex = nullptr;
	hr = CreateDDSTextureFromFile(pd3dDevice, L"font.dds", nullptr, &tFont.res);
	if (FAILED(hr))
	{
		std::cout << "[ERROR] Cannot load font.dds.\n" << std::flush;
		return hr;
	}

	// Create the sample state
	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	hr = pd3dDevice->CreateSamplerState(&sampDesc, &tFont.sampler);
	if (FAILED(hr))
		return hr;

	// Create buffers texture
	for (UINT index = 0; index < VBufferNumber; index++)
		createBufferTexture(index);

	// Create palette buffer
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(PaletteBuffer);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = pd3dDevice->CreateBuffer(&bd, nullptr, &pSurfacePaletteCB);
	if (FAILED(hr))
		return hr;

	paletteBuffer = { {
		XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f), // black
		XMFLOAT4(0.0f, 0.0f, 0.6f, 1.0f), // blue
		XMFLOAT4(0.0f, 0.6f, 0.0f, 1.0f), // green
		XMFLOAT4(0.0f, 0.6f, 0.6f, 1.0f), // cyan

		XMFLOAT4(0.6f, 0.0f, 0.0f, 1.0f), // red
		XMFLOAT4(0.6f, 0.0f, 0.6f, 1.0f), // magenta
		XMFLOAT4(0.6f, 0.3f, 0.0f, 1.0f), // brown
		XMFLOAT4(0.6f, 0.6f, 0.6f, 1.0f), // light gray

		XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f), // dark gray
		XMFLOAT4(0.3f, 0.3f, 1.0f, 1.0f), // bright blue
		XMFLOAT4(0.3f, 1.0f, 0.3f, 1.0f), // bright green
		XMFLOAT4(0.3f, 1.0f, 1.0f, 1.0f), // bright cyan 

		XMFLOAT4(1.0f, 0.3f, 0.3f, 1.0f), // bright red
		XMFLOAT4(1.0f, 0.3f, 1.0f, 1.0f), // bright magenta
		XMFLOAT4(1.0f, 1.0f, 0.3f, 1.0f), // bright yellow
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), // white
	} };

	pImmediateContext->UpdateSubresource(pSurfacePaletteCB, 0, nullptr, &paletteBuffer, 0, 0);

	return S_OK;
}

void d3dwindow::createBufferTexture(UINT index)
{
	D3D11_TEXTURE2D_DESC td;
	ZeroMemory(&td, sizeof(td));
	td.ArraySize = 1;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	td.Format = DXGI_FORMAT_R8_UNORM;//DXGI_FORMAT_R8_UINT;
	td.Height = bufferHeight;
	td.MipLevels = 1;
	td.MiscFlags = 0;
	td.SampleDesc.Count = 1;
	td.SampleDesc.Quality = 0;
	td.Usage = D3D11_USAGE_DYNAMIC;
	td.Width = bufferWidth;

	D3D11_SUBRESOURCE_DATA srd;
	srd.pSysMem = pRasterData;
	srd.SysMemPitch = bufferWidth;
	srd.SysMemSlicePitch = 0;

	pd3dDevice->CreateTexture2D(&td, &srd, &tVBuffer[index].tex);
	pd3dDevice->CreateShaderResourceView(tVBuffer[index].tex, 0, &tVBuffer[index].res);

	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	pd3dDevice->CreateSamplerState(&sampDesc, &tVBuffer[index].sampler);
}

void d3dwindow::releaseTexture(TextureInfo* texinfo)
{
	if (texinfo->res) texinfo->res->Release();
	if (texinfo->sampler) texinfo->sampler->Release();
	if (texinfo->tex) texinfo->tex->Release();
}

void d3dwindow::updateBufferTexture(UINT index)
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
	pImmediateContext->Map(tVBuffer[index].tex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	BYTE* mappedData = reinterpret_cast<BYTE*>(mappedResource.pData);
	BYTE* rasterPointer = pRasterData;
	for (UINT i = 0; i < bufferHeight; i++)
	{
		memcpy(mappedData, rasterPointer, bufferWidth);
		mappedData += mappedResource.RowPitch;
		rasterPointer += bufferWidth;
	}

	pImmediateContext->Unmap(tVBuffer[index].tex, 0);
}

void d3dwindow::updateSurface()
{
	if (!needRebuildSurface)
		return;

	buildSurface();

	D3D11_MAPPED_SUBRESOURCE mappedResource;
	ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
	pImmediateContext->Map(pSurfaceVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, pSurfaceMesh, sizeof(SurfaceVertex)*surfaceVCount);
	pImmediateContext->Unmap(pSurfaceVertexBuffer, 0);

	needRebuildSurface = false;
}

WORD* d3dwindow::generateSurfaceIndices()
{
	WORD* indices = (WORD*)malloc(sizeof(WORD)*maxSurfICount);

	for (int i = 0; i < maxSurfICount / 6; i++)
	{
		indices[i*6    ] = i*4;
		indices[i*6 + 1] = i*4 + 1;
		indices[i*6 + 2] = i*4 + 2;
		indices[i*6 + 3] = i*4;
		indices[i*6 + 4] = i*4 + 2;
		indices[i*6 + 5] = i*4 + 3;
	}
	return indices;
}

void d3dwindow::buildSurface()
{
	if (videoMode == XCM2_VIDEO_TEXT)
	{
		surfaceVCount = (textModeWidth*textModeHeight * 4);
		surfaceICount = (textModeWidth*textModeHeight * 6);

		float charw = 2.0f / (float)textModeWidth;
		float charh = 2.0f / (float)textModeHeight;

		for (int x = 0; x < textModeWidth; x++)
			for (int y = 0; y < textModeHeight; y++)
			{
				TextModeCharInfo* ch = &pTextModeData[(textModeHeight-y-1)*textModeWidth + x];
				int vi = (y*textModeWidth + x) * 4;
				float vx = 2.0f * ((float)x / (float)textModeWidth) - 1.0f;
				float vy = 2.0f * ((float)y / (float)textModeHeight) - 1.0f;
				float u = (float)(ch->ascii % 16) * FontUVw; // *2.0f
				float v = (float)(ch->ascii / 16) * FontUVh;
				XMFLOAT2 color = XMFLOAT2(1.0 - (float)ch->fcolor / 16.0f, (float)ch->bcolor / 16.0f);

				pSurfaceMesh[vi  ] = { XMFLOAT3(vx,       vy,       0.5f), XMFLOAT2(u,         v+FontUVh), color };
				pSurfaceMesh[vi+1] = { XMFLOAT3(vx,       vy+charh, 0.5f), XMFLOAT2(u,         v        ), color };
				pSurfaceMesh[vi+2] = { XMFLOAT3(vx+charw, vy+charh, 0.5f), XMFLOAT2(u+FontUVw, v        ), color };
				pSurfaceMesh[vi+3] = { XMFLOAT3(vx+charw, vy,       0.5f), XMFLOAT2(u+FontUVw, v+FontUVh), color };
			}
	}
	else if (videoMode == XCM2_VIDEO_EGA)
	{
		pSurfaceMesh[0] = { XMFLOAT3(-1.0f, -1.0f, 0.5f), XMFLOAT2(0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) };
		pSurfaceMesh[1] = { XMFLOAT3(-1.0f,  1.0f, 0.5f), XMFLOAT2(0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) };
		pSurfaceMesh[2] = { XMFLOAT3( 1.0f,  1.0f, 0.5f), XMFLOAT2(1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) };
		pSurfaceMesh[3] = { XMFLOAT3( 1.0f, -1.0f, 0.5f), XMFLOAT2(1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) };

		surfaceVCount = 4;
		surfaceICount = 6;
	}
}

void d3dwindow::switchMode(XCM2_VIDEO_MODE mode)
{
	needRebuildSurface = true;
	videoMode = mode;
}

void d3dwindow::present()
{
	if (videoMode == XCM2_VIDEO_EGA)
	{
		for (UINT i = 0; i < bufferSize; i++)
			pRasterData[i] = backBuffer->loadPixel(i) * PaletteSize;
	}
	else if (videoMode == XCM2_VIDEO_TEXT)
	{
		for (UINT i = 0; i < textModeWidth*textModeHeight; i++)
		{
			// In the Text mode char's ASCII code and its color information are stored in alternating bytes:
			// [8bit ASCII, 4bit background, 4bit foreground], ...
			TextModeCharInfo ch;
			ch.ascii = backBuffer->load(i*2);
			ch.bcolor = backBuffer->loadPixel(i*4 + 2); // pixel position = byte address * 2
			ch.fcolor = backBuffer->loadPixel(i*4 + 3);
			pTextModeData[i] = ch;
			needRebuildSurface = true;
		}
	}
}

void d3dwindow::readPallette()
{

}

void d3dwindow::initBuffers()
{
	backBuffer = new XCM2GRAM();
	spriteBuffer = new XCM2GRAM();
}

XCM2GRAM* d3dwindow::back() { return backBuffer; }
XCM2GRAM* d3dwindow::sprite() { return spriteBuffer; }

void d3dwindow::initRaster()
{
	pRasterData = (BYTE*)malloc(bufferSize);
	ZeroMemory(pRasterData, bufferSize);

	for (int x = 0; x < bufferWidth; x++)
	{
		BYTE color = (x / (bufferWidth / PaletteSize)) * PaletteSize;
		for (int y = 0; y < bufferHeight; y++)
		{
			rasterSetPixel(x, y, color);
		}
	}
}

void d3dwindow::initTextMode()
{
	textModeWidth = bufferWidth / VTextModeCharWidth;
	textModeHeight = bufferHeight / VTextModeCharHeight;

	size_t size = textModeWidth*textModeHeight*sizeof(TextModeCharInfo);
	pTextModeData = (TextModeCharInfo*)malloc(size);
	ZeroMemory(pTextModeData, size);

	char str[34] = "Welcome to XCM2 Fridge           ";
	for (int x = 0; x < textModeWidth; x++)
	{
		for (int y = 0; y < textModeHeight; y++)
		{
			TextModeCharInfo ch;
			ch.ascii = str[x%34];//(BYTE)(y*textModeWidth + x);
			ch.bcolor = y % 8;
			ch.fcolor = 15 - y % 8;
			pTextModeData[y*textModeWidth + x] = ch;
		}
	}
}

void d3dwindow::rasterClear(BYTE color)
{
	for (UINT i = 0; i < bufferSize; i++)
		pRasterData[i] = color;
}

void d3dwindow::rasterSetPixel(UINT x, UINT y, BYTE color)
{
	pRasterData[y*bufferWidth + x] = color;
}

BYTE d3dwindow::rasterGetPixel(UINT x, UINT y)
{
	return pRasterData[y*bufferWidth + x];
}

void d3dwindow::textModeClear(BYTE color)
{
	for (UINT i = 0; i < textModeWidth*textModeHeight; i++)
		pTextModeData[i] = { 0, 0, color };
}

void d3dwindow::textModeSetChar(UINT x, UINT y, TextModeCharInfo ch)
{
	pTextModeData[y*textModeWidth + x] = ch;
}

LRESULT d3dwindow::wndProc(HWND hWnd, int msg, WORD wParam, LONG lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	switch (msg)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool d3dwindow::update()
{
	MSG msg = { 0 };
	if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	else
	{
		render();
		cycleCounter++;
	}

	return (msg.message != WM_QUIT);
}


void d3dwindow::render()
{
	updateBufferTexture(0);
	updateSurface();

	pImmediateContext->ClearRenderTargetView(pRenderTargetView, Colors::Black);

	pImmediateContext->UpdateSubresource(pSurfacePaletteCB, 0, nullptr, &paletteBuffer, 0, 0);

	pImmediateContext->VSSetShader(pSurfaceVS, nullptr, 0);
	pImmediateContext->VSSetConstantBuffers(0, 1, &pSurfacePaletteCB);
	pImmediateContext->PSSetShader(pSurfacePS, nullptr, 0);
	pImmediateContext->PSSetConstantBuffers(0, 1, &pSurfacePaletteCB);
	if (videoMode == XCM2_VIDEO_EGA)
	{
		pImmediateContext->PSSetShaderResources(0, 1, &tVBuffer[VisibleVBufferIndex].res);
		pImmediateContext->PSSetSamplers(0, 1, &tVBuffer[VisibleVBufferIndex].sampler);
	}
	else if (videoMode == XCM2_VIDEO_TEXT)
	{
		pImmediateContext->PSSetShaderResources(0, 1, &tFont.res);
		pImmediateContext->PSSetSamplers(0, 1, &tFont.sampler);
	}
	pImmediateContext->DrawIndexed(surfaceICount, 0, 0);

	pSwapChain->Present(0, 0);
}

void d3dwindow::cleanupDevice()
{
	if (pImmediateContext) pImmediateContext->ClearState();

	releaseTexture(&tFont);
	for (UINT i = 0; i < VBufferNumber; i++)
		releaseTexture(&tVBuffer[i]);

	free(pRasterData);
	free(pTextModeData);

	if (pVertexLayout) pVertexLayout->Release();
	if (pSurfaceVS) pSurfaceVS->Release();
	if (pSurfacePS) pSurfacePS->Release();
	if (pSurfaceVertexBuffer) pSurfaceVertexBuffer->Release();
	if (pSurfaceIndexBuffer) pSurfaceIndexBuffer->Release();
	if (pSurfacePaletteCB) pSurfacePaletteCB->Release();
	if (pSurfaceTransformCB) pSurfaceTransformCB->Release();
	if (pRenderTargetView) pRenderTargetView->Release();
	if (pSwapChain1) pSwapChain1->Release();
	if (pSwapChain) pSwapChain->Release();
	if (pImmediateContext1) pImmediateContext1->Release();
	if (pImmediateContext) pImmediateContext->Release();
	if (pd3dDevice1) pd3dDevice1->Release();
	if (pd3dDevice) pd3dDevice->Release();
}

d3dwindow::~d3dwindow()
{
	cleanupDevice();
}

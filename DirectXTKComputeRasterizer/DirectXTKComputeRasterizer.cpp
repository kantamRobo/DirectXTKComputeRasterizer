#include "pch.h"
#include "DirectXTKComputeRasterizer.h"



void DirectXTKComputeRasterizer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int screenWidth, int screenHeight)
{

	// 1. テクスチャの作成 (UAVとして使えるように BindFlags を設定)
	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = screenWidth;
	texDesc.Height = screenHeight;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	// 重要: SHADER_RESOURCE と UNORDERED_ACCESS を指定
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

	ID3D11Texture2D* pOutputTexture = nullptr;
	device->CreateTexture2D(&texDesc, nullptr, &pOutputTexture);

	// 2. UAVの作成
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = texDesc.Format;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	ID3D11UnorderedAccessView* pUAV = nullptr;
	device->CreateUnorderedAccessView(pOutputTexture, &uavDesc, &pUAV);

	// 3. コンピュートシェーダーのコンパイルと作成
	// (d3dcompiler.h の D3DCompileFromFile 等を使用)
	// ... CreateComputeShader(...)

}


void DirectXTKComputeRasterizer::Render(DX::DeviceResources* DR,ID3D11ShaderResourceView* vertexBufferSRV, ID3D11ShaderResourceView* indexBufferSRV, uint32_t triangleCount, int screenWidth, int screenHeight)
{
	auto device = DR->GetD3DDevice();
	auto context = DR->GetD3DDeviceContext();
	auto swapChain = DR->GetSwapChain();

	// コンテキストにCSを設定
	context->CSSetShader(pComputeShader.Get(), nullptr, 0);

	// UAVをスロット0に設定
	context->CSSetUnorderedAccessViews(0, 1, pUAV.GetAddressOf(), nullptr);

	// Dispatch実行 (スレッドグループ数の計算)
	// シェーダー側で [numthreads(16, 16, 1)] としているので、画面サイズを16で割る
	UINT x = (UINT)ceil(screenHeight / 16.0f);
	UINT y = (UINT)ceil(screenWidth / 16.0f);
	context->Dispatch(x, y, 1);

	// --- 重要: バックバッファへの転送 ---
	// UAVに書き込まれた結果をスワップチェーンのバックバッファにコピーします。
	// ※バックバッファもTexture2Dなので CopyResource が使えます。
	ID3D11Texture2D* pBackBuffer = nullptr;
	swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);

	context->CopyResource(pBackBuffer, pOutputTexture.Get());

	// UAVの解除 (次に使うために重要)
	ID3D11UnorderedAccessView* nullUAV = nullptr;
	context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

	// 表示
	swapChain->Present(1, 0);

	pBackBuffer->Release();
}
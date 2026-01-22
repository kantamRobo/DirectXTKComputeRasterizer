#include "pch.h"
#include "DirectXTKComputeRasterizer.h"
#include <d3dcompiler.h>


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

	
	device->CreateTexture2D(&texDesc, nullptr, &pOutputTexture);

	// 2. UAVの作成
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = texDesc.Format;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	device->CreateUnorderedAccessView(pOutputTexture.Get(), &uavDesc, &pUAV);

	// 3. コンピュートシェーダーのコンパイルと作成
	// (d3dcompiler.h の D3DCompileFromFile 等を使用)
	
	Microsoft::WRL::ComPtr<ID3DBlob> csBlob;
    HRESULT hr = D3DCompileFromFile(
        L"ComputeShader.hlsl", // シェーダーファイルのパス
        nullptr,
        nullptr,
        "CSMain", // エントリーポイント
        "cs_5_0", // シェーダーモデル
        D3DCOMPILE_ENABLE_STRICTNESS,
        0,
        &csBlob,
        nullptr
	);

    //コンピュートシェーダーの作成
    device->CreateComputeShader(
        csBlob->GetBufferPointer(),
        csBlob->GetBufferSize(),
        nullptr,
        &pComputeShader
	);

}


void DirectXTKComputeRasterizer::Render(DX::DeviceResources* DR, ID3D11ShaderResourceView* vertexBufferSRV, ID3D11ShaderResourceView* indexBufferSRV, uint32_t triangleCount, int screenWidth, int screenHeight)
{
    auto device = DR->GetD3DDevice();
    auto context = DR->GetD3DDeviceContext();
    auto swapChain = DR->GetSwapChain();
    auto pbackBuffer = DR->GetRenderTarget();
    // コンテキストにCSを設定
    context->CSSetShader(pComputeShader.Get(), nullptr, 0);

    // UAVをスロット0に設定
    context->CSSetUnorderedAccessViews(0, 1, pUAV.GetAddressOf(), nullptr);

    // Dispatch実行 (スレッドグループ数の計算)
    UINT x = (UINT)ceil(screenHeight / 16.0f);
    UINT y = (UINT)ceil(screenWidth / 16.0f);
    context->Dispatch(x, y, 1);

    // --- 重要: バックバッファへの転送 ---
    ID3D11Texture2D* pBackBuffer = nullptr;
    HRESULT hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);

    if (SUCCEEDED(hr) && pBackBuffer != nullptr)
    {
        context->CopyResource(pBackBuffer, pOutputTexture.Get());
        pBackBuffer->Release();
    }
    else
    {
        // エラー処理: バックバッファ取得失敗時
        // 必要に応じてログ出力や例外処理を追加
    }

    // UAVの解除
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);

    // 表示
    swapChain->Present(1, 0);
}
#include "pch.h"
#include "DirectXTKComputeRasterizer.h"
#include <d3dcompiler.h>

void DirectXTKComputeRasterizer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int screenWidth, int screenHeight, DXGI_FORMAT format)
{
    OutputDebugStringA("=== DirectXTKComputeRasterizer::Initialize START ===\n");
    
    // CommonStatesの初期化
    commonstate = std::make_unique<DirectX::CommonStates>(device);

    // 1. テクスチャの作成
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = screenWidth;
    texDesc.Height = screenHeight;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    HRESULT hr = device->CreateTexture2D(&texDesc, nullptr, &pOutputTexture);
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create output texture\n");
        throw std::runtime_error("Failed to create output texture");
    }
    OutputDebugStringA("Output texture created successfully\n");

    // 2. UAVの作成
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = texDesc.Format;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;

    hr = device->CreateUnorderedAccessView(pOutputTexture.Get(), &uavDesc, &pUAV);
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create UAV\n");
        throw std::runtime_error("Failed to create UAV");
    }
    OutputDebugStringA("UAV created successfully\n");

    // 3. Constant Bufferの作成
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(CBData);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = device->CreateBuffer(&cbDesc, nullptr, &pConstantBuffer);
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create constant buffer\n");
        throw std::runtime_error("Failed to create constant buffer");
    }
    OutputDebugStringA("Constant buffer created successfully\n");

    // 4. コンピュートシェーダーのコンパイルと作成
    Microsoft::WRL::ComPtr<ID3DBlob> csBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    hr = D3DCompileFromFile(
        L"TriangleRasterizer.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "CSMain",
        "cs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_DEBUG,
        0,
        &csBlob,
        &errorBlob
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA("Shader compilation failed:\n");
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        throw std::runtime_error("Shader compilation failed");
    }
    OutputDebugStringA("Shader compiled successfully\n");

    hr = device->CreateComputeShader(
        csBlob->GetBufferPointer(),
        csBlob->GetBufferSize(),
        nullptr,
        &pComputeShader
    );

    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create compute shader\n");
        throw std::runtime_error("Failed to create compute shader");
    }
    OutputDebugStringA("Compute shader created successfully\n");

    // 5. テスト用の三角形を作成
    CreateTestTriangle(device);

    // 6. フォールバック用の白テクスチャを作成
    CreateFallbackTexture(device);
    
    OutputDebugStringA("=== DirectXTKComputeRasterizer::Initialize END ===\n");
}

void DirectXTKComputeRasterizer::CreateTestTriangle(ID3D11Device* device)
{
    OutputDebugStringA("=== CreateTestTriangle START ===\n");
    
    // テスト用の三角形データ（NDC座標系で画面中央に表示）
    // 1つの三角形 = 3頂点
    Vertex triangleVertices[] = {
        // 位置(x, y, z)                   色(r, g, b, a)              UV(u, v)
        { DirectX::XMFLOAT3(0.0f, 0.5f, 0.5f),   DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f), DirectX::XMFLOAT2(0.5f, 0.0f) },  // 上
        // ▼ ここを入れ替え（左下を先に定義）
        { DirectX::XMFLOAT3(-0.5f, -0.5f, 0.5f), DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f), DirectX::XMFLOAT2(0.0f, 1.0f) },  // 左下
        { DirectX::XMFLOAT3(0.5f, -0.5f, 0.5f),  DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f) },  // 右下
    };

    m_testTriangleCount = 1; // 1つの三角形

    // StructuredBufferの作成
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = sizeof(triangleVertices);
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bufferDesc.StructureByteStride = sizeof(Vertex);

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = triangleVertices;

    HRESULT hr = device->CreateBuffer(&bufferDesc, &initData, &pTestVertexBuffer);
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create vertex buffer\n");
        throw std::runtime_error("Failed to create vertex buffer");
    }
    OutputDebugStringA("Vertex buffer created successfully\n");

    // ShaderResourceViewの作成
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = 3; // 3頂点

    hr = device->CreateShaderResourceView(pTestVertexBuffer.Get(), &srvDesc, &pTestVertexBufferSRV);
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create vertex buffer SRV\n");
        throw std::runtime_error("Failed to create vertex buffer SRV");
    }
    OutputDebugStringA("Vertex buffer SRV created successfully\n");
    OutputDebugStringA("=== CreateTestTriangle END ===\n");
}

void DirectXTKComputeRasterizer::CreateFallbackTexture(ID3D11Device* device)
{
    if (pFallbackTextureSRV)
    {
        return;
    }

    OutputDebugStringA("=== CreateFallbackTexture START ===\n");

    static const uint32_t whitePixel = 0xFFFFFFFF;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = 1;
    desc.Height = 1;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = &whitePixel;
    initData.SysMemPitch = sizeof(uint32_t);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device->CreateTexture2D(&desc, &initData, &texture);
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create fallback texture\n");
        throw std::runtime_error("Failed to create fallback texture");
    }

    hr = device->CreateShaderResourceView(texture.Get(), nullptr, &pFallbackTextureSRV);
    if (FAILED(hr))
    {
        OutputDebugStringA("Failed to create fallback texture SRV\n");
        throw std::runtime_error("Failed to create fallback texture SRV");
    }

    OutputDebugStringA("Fallback texture created successfully\n");
    OutputDebugStringA("=== CreateFallbackTexture END ===\n");
}

void DirectXTKComputeRasterizer::Render(DX::DeviceResources* DR, ID3D11ShaderResourceView* vertexBufferSRV, ID3D11ShaderResourceView* indexBufferSRV, uint32_t triangleCount, int screenWidth, int screenHeight)
{
    OutputDebugStringA("=== Render START ===\n");
    
    auto device = DR->GetD3DDevice();
    auto context = DR->GetD3DDeviceContext();
    auto swapChain = DR->GetSwapChain();
    auto samplerState = commonstate->LinearWrap();

    // 引数でvertexBufferSRVが指定されていない場合は、テスト用の三角形を使用
    if (vertexBufferSRV == nullptr && pTestVertexBufferSRV != nullptr)
    {
        vertexBufferSRV = pTestVertexBufferSRV.Get();
        triangleCount = m_testTriangleCount;
        OutputDebugStringA("Using test triangle\n");
    }

    // Constant Bufferの更新
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = context->Map(pConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr))
    {
        CBData* cbData = reinterpret_cast<CBData*>(mapped.pData);

        // 単位行列を設定 (NDC座標系で描画)
        cbData->worldViewProj = DirectX::XMMatrixIdentity();
        cbData->screenSize = DirectX::XMFLOAT2(static_cast<float>(screenWidth), static_cast<float>(screenHeight));
        cbData->triangleCount = triangleCount;
        cbData->padding = 0.0f;

        context->Unmap(pConstantBuffer.Get(), 0);

        char debugMsg[256];
        sprintf_s(debugMsg, "ScreenSize: %.0f x %.0f, TriangleCount: %u\n", 
                  static_cast<float>(screenWidth), static_cast<float>(screenHeight), triangleCount);
        OutputDebugStringA(debugMsg);
    }

    // コンピュートシェーダーとリソースの設定
    context->CSSetShader(pComputeShader.Get(), nullptr, 0);
    context->CSSetConstantBuffers(0, 1, pConstantBuffer.GetAddressOf());
    context->CSSetSamplers(0, 1, &samplerState);

    // 頂点バッファの設定
    if (vertexBufferSRV != nullptr)
    {
        context->CSSetShaderResources(0, 1, &vertexBufferSRV);
        OutputDebugStringA("Vertex buffer SRV set\n");
    }

    if (pFallbackTextureSRV)
    {
        ID3D11ShaderResourceView* baseTextureSRV = pFallbackTextureSRV.Get();
        context->CSSetShaderResources(1, 1, &baseTextureSRV);
        OutputDebugStringA("Fallback texture SRV set\n");
    }

    // UAVをスロット0に設定
    context->CSSetUnorderedAccessViews(0, 1, pUAV.GetAddressOf(), nullptr);
    OutputDebugStringA("UAV set\n");

    // Dispatch実行
    UINT x = (UINT)ceil(screenWidth / 16.0f);
    UINT y = (UINT)ceil(screenHeight / 16.0f);
    
    char dispatchMsg[256];
    sprintf_s(dispatchMsg, "Dispatching: %u x %u thread groups\n", x, y);
    OutputDebugStringA(dispatchMsg);
    
    context->Dispatch(x, y, 1);
    OutputDebugStringA("Dispatch completed\n");

    // UAVのアンバインド (重要: CopyResourceの前に必須)
    ID3D11UnorderedAccessView* nullUAV = nullptr;
    context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);
    OutputDebugStringA("UAV unbound\n");

    // バックバッファへの転送
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);

    if (SUCCEEDED(hr) && pBackBuffer != nullptr)
    {
        OutputDebugStringA("Copying to back buffer...\n");
        context->CopyResource(pBackBuffer, pOutputTexture.Get());
        pBackBuffer->Release();
        OutputDebugStringA("Copy completed\n");
    }
    else
    {
        OutputDebugStringA("Failed to get back buffer\n");
    }

    // リソースのクリーンアップ
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ID3D11Buffer* nullCB = nullptr;

    context->CSSetShaderResources(0, 1, &nullSRV);
    context->CSSetShaderResources(1, 1, &nullSRV);
    context->CSSetConstantBuffers(0, 1, &nullCB);
    context->CSSetShader(nullptr, nullptr, 0);
    
    OutputDebugStringA("=== Render END ===\n");
}
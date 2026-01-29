#pragma once
#include <DirectXMath.h>
#include <DeviceResources.h>
#include <wrl.h>
#include <CommonStates.h>

// 頂点構造体
struct Vertex {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT2 uv;
};

// 定数バッファ構造体 (16byte境界に注意)
struct CBData {
    DirectX::XMMATRIX worldViewProj;
    DirectX::XMFLOAT2 screenSize;
    uint32_t triangleCount;
    float    padding;
};

class DirectXTKComputeRasterizer
{
public:
    DirectXTKComputeRasterizer() {};
    ~DirectXTKComputeRasterizer() {};
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int screenWidth, int screenHeight, DXGI_FORMAT format);
    void CreateTestTriangle(ID3D11Device* device);
    void CreateFallbackTexture(ID3D11Device* device);

    void Render(DX::DeviceResources* DR, ID3D11ShaderResourceView* vertexBufferSRV, ID3D11ShaderResourceView* indexBufferSRV, uint32_t triangleCount, int screenWidth, int screenHeight);
   
    Microsoft::WRL::ComPtr<ID3D11Texture2D> pOutputTexture = nullptr;
    Microsoft::WRL::ComPtr<ID3D11ComputeShader> pComputeShader;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> pUAV;
    Microsoft::WRL::ComPtr<ID3D11Buffer> pConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> pTestVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pTestVertexBufferSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pFallbackTextureSRV;
    std::unique_ptr<DirectX::CommonStates> commonstate;
    
    uint32_t m_testTriangleCount = 0;
};


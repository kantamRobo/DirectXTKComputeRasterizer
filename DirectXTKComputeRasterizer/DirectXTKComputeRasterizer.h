#pragma once
#include <DirectXMath.h>
#include <DeviceResources.h>
#include <wrl.h>
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
    DirectXTKComputeRasterizer();
    ~DirectXTKComputeRasterizer();
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int screenWidth, int screenHeight);

    void Render(DX::DeviceResources* DR, ID3D11ShaderResourceView* vertexBufferSRV, ID3D11ShaderResourceView* indexBufferSRV, uint32_t triangleCount, int screenWidth, int screenHeight);
   
    Microsoft::WRL::ComPtr <ID3D11Texture2D> pOutputTexture = nullptr;
	Microsoft::WRL::ComPtr<ID3D11ComputeShader> pComputeShader;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> pUAV;
};


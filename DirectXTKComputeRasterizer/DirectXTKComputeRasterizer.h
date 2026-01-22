#pragma once
#include <DirectXMath.h>
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
	void Render(ID3D11ShaderResourceView* vertexBufferSRV, ID3D11ShaderResourceView* indexBufferSRV, uint32_t triangleCount);
};


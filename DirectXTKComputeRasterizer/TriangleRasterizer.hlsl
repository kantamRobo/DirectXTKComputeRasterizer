// ==================================================================================
// TriangleRasterizer.hlsl
// ==================================================================================

// --- リソース定義 ---

// 出力先: バックバッファへ転送するためのテクスチャ
RWTexture2D<float4> OutputTexture : register(u0);

// 入力: 頂点データ (StructuredBuffer)
struct Vertex {
    float3 pos;   // ローカル座標
    float4 color; // 頂点カラー
    float2 uv;    // UV座標
};
StructuredBuffer<Vertex> VertexBuffer : register(t0);

// 入力: テクスチャとサンプラー
Texture2D<float4> BaseTexture : register(t1);
SamplerState BaseSampler : register(s0);

// 定数バッファ: 行列と画面情報
cbuffer ConstantBuffer : register(b0)
{
    matrix WorldViewProj; // Local -> Clip 行列
    float2 ScreenSize;    // 画面解像度 (Width, Height)
    uint TriangleCount;   // 描画する三角形の枚数
    float Padding;
}

// --- ユーティリティ関数 ---

// エッジ関数: ベクトル(a->b)に対して点cが右側か左側かを判定
// 戻り値: 正なら内側、負なら外側（巻き順による）
float EdgeFunction(float2 a, float2 b, float2 c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

// --- メイン関数 ---
[numthreads(16, 16, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // 現在処理中のピクセル座標 (中心)
    float2 p = float2(dispatchThreadID.x, dispatchThreadID.y) + 0.5f;

    // 画面外チェック
    if (p.x >= ScreenSize.x || p.y >= ScreenSize.y) return;

    // 深度バッファの初期値 (1.0 = 最も奥)
    float bestDepth = 1.0f;
    // 背景色 (クリアカラー)
    float4 bestColor = float4(0.1f, 0.1f, 0.15f, 1.0f);

    // ----------------------------------------------------------------
    // 全三角形ループ (ピクセル主導レンダリング)
    // ----------------------------------------------------------------
    for (uint i = 0; i < TriangleCount; ++i)
    {
        // 頂点データの取得
        uint idx = i * 3;
        Vertex v0_raw = VertexBuffer[idx];
        Vertex v1_raw = VertexBuffer[idx + 1];
        Vertex v2_raw = VertexBuffer[idx + 2];

        // 1. 頂点変換 (Local -> Clip Space)
        float4 c0 = mul(float4(v0_raw.pos, 1.0f), WorldViewProj);
        float4 c1 = mul(float4(v1_raw.pos, 1.0f), WorldViewProj);
        float4 c2 = mul(float4(v2_raw.pos, 1.0f), WorldViewProj);

        // 2. パースペクティブ補正の準備 (1/W を計算)
        // W成分はカメラからの深度情報を含みます
        float invW0 = 1.0f / c0.w;
        float invW1 = 1.0f / c1.w;
        float invW2 = 1.0f / c2.w;

        // 3. スクリーン座標への変換 (Viewport Transform)
        // NDC (-1~1) -> Screen (0~w, 0~h)
        float2 s0, s1, s2;
        s0.x = (c0.x * invW0 + 1.0f) * 0.5f * ScreenSize.x;
        s0.y = (1.0f - c0.y * invW0) * 0.5f * ScreenSize.y; // Y反転

        s1.x = (c1.x * invW1 + 1.0f) * 0.5f * ScreenSize.x;
        s1.y = (1.0f - c1.y * invW1) * 0.5f * ScreenSize.y;

        s2.x = (c2.x * invW2 + 1.0f) * 0.5f * ScreenSize.x;
        s2.y = (1.0f - c2.y * invW2) * 0.5f * ScreenSize.y;

        // 4. ラスタライズ判定 (エッジ関数)
        float area = EdgeFunction(s0, s1, s2);

        // バックフェイスカリング (反時計回りを正とする場合、負なら裏面)
        if (area <= 0) continue;

        float w0 = EdgeFunction(s1, s2, p);
        float w1 = EdgeFunction(s2, s0, p);
        float w2 = EdgeFunction(s0, s1, s2);

        // 5. 三角形の内外判定
        if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
            // 重心座標の正規化
            w0 /= area;
            w1 /= area;
            w2 /= area;

            // 6. 深度テスト (Z値の線形補間)
            // NDCのZ (c.z / c.w) を補間するのが一般的ですが、
            // ここでは簡易的に W の逆数を使って深度判定します (1/Wが大きい=Wが小さい=手前)
            float interpolatedInvW = w0 * invW0 + w1 * invW1 + w2 * invW2;
            float currentW = 1.0f / interpolatedInvW;

            // 深度バッファ更新チェック (Wが小さい方が手前)
            // ※NDC深度(0~1)を使う場合は z < bestDepth
            // ここでは簡易的なZテストとして比較
            float currentDepth = c0.z * invW0 * w0 + c1.z * invW1 * w1 + c2.z * invW2 * w2;
            currentDepth *= currentW; // 復元

            if (currentDepth < bestDepth) {
                bestDepth = currentDepth;

                // 7. パースペクティブ・コレクト補間 (重要)
                // UVやColorは直接 w0,w1,w2 で補間すると歪むため、
                // 一度 (Value / W) を補間し、最後に W を掛けて復元する。

                // UVの補間
                float2 uv0_p = v0_raw.uv * invW0;
                float2 uv1_p = v1_raw.uv * invW1;
                float2 uv2_p = v2_raw.uv * invW2;

                float2 finalUV = (w0 * uv0_p + w1 * uv1_p + w2 * uv2_p) * currentW;

                // Colorの補間
                float4 col0_p = v0_raw.color * invW0;
                float4 col1_p = v1_raw.color * invW1;
                float4 col2_p = v2_raw.color * invW2;

                float4 finalVertexColor = (w0 * col0_p + w1 * col1_p + w2 * col2_p) * currentW;

                // テクスチャサンプリング
                float4 texColor = BaseTexture.SampleLevel(BaseSampler, finalUV, 0);

                // 最終カラー決定
                bestColor = finalVertexColor * texColor;
            }
        }
    }

    // 結果書き込み
    OutputTexture[dispatchThreadID.xy] = bestColor;
}
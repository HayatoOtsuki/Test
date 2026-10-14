// =============================================================================
// Background.hlsl — 背景 (空と床) を全画面に描く
//
//   VSMain: 頂点バッファを使わず、 頂点の番号 (0, 1, 2) から画面を覆う三角形を作る
//   PSMain: 画素ごとにカメラからの光線を作り、 その先の色 (Environment) を塗る
// =============================================================================

// カメラ (InvViewProj, CameraPos) とライトは、 シーンの定数 (b0) から読む。
// Environment.hlsli が SceneConstants.hlsli を #include している
#include "Environment.hlsli"

struct VSOutput
{
    float4 position : SV_Position;
    float2 ndc      : TEXCOORD0;  // 正規化デバイス座標 (画面の左下が (-1, -1)、 右上が (1, 1))
};

// 頂点の番号 id から、 画面を覆う大きな三角形を作る。
//
//   id | uv = ((id << 1) & 2, id & 2) | 位置 (クリップ空間)
//   ---+-----------------------------+-------------------
//    0 | (0, 0)                      | (-1,  1)  左上
//    1 | (2, 0)                      | ( 3,  1)  右の外
//    2 | (0, 2)                      | (-1, -3)  下の外
//
// 画面 (x, y とも -1〜1 の四角) はこの三角形にすっぽり入り、 はみ出た所は切り捨てられる。
// 四角形 (三角形 2 枚) にしないのは、 対角線の継ぎ目で、 GPU が 2×2 画素の組を両方の三角形で
// 計算する無駄を避けるため。
VSOutput VSMain(uint id : SV_VertexID)
{
    float2 uv = float2((id << 1) & 2, id & 2);
    VSOutput output;
    output.ndc = uv * float2(2.0, -2.0) + float2(-1.0, 1.0);
    output.position = float4(output.ndc, 0.0, 1.0);
    return output;
}

// 正規化デバイス座標 ndc の画素に塗る色 (表示用)
float3 BackgroundColor(float2 ndc)
{
    // 画素を奥の面 (クリップ空間の z = 1) まで延ばした点を、 ワールドへ戻す。
    // w で割ると実際の位置になる (透視投影で掛かった w を外す)。 カメラからその点への向きが光線
    float4 farPoint = mul(float4(ndc, 1.0, 1.0), InvViewProj);
    float3 rd = normalize(farPoint.xyz / farPoint.w - CameraPos);
    return ToDisplay(Environment(CameraPos, rd));
}

float4 PSMain(VSOutput input) : SV_Target
{
    return float4(BackgroundColor(input.ndc), 1.0);
}

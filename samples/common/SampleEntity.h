#pragma once
// =============================================================================
// SampleEntity.h — 簡易サンプルの基底クラス
//
// エンジンの RenderEntity (画面に描く物) に、 サンプルで使う 3 つを足しただけ。
//   ・onKey     : キーが押された (SampleApp が使わないキーだけが届く)
//   ・getStatus : ウィンドウの題名に出す、 今の状態 (例: L"形: 球")
//   ・getParts  : 自分のほかに描く部品 (氷 1 つ = RenderEntity 1 つ、 のように並べるとき)
//
// サンプルはこれを継承し、 RenderEntity の 3 つの関数 (initializeImplement / drawImplement / reloadImplement) と、
// 必要ならこの 2 つを書く。
// =============================================================================

#include "engine/RenderEntity.h"

#include <windows.h>

#include <string>
#include <vector>

class SampleEntity : public RenderEntity {
public:
    // キーが押された (仮想キーコード)。 使わないキーは無視してよい
    virtual void onKey(WPARAM key) { static_cast<void>(key); }

    // ウィンドウの題名に出す、 今の状態。 空なら出さない
    virtual std::wstring getStatus() const { return L""; }

    // 自分のほかに Renderer に登録する部品。 SampleApp が、 自分の後にこの順で登録する (= この順で描かれる)
    virtual std::vector<RenderEntity*> getParts() { return {}; }
};

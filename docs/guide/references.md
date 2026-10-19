# 参考文献と利用条件

確認日: 2026-10-15 (各文献の公式のページ・公式のリポジトリの LICENSE で確認)。 確認できなかった所は 「未確認」 と書いた。 利用の前に、 リンク先で最新の条件を確かめること。

## このリポジトリでの扱い

- **このリポジトリのコード・文書・図は、 すべて独自に書いた**。 下の文献のコードや本文・図は写していない。
- 文献は、 **理論と式を確かめるため** に使った。 式 (数学的な事実) を自分の言葉と記法で書き直して実装するのは、 どの文献でも問題ない。
- そのため、 第三者のライセンスの表示 (THIRD_PARTY_NOTICES) は要らない。 **もし後で文献のコードを写すなら**、 下の 「コードを写して商用」 の条件に従い、 元の著作権表示とライセンスの全文を同梱する。

## 文献

### 光線と SDF

| 文献 | 正規の入手先 | 使った所 | 利用条件 | 読む・式を使う | コードを写して商用 |
| --- | --- | --- | --- | --- | --- |
| John C. Hart, "Sphere Tracing: A Geometric Method for the Antialiased Ray Tracing of Implicit Surfaces", The Visual Computer 12(10), 1996 | DOI [10.1007/s003710050084](https://doi.org/10.1007/s003710050084) (Springer。 購入か、 大学などの購読で読む) | 球追跡 (step4、 [05](../05-generate-sdf.md) §5.4) | 出版社の著作権。 オープンアクセスではない | 可 (DOI で引用する) | 対象なし (論文の本文・図の転載は不可) |
| Inigo Quilez, 記事 "distance functions", "normals for an SDF", "raymarching SDFs" | [iquilezles.org/articles](https://iquilezles.org/articles/) | 箱・球の距離、 CSG、 中心差分の法線の考え方 ([05](../05-generate-sdf.md) §2, §5.6) | 記事の一覧のページに、 技術的なコードの断片は MIT と書かれている (作品としてのシェーダーは別の扱い) | 可 | 可 (MIT: 著作権表示と許諾文を同梱)。 作品は不可 |

### 反射・屈折・PBR

| 文献 | 正規の入手先 | 使った所 | 利用条件 | 読む・式を使う | コードを写して商用 |
| --- | --- | --- | --- | --- | --- |
| Christophe Schlick, "An Inexpensive BRDF Model for Physically-based Rendering", Computer Graphics Forum 13(3), 1994 | DOI [10.1111/1467-8659.1330233](https://doi.org/10.1111/1467-8659.1330233) (Wiley)、 [Eurographics Digital Library](https://diglib.eg.org/handle/10.2312/6905) | Fresnel の近似 ([03](../03-ice-component.md) §9.4, [06](../06-ice-optics.md) §2) | 出版社の著作権 (Wiley の通常の条件)。 Wiley のページは直接は未確認 | 可 | 対象なし |
| Bruce Walter ほか, "Microfacet Models for Refraction through Rough Surfaces", EGSR 2007 | 著者のページ [cs.cornell.edu/~srm/publications/EGSR07-btdf.html](https://www.cs.cornell.edu/~srm/publications/EGSR07-btdf.html) (著者が公開) | GGX の分布 ([03](../03-ice-component.md) §9.2) | © The Eurographics Association 2007 | 可 | 対象なし |
| Brian Karis, "Real Shading in Unreal Engine 4", SIGGRAPH 2013 コース | [コースのページ](https://blog.selfshadow.com/publications/s2013-shading-course/) | 金属・粗さの流儀、 粗さの 2 乗、 遮蔽の k ([03](../03-ice-component.md) §9) | 利用許諾の表記なし (通常の著作権) | 可 | **不可** (ノートのコードは写さず、 式から自分で書く) |
| Brent Burley, "Physically-Based Shading at Disney", SIGGRAPH 2012 コース | [Disney Animation の公開ページ](https://disneyanimation.com/publications/physically-based-shading-at-disney/) | 金属・粗さの考え方 | 利用許諾の表記なし (通常の著作権) | 可 | 不可 |
| Romain Guy, Mathias Agopian, "Physically Based Rendering in Filament" | [google.github.io/filament/Filament.html](https://google.github.io/filament/Filament.html) | PBR の式の確かめ | 文書のソースがあるリポジトリ (google/filament) は Apache-2.0。 文書の本文に当てはまるかは、 リポジトリの表示からの推定 | 可 | 条件つきで可 (Apache-2.0: ライセンスの同梱・表示の保持・変更の明示)。 写すならリポジトリのファイルから |
| Matt Pharr, Wenzel Jakob, Greg Humphreys, "Physically Based Rendering: From Theory to Implementation" 第 4 版 | [pbr-book.org/4ed](https://pbr-book.org/4ed/contents.html) (全文が無料で公式に公開) | 鏡面の反射と透過 (スネル・Fresnel・全反射)、 透過率 (Beer の法則)、 関与媒質 ([06](../06-ice-optics.md), [07](../07-ice-bubbles.md)) | 本文 CC BY-NC-ND 4.0、 図 CC BY-NC-SA 4.0 (序文に記載)。 プログラム pbrt-v4 は Apache-2.0 | 可 | **本文中のコードは不可** (非営利・改変禁止)。 同じコードを pbrt-v4 のリポジトリから Apache-2.0 で使うのは可 |
| Tomas Akenine-Möller ほか, "Real-Time Rendering" 第 4 版 | [realtimerendering.com](https://www.realtimerendering.com/) (本は購入する。 一部の章は公式に無料公開) | 第 9 章 (物理ベースのシェーディング)、 第 14 章 (半透明と関与媒質) の確かめ | 商業出版物 | 可 | 対象なし (本文・図の転載は不可) |

### 氷の値

| 文献 | 正規の入手先 | 使った所 | 利用条件 | 読む・式を使う | データを使う |
| --- | --- | --- | --- | --- | --- |
| Stephen G. Warren, Richard E. Brandt, "Optical constants of ice from the ultraviolet to the microwave: A revised compilation", J. Geophys. Res. 113, D14220, 2008 | DOI [10.1029/2007JD009744](https://doi.org/10.1029/2007JD009744)、 著者側の公開 [atmos.washington.edu/ice_optical_constants](https://atmos.washington.edu/ice_optical_constants/) | 屈折率 1.31、 吸収の係数の実測 ([06](../06-ice-optics.md) §4.3) | 論文は出版社の著作権。 著者側のページに利用条件の記載なし | 可 (値は事実として使える。 出典を書く) | 可 (下の refractiveindex.info の CC0 のデータとしても入手できる) |
| refractiveindex.info のデータベース (M. N. Polyanskiy, Sci. Data 11, 94, 2024) | [GitHub: polyanskiy/refractiveindex.info-database](https://github.com/polyanskiy/refractiveindex.info-database) | Warren 2008 の氷のデータ (`main/H2O/nk/Warren-2008.yml`) | データは CC0 1.0 (商用も許可不要と明記)。 ウェブサイトの本文と見た目は別 (CC0 ではない) | 可 | 可 (CC0) |

### 表示

| 文献 | 正規の入手先 | 使った所 | 利用条件 | 読む・式を使う | コードを写して商用 |
| --- | --- | --- | --- | --- | --- |
| Krzysztof Narkowicz, "ACES Filmic Tone Mapping Curve", 2016 | [著者のブログ](https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/) | トーンマップの近似式 (`ToDisplay`) | 記事の中のコードは CC0 または MIT で自由に使えると明記 | 可 | 可 |

> この式は ACES の近似で、 ACES そのものではない。 発表などでは 「ACES 準拠」 ではなく **「ACES の近似 (Narkowicz 2016)」** と書く。

### コードつきの読み物 (サンプル 06 で追加、 確認日 2026-10-19)

論文だけではコードとのつながりが分かりにくいので、 コードつきで説明している物を足した。 どれも **読んで考え方を使うだけ** で、 コードは写していない。

| 文献 | 正規の入手先 | 使った所 | 利用条件 | 読む・式を使う | コードを写して商用 |
| --- | --- | --- | --- | --- | --- |
| Patricio Gonzalez Vivo, Jen Lowe, "The Book of Shaders" 第 10 章 ランダム・第 11 章 ノイズ | [thebookofshaders.com/10](https://thebookofshaders.com/10/?lan=jp)・[/11](https://thebookofshaders.com/11/?lan=jp) (日本語あり) | sin のハッシュ、 値ノイズ、 エルミート補間 ([samples/06](../../samples/06_glass_ice/README.md) §3) | リポジトリ (patriciogonzalezvivo/thebookofshaders) の LICENSE は 「All rights reserved」 | 可 | **不可** (文章もコードも写さない) |
| Inigo Quilez, 記事 "distance functions"・"normals for an SDF"・"raymarching SDFs" (個別のページ) | [distfunctions](https://iquilezles.org/articles/distfunctions)・[normalsSDF](https://iquilezles.org/articles/normalsSDF)・[raymarchingdf](https://iquilezles.org/articles/raymarchingdf) | 角の丸い箱・円柱・onion・共通部分、 中心差分、 球追跡 (06 §4〜6) | 記事の一覧のページに 「技術的なコード片は MIT、 作品としての数学・シェーダーは別」 と記載 | 可 | 可 (MIT の表示を同梱)。 作品は不可 |
| Joey de Vries, "LearnOpenGL" の Cubemaps (Reflection / Refraction)・PBR Theory (Fresnel) | [Cubemaps](https://learnopengl.com/Advanced-OpenGL/Cubemaps)・[PBR Theory](https://learnopengl.com/PBR/Theory) | reflect / refract の使い方、 Schlick のフレネル (06 §7〜8) | About のページに 「コードは CC BY-NC 4.0、 画像は CC BY 4.0」 と記載 | 可 | **不可** (非営利のみ) |
| Microsoft Learn: HLSL の `refract`・`discard`・セマンティクス (`SV_Depth`)、 D3D12 の `D3D12_RENDER_TARGET_BLEND_DESC`・Root Signature Limits | [refract](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-refract)・[discard](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/discard--sm4---asm-)・[semantics](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-semantics)・[blend](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_render_target_blend_desc)・[root signature limits](https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signature-limits) | 全反射で (0, 0, 0)、 描かない画素、 深度を書く、 ブレンド、 ルート定数の上限 (06 §10〜12、 §14) | 下の 「DirectX 12」 の Microsoft Learn と同じ | 可 | 下と同じ |
| PBR Book 第 4 版 "Specular Reflection and Transmission"・"Transmittance"・"Volume Scattering Processes" | [Specular](https://pbr-book.org/4ed/Reflection_Models/Specular_Reflection_and_Transmission)・[Transmittance](https://pbr-book.org/4ed/Volume_Scattering/Transmittance)・[Processes](https://pbr-book.org/4ed/Volume_Scattering/Volume_Scattering_Processes) | スネル・全反射・F0、 ランベルト・ベール、 放射伝達の式 (06 §8〜10) | 上の 「反射・屈折・PBR」 の PBR Book と同じ | 可 | 上と同じ |
| Wikipedia "Ice cube" の Clear and cloudy | [en.wikipedia.org/wiki/Ice_cube](https://en.wikipedia.org/wiki/Ice_cube) | 冷凍庫の氷が白く濁る理由 (06 §9) | 本文 CC BY-SA 4.0 | 可 | 文章を写すなら同じ条件での公開が要る (写していない) |

### DirectX 12

| 文献 | 正規の入手先 | 使った所 | 利用条件 | 読む | コードを写して商用 |
| --- | --- | --- | --- | --- | --- |
| Microsoft Learn: Direct3D 12 の説明と API の説明 (`CopyTextureRegion`, `GetCopyableFootprints`, ルートシグネチャ, リソースバリアなど) | [learn.microsoft.com/windows/win32/direct3d12](https://learn.microsoft.com/windows/win32/direct3d12/directx-12-programming-guide) | 全部の段の API の使い方 | 文書のソースのリポジトリ (MicrosoftDocs/win32, sdk-api) は、 本文 CC BY 4.0・コード MIT。 公開ページへの適用はリポジトリの表示からの推定 | 可 | 可 (コード片は MIT の表示、 本文は CC BY 4.0 の表示) |
| microsoft/DirectX-Graphics-Samples | [GitHub](https://github.com/microsoft/DirectX-Graphics-Samples) | API の使い方の確かめ | ルートの LICENSE は MIT。 一部のフォルダ (MiniEngine、 素材など) は別のライセンス | 可 | 可 (MIT の表示)。 写すファイルのフォルダのライセンスを個別に確かめる |

## ツール

| ツール | 入手先 | 利用条件 |
| --- | --- | --- |
| RenderDoc | [renderdoc.org](https://renderdoc.org/) | MIT (GitHub の LICENSE.md で確認) |
| PIX on Windows | [devblogs.microsoft.com/pix/download](https://devblogs.microsoft.com/pix/download/) | Microsoft の公式ページから入手。 ダウンロードのページに利用条件の記載は無い (未確認)。 インストール時の使用許諾を確かめる |

## 使ってはいけない物

| 物 | 理由 |
| --- | --- |
| Shadertoy のシェーダーのコード (作者がライセンスを書いていない物) | 規約の既定は CC BY-NC-SA 3.0 (非営利・継承)。 商用の製品に入れられず、 MIT のリポジトリにも混ぜられない (規約のページは検索エンジンの索引で確認。 直接は未確認) |
| LearnOpenGL のコード | CC BY-NC 4.0 (非営利)。 本文は利用許諾の表記なし。 読んで学ぶのはよい |
| PBR 本 (pbr-book.org) の本文中のコード | CC BY-NC-ND 4.0。 同じ物は pbrt-v4 (Apache-2.0) から取る |
| SIGGRAPH のコースノートのコード (Karis 2013 など) | 利用許諾の表記が無い |
| Wikipedia などの本文を貼ること | CC BY-SA (表示と継承が要る)。 読んで、 自分の言葉で書く |
| 非公式の PDF (無料 PDF のサイト、 Sci-Hub、 LibGen、 Z-Library、 講義のフォルダに置かれた論文など) | 権利者の許諾が無い・分からない。 DOI と公式のページから入手する |

## 書き方の注意

- 論文は DOI で、 ウェブの資料は URL と見た日で引用する。
- 式は自分の記法で書き直し、 図は自分で作る。 他の資料の本文・図・コードを訳して載せるのも転載になるので避ける。
- 発表のスライドに他の資料の図を使いたいときは、 その図のライセンス (CC BY なら表示、 NC なら非営利の場だけ、 表記なしなら許可を取る) を確かめる。

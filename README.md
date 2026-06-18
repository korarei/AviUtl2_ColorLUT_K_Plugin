# ColorLUT_K

![GitHub License](https://img.shields.io/github/license/korarei/AviUtl2_ColorLUT_K_Plugin)
![GitHub Last commit](https://img.shields.io/github/last-commit/korarei/AviUtl2_ColorLUT_K_Plugin)
![GitHub Downloads](https://img.shields.io/github/downloads/korarei/AviUtl2_ColorLUT_K_Plugin/total)
[![GitHub Release][releases-badge]][releases-url]
[![AviUtl2 Catalog][catalog-badge]][catalog-url]

AviUtl ExEdit2でLUTファイルを扱えるようにするプラグイン．

以下の機能が追加される．

フィルタ

- 色調整\\ColorLUT_K: LUTファイルを用いた色調補正

メディアオブジェクト

- LUT\\HaldCLUT_K: Hald CLUT (画像形式のLUT)

ファイルドロップ

- LUT フィルタをレイヤーに追加: LUTファイルをD&Dでレイヤーに追加する

ファイル出力

- LUT ファイルを出力: 画像をLUTファイルとして出力する

[ダウンロードはこちらから](https://github.com/korarei/AviUtl2_ColorLUT_K_Plugin/releases)

## 動作確認

- [AviUtl ExEdit2 beta50](https://spring-fragrance.mints.ne.jp/aviutl/)

> [!CAUTION]
> beta48以降必須．

## 導入・更新・削除

### パッケージファイルからインストール

#### 導入・更新

[こちら][releases-url]からダウンロードした `*.au2pkg.zip` をAviUtl2にD&D．

#### 削除

パッケージ情報からアンインストールする．

### [AviUtl2 カタログ](https://github.com/Neosku/aviutl2-catalog)からインストール

[こちら][catalog-url]から導入，更新，削除を行う．

## 使い方

### ColorLUT_K

初期ラベル: `色調整`

以下の形式のLUTファイルを読み込み，画像の色を変えるフィルタ．

- Cube LUT Specification Version 1.0 に準拠した LUT ファイル (.cube)
- Hald CLUT ファイル (.bmp, .png, .tiff, .tif)

読み込んだ LUT はファイルパスをキーとしてキャッシュを取るので， LUT に変更があった場合は `Reload LUT` または本体の`キャッシュを破棄`をクリックして再読み込みを行うこと．

本体の`キャッシュを破棄`を行った場合，すべての LUT ファイルに対してキャッシュ破棄を行う．

#### パラメータ

- LUT File: LUT ファイルを指定する
- Reload LUT: `LUT File` で指定した LUT を再読み込みする
- Blend Mode: 合成時のブレンドモード
  - Normal: 通常
  - Dissolve: ディザ合成
  - Darken: 比較 (暗)
  - Multiply: 乗算
  - Color Burn: 焼き込み (カラー)
  - Linear Burn: 焼き込み (リニア)
  - Darker Color: カラー比較 (暗)
  - Lighten: 比較 (明)
  - Screen: スクリーン
  - Color Dodge: 覆い焼き (カラー)
  - Linear Dodge (Add): 覆い焼き (リニア) - 加算
  - Lighter Color: カラー比較 (明)
  - Overlay: オーバーレイ
  - Soft Light: ソフトライト
  - Hard Light: ハードライト
  - Linear Light: リニアライト
  - Vivid Light: ビビッドライト
  - Pin Light: ピンライト
  - Hard Mix: ハードミックス
  - Difference: 差分
  - Exclusion: 除外
  - Subtract: 減算
  - Divide: 除算
  - Hue: 色相
  - Saturation: 彩度
  - Color: カラー
  - Luminosity: 輝度
- Opacity: エフェクトの適用度合
- Clamp: 合成結果を `[0, 1]` にクランプする

> [!NOTE]
> - `Clamp` にチェックがない場合，合成結果が `[0, 1]` の範囲を超えてしまうことがある．
> - Hue, Saturation, Color, Luminosity は Photoshop で採用されている HSL をベースにしている．
> - AviUtl の合成モードで陰影は焼き込み (リニア)，明暗はリニアライト，色差はカラーに対応する．

### HaldCLUT_K

Hald CLUTを生成するメディアオブジェクト．

#### パラメータ

- Level: Identity Hald CLUT のレベルを指定する (Level の 2 乗が Cube LUT の `LUT_3D_SIZE` となり， Level の 3 乗が Hald CLUT の辺の長さとなる)
- Resize Scene to LUT: Hald CLUT の画像サイズに合わせてシーンサイズを変更する (いずれ削除する可能性がある)

> [!NOTE]
> beta50ではシーンサイズの変更はUndoに対応していない．

### LUT ファイル出力

Cube LUT (`.cube`) または Hald CLUT (16bit RGB PNG `.png`) を出力する出力プラグイン．

シーン全体を Hald CLUT として読み込み変換を行う．シーンサイズが Hald CLUT のサイズと一致しない場合は周りの透明部分をカットする．

以下の手順でオリジナルLUTファイルを作成できる．

1. 画像の色調補正を行う．(複数レイヤーを使用してよい)
2. 見た目を整えた後，画像を `HaldCLUT_K` に差し替える．
3. `ファイル/ファイル出力/LUT ファイル出力`で Cube LUT または Hald CLUT としてエクスポートする．

出力したCube LUTファイルのサンプルを [samples/](./samples/) に置いている．

### LUT フィルタをレイヤーに追加

LUT ファイルを D&D することでレイヤー編集に `ColorLUT_K` フィルタオブジェクトを追加する．

## ビルド方法

[リリース用ワークフロー](./.github/workflows/releaser.yml)を参照されたい．

[extern](./plugins/extern/) 内 `vcpkg` ディレクトリに [vcpkg](https://github.com/microsoft/vcpkg) 本体を配置する必要がある．

## ライセンス

本プログラムのライセンスは [LICENSE](./LICENSE) を参照されたい．

また，本プログラムが利用するサードパーティ製ライブラリ等のライセンス情報は [THIRD_PARTY_LICENSES](./THIRD_PARTY_LICENSES.md) に記載している．

## 更新履歴

[CHANGELOG](./CHANGELOG.md) を参照されたい．

<!-- links -->

[releases-url]: https://github.com/korarei/AviUtl2_ColorLUT_K_Plugin/releases
[releases-badge]: https://img.shields.io/github/v/release/korarei/AviUtl2_ColorLUT_K_Plugin
[catalog-url]: https://aviutl2-catalog-badge.sevenc7c.workers.dev/package/korarei.ColorLUT_K
[catalog-badge]: https://aviutl2-catalog-badge.sevenc7c.workers.dev/badge/v/korarei.ColorLUT_K

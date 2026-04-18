# ColorLUT_K

![GitHub License](https://img.shields.io/github/license/korarei/AviUtl2_ColorLUT_K_Plugin)
![GitHub Last commit](https://img.shields.io/github/last-commit/korarei/AviUtl2_ColorLUT_K_Plugin)
![GitHub Downloads](https://img.shields.io/github/downloads/korarei/AviUtl2_ColorLUT_K_Plugin/total)
![GitHub Release](https://img.shields.io/github/v/release/korarei/AviUtl2_ColorLUT_K_Plugin)

AviUtl ExEdit2でLUTファイルを扱えるようにするプラグイン．

[ダウンロードはこちらから](https://github.com/korarei/AviUtl2_ColorLUT_K_Plugin/releases)

## 動作確認

- [AviUtl ExEdit2 beta41a](https://spring-fragrance.mints.ne.jp/aviutl/)

> [!CAUTION]
> beta37以降必須．

## 導入・更新・削除

初期配置場所は`色調整`と`LUT`である．

`オブジェクト追加メニューの設定`から`ラベル`を変更することで任意の場所へ移動可能．

### 導入・更新

ダウンロードした`*.au2pkg.zip`をAviUtl2にD&D．

手動で導入する場合は，`*.aux2`をAviUtl2が認識する場所に設置．

### 削除

パッケージ情報からアンインストールする．

手動で導入した場合は設置した`*.aux2`を削除する．

## 使い方

### ColorLUT_K

初期ラベル: `色調整`

以下の形式のLUTファイルを読み込み，画像の色を変えるフィルタ．

- Cube LUT Specification Version 1.0に準拠したLUTファイル (.cube)
- Hald CLUTファイル (.bmp, .png, .tiff, .tif)

読み込んだLUTはファイルパスをキーとしてキャッシュを取るので，LUTに変更があった場合は`Reload LUT`または本体の`キャッシュを破棄`をクリックして再読み込みを行うこと．

本体の`キャッシュを破棄`を行った場合，すべてのLUTファイルに対してキャッシュ破棄を行う．

#### パラメータ

- LUT File: LUTファイルを指定する
- Reload LUT: `LUT File`で指定したLUTを再読み込みする
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
- Clamp: 合成結果を`[0, 1]`にクランプする

> [!NOTE]
> - `Clamp`にチェックがない場合，合成結果が`[0, 1]`の範囲を超えてしまうことがある．
> - Hue, Saturation, Color, LuminosityはPhotoshopで採用されているHSLをベースにしている．
> - AviUtlの合成モードで陰影は焼き込み (リニア)，明暗はリニアライト，色差はカラーに対応する．

### HaldCLUT_K

Hald CLUTを生成するメディアオブジェクト．

#### パラメータ

- Level: Identity Hald CLUTのレベルを指定する (Levelの2乗がCube LUTの`LUT_3D_SIZE`となり，Levelの3乗がHald CLUTの辺の長さとなる)
- Resize Scene to LUT: Hald CLUTの画像サイズに合わせてシーンサイズを変更する (いずれ削除する可能性がある)

> [!NOTE]
> beta41aではシーンサイズの変更はUndoに対応していない．

### LUTファイル出力

Cube LUT (`.cube`) またはHald CLUT (16bit RGB PNG `.png`) を出力する出力プラグイン．

シーン全体をHald CLUTとして読み込み変換を行う．シーンサイズがHald CLUTのサイズと一致しない場合は周りの透明部分をカットする．

以下の手順でオリジナルLUTファイルを作成できる．

1. 画像の色調補正を行う．(複数レイヤーを使用してよい)
2. 見た目を整えた後，画像を`HaldCLUT_K`に差し替える．
3. `ファイル/ファイル出力/LUTファイル出力`でCube LUTまたはHald CLUTとしてエクスポートする．

出力したCube LUTファイルのサンプルを[samples/](./samples/)に置いている．

### LUTフィルタをレイヤーに追加

LUTファイルをD&Dすることでレイヤー編集に`ColorLUT_K`フィルタオブジェクトを追加する．

## ビルド方法

[リリース用ワークフロー](./.github/workflows/releaser.yml)を参照されたい．

## ライセンス

本プログラムのライセンスは[LICENSE](./LICENSE)を参照されたい．

また，本プログラムが利用するサードパーティ製ライブラリ等のライセンス情報は[THIRD_PARTY_LICENSES](./THIRD_PARTY_LICENSES.md)に記載している．

## 更新履歴

[CHANGELOG](./CHANGELOG.md)を参照されたい．

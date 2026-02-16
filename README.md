# ColorLUT_K

![GitHub License](https://img.shields.io/github/license/korarei/AviUtl2_ColorLUT_K_Plugin)
![GitHub Last commit](https://img.shields.io/github/last-commit/korarei/AviUtl2_ColorLUT_K_Plugin)
![GitHub Downloads](https://img.shields.io/github/downloads/korarei/AviUtl2_ColorLUT_K_Plugin/total)
![GitHub Release](https://img.shields.io/github/v/release/korarei/AviUtl2_ColorLUT_K_Plugin)

AviUtl2でLUTファイルを扱えるようにするプラグイン．

[ダウンロードはこちらから](https://github.com/korarei/AviUtl2_ColorLUT_K_Plugin/releases)

## 動作確認

- [AviUtl ExEdit2 beta33](https://spring-fragrance.mints.ne.jp/aviutl/)

> [!CAUTION]
> beta33以降必須．

## 導入・更新・削除

初期配置場所は`色調整`と`Utility`である．

`オブジェクト追加メニューの設定`から`ラベル`を変更することで任意の場所へ移動可能．

> [!IMPORTANT]
> 同一ファイルが複数個所に存在しないようにすること．

### 導入・更新

ダウンロードした`*.au2pkg.zip`をAviUtl2にD&D．

手動で導入する場合は，`*.aux2`をAviUtl2が認識する場所に設置．

### 削除

パッケージ情報からアンインストールする．

手動で導入した場合は設置した`*.aux2`を削除する．

## 使い方

### ColorLUT_K

以下の形式のLUTファイルを読み込み，画像の色を変える．

- Cube LUT Specification Version 1.0に準拠したLUTファイル (.cube)
- Hald CLUTファイル (.bmp, .png, .tiff, .tif)

> [!NOTE]
> - 浮動小数点形式のTIFFファイルはサポートしていない．
> - iccプロファイルが埋め込まれた画像はWICが自動的にsRGBに変換する可能性がある．

読み込んだLUTはファイルパスをキーとしてキャッシュを取るので，LUTに変更があった場合は`Reload LUT`または本体の`キャッシュを破棄`をクリックして再読み込みを行うこと．

本体の`キャッシュを破棄`を行った場合，すべてのLUTファイルに対してキャッシュ破棄を行う．

### パラメータ

#### LUT File

LUTファイルを指定する．

#### Reload LUT

`LUT File`で指定したLUTを再読み込みする．

#### Opacity

エフェクトの適用度合．

### Hald2Cube_K

Hald CLUTファイルを読み込み，Cube LUTファイルとしてエクスポートする．

以下のようにオブジェクトに対してエフェクトをかけることを想定している．

1. `Hald2Cube_K` (`Identity Pattern`が有効)
2. 様々なエフェクト (固定サイズ)
3. ...
4. `Hald2Cube_K` (`Export as .cube...`でエクスポート)

複数レイヤーを使う場合は以下の手順で行う．

1. `Hald2Cube_K`を追加したオブジェクト (`Identity Pattern`が有効，`Fit Scene to LUT`をクリック)
2. オブジェクト (様々なエフェクト)
3. ...
4. `Hald2Cube_K`を追加したフレームバッファオブジェクト (`Export as .cube...`をクリック)

外部から入手したHald CLUTを変換したい場合は，画像オブジェクトに`Hald2Cube_K`を追加し，`Identity Pattern`を無効にした状態でエクスポートを行うこと．

> [!IMPORTANT]
> シークバーを動かし，`Hald2Cube_K`を描画させた状態でエクスポートを行うこと．

#### Identity Pattern

チェックを入れると，Identity Hald CLUTを生成する．

#### Level

Identity Hald CLUTのレベルを指定する．

Levelの2乗がCube LUTの`LUT_3D_SIZE`となり，Levelの3乗がHald CLUTの辺の長さとなる．

#### Fit Scene to LUT

チェックを入れると，Hald CLUTの画像サイズに合わせてシーンサイズを調整する．

> [!NOTE]
> beta33ではUndoに対応していない．

#### Title

Cube LUTファイルのタイトルを指定する．

#### Export as .cube...

Cube LUTファイルとしてエクスポートする．

## ビルド方法

[リリース用ワークフロー](./.github/workflows/releaser.yml)を参照されたい．

## ライセンス

本プログラムのライセンスは[LICENSE](./LICENSE)を参照されたい．

また，本プログラムが利用するサードパーティ製ライブラリ等のライセンス情報は[THIRD_PARTY_LICENSES](./THIRD_PARTY_LICENSES.md)に記載している．

## 更新履歴

[CHANGELOG](./CHANGELOG.md)を参照されたい．

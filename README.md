# ColorLUT_K

![GitHub License](https://img.shields.io/github/license/korarei/AviUtl2_ColorLUT_K_Plugin)
![GitHub Last commit](https://img.shields.io/github/last-commit/korarei/AviUtl2_ColorLUT_K_Plugin)
![GitHub Downloads](https://img.shields.io/github/downloads/korarei/AviUtl2_ColorLUT_K_Plugin/total)
![GitHub Release](https://img.shields.io/github/v/release/korarei/AviUtl2_ColorLUT_K_Plugin)

AviUtl2でLUTファイルを扱えるようにするプラグイン

[ダウンロードはこちらから](https://github.com/korarei/AviUtl2_ColorLUT_K_Plugin/releases)

## 動作確認

- [AviUtl ExEdit2 beta26](https://spring-fragrance.mints.ne.jp/aviutl/)

> [!CAUTION]
> beta26以降必須．

## 導入・削除・更新

初期配置場所は`色調整`である．

`オブジェクト追加メニューの設定`から`ラベル`を変更することで任意の場所へ移動可能．

### 導入

下記のいずれかの方法で導入可能．

- 同梱の`*.aux2`をAviUtl2にD&D．

- 同梱の`*.aux2`を`%ProgramData%`内の`aviutl2\Plugin`フォルダに入れる．

- 同梱の`*.aux2`を`aviutl2.exe`と同じ階層内の`data\Plugin`フォルダに入れる．

### 削除

- 導入したものを削除する．

### 更新

- 導入したものを上書きする．

## 使い方

Cube LUT Specification Version 1.0に準拠したLUTファイル (.cube) を読み込み，画像の色を変える．

読み込んだLUTはファイルパスをキーとしてキャッシュを取るので，LUTに変更があった場合は`Reload LUT`または本体の`キャッシュを破棄`をクリックして再読み込みを行うこと．

本体の`キャッシュを破棄`を行った場合，すべてのLUTファイルに対してキャッシュ破棄を行う．

### パラメータ

#### LUT File

LUTファイルを指定する．

#### Reload LUT

`LUT File`で指定したLUTを再読み込みする．

#### Opacity

エフェクトの適用度合．

## ビルド方法

[リリース用ワークフロー](./.github/workflows/releaser.yml)を参照されたい．

## ライセンス

本プログラムのライセンスは[LICENSE](./LICENSE)を参照されたい．

また，本プログラムが利用するサードパーティ製ライブラリ等のライセンス情報は[THIRD_PARTY_LICENSES](./THIRD_PARTY_LICENSES.md)に記載している．

## 更新履歴

[CHANGELOG](./CHANGELOG.md)を参照されたい．

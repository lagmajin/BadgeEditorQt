# MainWindow 分割計画

`mainwindow.cpp` は UI 構築、Designer 編集、Layout 同期、出力、設定保存を同時に持つため、責務ごとに実装ファイルを分ける。

## 移行順

## 実装済み（現時点）

- `mainwindow_support.h/.cpp`: 操作警告、プリンター設定、色補間 `blend`
- `mainwindow_io.cpp`: 新規／開く／保存、画像・PDF出力、印刷プレビュー、直接印刷
- `mainwindow_layout.cpp`: レイアウト生成、ページ操作、レイアウト更新
- `mainwindow_designer.cpp`: バッジ編集、画像ドロップ、ガイド／照明／グリッター操作
- `mainwindow_edit.cpp`: Undo／Redo
- `mainwindow_ui.cpp`: テーマ、モード、Perspective、Dock／アプリ設定
- `mainwindow_inspector.cpp`: Inspector のレイヤー表示状態更新
- `layerpreview.h/.cpp`: レイヤー画像補正、合成、塗りつぶし、共通座標計算

残りは `renderLayerPreviewPixmap` などのプレビュー描画本体と、転送用描画処理の分離。

### 1. 共通補助関数

先に次の関数を `mainwindow_support.h/.cpp` へ移す。

- `showOperationWarning`
- `configurePrinterForDocument`

匿名名前空間に依存する IO／Layout 実装をなくすための境界。

### 2. 出力・ファイル IO

`mainwindow_io.cpp` に移す。

- `onNew`
- `onOpen`
- `openProjectPath`
- `onSave`
- `onSaveAs`
- `onExportPdf`
- `onExportPng`
- `onPrintPreview`
- `onPrint`

主な依存は `projectsync`、`ExportDialog`、`PrintDialog`、`LayoutWorkspaceWidget`。

### 3. Layout 操作

`mainwindow_layout.cpp` に移す。

- `onSendToLayout`
- `syncLayoutWorkspace`
- `currentLayoutPages`
- `onAutoLayout`
- `onMixedLayout`
- `onClearLayout`
- `updateLayoutPageUi`
- ページ選択、複製、削除、並び替え、名前変更

`LayoutEngine` と `projectsync::currentDocument` をこの境界の中心にする。

### 4. Designer 操作

`mainwindow_designer.cpp` に移す。

- バッジ追加、削除、複製、画像ドロップ
- 選択変更、移動、整列、ナッジ
- Inspector とレイヤー UI の更新
- ガイド、照明、グリッター操作

### 5. 転送処理

`layouttransfer.h/.cpp` または将来の `badge.layouttransfer.ixx` に移す。

- `renderTransferCropImage`
- `renderLayoutDebugImage`
- `makeLayoutTransferBadge`
- ガイド内クロップと転送用データ生成

ここは `MainWindow` を参照しない純粋な API にする。

## CMake への追加順

```cmake
target_sources(BadgeEditorQt PRIVATE
    mainwindow.cpp
    mainwindow_support.cpp
    mainwindow_io.cpp
    mainwindow_layout.cpp
    mainwindow_designer.cpp
    layouttransfer.cpp
)
```

## 検証単位

各段階でビルド前に次を確認する。

1. `MainWindow` のメンバー定義が重複していない
2. 匿名名前空間の補助関数を別 translation unit から直接参照していない
3. `mainwindow.h` の宣言と移動先の定義が一致している
4. Layout の表示、画像／PDF出力、印刷プレビュー、直接印刷が同じページ生成結果を使っている
5. Designer の編集履歴と選択状態が分割前と変わらない

最初の実装単位は `mainwindow_support` → `mainwindow_io` とし、Layout の挙動変更とファイル移動を同じ変更に混在させない。

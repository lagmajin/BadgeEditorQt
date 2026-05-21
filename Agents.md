# Agents.md

このリポジトリで作業するエージェント向けの開発指針です。

## 基本方針

- 既存機能を壊さないことを最優先にする
- 変更は小さく、境界から順に進める
- Qt 依存の強い UI 層は後回しにする
- 純データ層とサービス層を先に整える
- 変更は単独で意味が通る単位に分ける
- 明示的な指示がない限り `build` / `make` / `cmake --build` は実行しない

## モジュール化方針

- `ixx` 化は段階的に進める
- まずは低リスクで参照数の多い葉ノードから切る
- 優先候補は `badgeitem.h`, `layoutengine.h`, `imageprocessor.h`, `constants.h`, `viewportbackend.h`
- `mainwindow.h`, `designerwidget.h`, `layoutworkspacewidget.h` のような UI ハブは最後に回す
- Qt ヘビーな機能を持つファイルは、依存関係が落ち着くまで無理に module 化しない
- 新しいモジュールは既存の `badge.model.ixx`, `badge.paper.ixx`, `badge.layout.ixx`, `badge.document.ixx`, `badge.qtbridge.ixx` の流れを壊さない位置に置く

## `import std` 方針

- `import std` は `ixx` 化と並行して進めてよい
- ただし一気に全域へ広げず、まずは新規または切り出したモジュール単位で使う
- 標準ライブラリの `import std` は、現在のコンパイラと生成環境で安定して使える範囲に限定する
- 既存の巨大な `.cpp` をまとめて `import std` 化するより、葉ノードのモジュールから順に導入する
- 互換性に不安がある箇所では `#include` を残してよい
- `import std` 以外の標準ライブラリは、可能な限り `import <vector>;` のような標準ヘッダーユニットを使う
- ただし、コンパイラやビルド生成物の都合で安定しない場合は、従来の `#include` に戻してよい
- 新規のモジュールや移植対象コードでは、まず `import <...>;` を試し、駄目なら `#include` にフォールバックする

## 内部イベント基盤

- Qt の生イベントは残す
- アプリ内部の意味ある変化を畳む層を別に持つ
- 内部イベントの受け皿は `badge.event.ixx` のように独立させる
- `MainWindow` は queue の所有と flush を担当する
- 高頻度イベントは coalesce して、Inspector 更新や Diagnostics 更新をまとめて処理する

## 作業順の目安

1. 純データ層のモジュール化
2. サービス層のモジュール化
3. `import std` の段階導入
4. 内部イベント基盤の導入
5. UI ハブの整理

## 迷ったとき

- 低リスクで再利用しやすいものを先に切る
- UI ハブやイベント配線の大改造は後に回す
- ひとつの変更でモジュール化と振る舞い変更を同時に増やしすぎない
- 依存が増える方向より、依存を減らす方向を優先する

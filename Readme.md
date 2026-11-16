# BonCasClient

BonCasLink（BonCasServer / BonCasService）へ TCP で接続する Windows 用クライアント DLL です。BonCasClient v1.1.1 のソースコードを基に、複数の接続先の選択と安定性の改善を加えています。

## この fork の特徴

- 複数のサーバー・ポートを登録し、利用ソフトのリーダー選択で接続先を切り替えられます。
- リーダー名は `IP:ポート` です。既存の `[Server]` / `[Server:Port]` の設定形式を使用します。
- 通信のタイムアウト・接続判定・切断処理、バッファ長検証、カードハンドルの寿命管理を改善しています。
- 接続失敗後の1秒待機を接続先ごとに管理し、他の接続先への影響を抑えています。
- x86・x64 の DLL を GitHub Actions でビルドし、回帰テストを実行します。
- ローカルの WinSCard 互換 DLL を指定し、そのリーダーをネットワークリーダーと併用できます。

## 入手・導入

### このリポジトリのビルドを入手する

1. [GitHub Actions の build](https://github.com/nanamitm/BonCasClient/actions/workflows/build.yml) を開きます。
2. `master` ブランチの成功した実行を選びます。
3. 実行画面の **Artifacts** から、利用ソフトのビット数に合う ZIP をダウンロードします。ダウンロードには GitHub へのログインが必要です。

| 利用ソフト | Artifact | DLL |
| --- | --- | --- |
| 32bit（x86） | `BonCasClient-Win32` | 32bit 版 `BonCasClient.dll` |
| 64bit（x64） | `BonCasClient-x64` | 64bit 版 `BonCasClient.dll` |

Artifacts には保存期限があります。入手できない場合は、後述の手順でソースコードからビルドできます。

### DLL と設定ファイルを配置する

ZIP を展開し、利用ソフトが指定する場所に DLL を配置します。読み込み方法や DLL 名は利用ソフトの説明に従ってください。接続先を設定する INI は、DLL と同じフォルダに配置します。

DLL のビット数は **Windows ではなく、DLL を読み込む利用ソフト**に合わせてください。64bit Windows 上で32bit の利用ソフトを使う場合は x86 版が必要です。

クライアントと BonCasLink サーバーのビット数は、TCP 通信のため一致する必要がありません。

### Visual C++ ランタイム

このリポジトリの v143 ビルドには、対応する Microsoft Visual C++ v14 再頒布可能パッケージが必要です。[Microsoft の公式ダウンロード案内](https://learn.microsoft.com/ja-jp/cpp/windows/latest-supported-vc-redist)から、使用する DLL に合う x86 または x64 のパッケージをインストールしてください。ランタイムはビルドに使用した MSVC と同じバージョン以降が必要です。

### 参考・旧版

[別 fork の BonCasClient v1.1.1-patch](https://github.com/mizunoko/BonCasClient/releases/download/v1.1.1-patch/BonCasClient_v1.1.1-patch.zip) は従来の配布物です。このリポジトリで追加した接続先選択や安定性改善を含むビルドとは異なります。

## 接続先の設定

### INI の場所と名前

INI の名前は **DLL と同じベース名 + `.ini`** です。

| DLL | 設定ファイル |
| --- | --- |
| `BonCasClient.dll` | 同じフォルダの `BonCasClient.ini` |
| `winscard.dll` にリネームした場合 | 同じフォルダの `winscard.ini` |

設定は DLL の読み込み時に読み込まれます。変更後は、利用ソフトを再起動してください。

### 単一の接続先

```ini
[Server]
IP=127.0.0.1
Port=6900
```

INI がない場合も、この接続先が既定値になります。リーダー一覧には `127.0.0.1:6900` と表示されます。

### 複数の接続先

`[Server]` の接続先に加えて、`[Server:Port]` に追加の接続先を登録します。

```ini
[Server]
IP=127.0.0.1
Port=6900

[Server:Port]
Server00=127.0.0.1:6901
Server01=192.168.1.20:6900
```

この例では、次の順番でリーダーが表示されます。

1. `127.0.0.1:6900`
2. `127.0.0.1:6901`
3. `192.168.1.20:6900`

利用ソフトがリーダー選択に対応している場合、その設定画面や設定ファイルで上記の名前を選択してください。設定項目や操作方法は利用ソフトに依存します。

### 設定時の注意

- 接続先には IPv4 アドレスと、1〜65535 のポート番号を指定してください。ホスト名や IPv6 アドレスには対応していません。
- 追加のキーは `Server00` から `Server98` まで、欠番のない連番で指定します。キーが存在しない、または値が空の場合、その時点で追加設定の読み込みを終了します。
- 任意の表示名を付ける機能はありません。リーダー名には `IP:ポート` を使用します。
- 別のカードを使うには、対応するサーバー・ポートを用意してください。
- `[Readers]` 形式の設定には対応していません。

### ローカルの WinSCard 互換 DLL を併用する

同じ INI の `[Local]` に `Enable=1` と、読み込む **DLL ファイルのフルパス**を指定します。プロジェクトのフォルダ名だけでは読み込めません。省略時や `Enable=0` の場合、この機能は無効です。

たとえば、64bit 版の SCardHub を併用する場合は次のように設定します。

```ini
[Server]
IP=127.0.0.1
Port=6900

[Local]
Enable=1
Path=C:\FreeSoftwere\git\SCardHub\x64\Release\WinSCard.dll
```

32bit 版を使用する場合は、SCardHub の `Win32\Release\WinSCard.dll` など、対応する DLL を指定してください。**利用ソフト・BonCasClient・指定する DLL・その配下のリーダー DLL のビット数を合わせる必要があります。** SCardHub 自体のリーダー登録は、SCardHub 側の `SCardHub.ini` で行います。

- リーダー一覧は、ネットワークリーダー、指定 DLL のリーダーの順です。同じ名前は重複表示せず、ネットワークリーダーを優先します。
- 指定 DLL のリーダー名を選ぶと、接続・送受信・切断をその DLL に転送します。共有モードとプロトコル、DLL が返すエラーも転送します。
- `Enable=1` で `Path` を省略すると、Windows のシステムディレクトリにある `winscard.dll` を使用します。独自 DLL と OS のリーダーをまとめる場合は、SCardHub 側で設定してください。
- DLL が見つからない、ビット数が違う、必要な API がない、コンテキスト作成や一覧取得に失敗する場合、その DLL のリーダーは表示しません。ネットワークリーダーは引き続き使用できます。
- 相対パスや BonCasClient 自身の指定には対応しません。SCardHub からこの BonCasClient を読み込むなど、循環する構成も避けてください。

読み込んだ DLL は参照を保持するため、パスや DLL の変更を反映するには利用ソフトの再起動が必要です。

## 接続できないとき

| 症状 | 確認すること |
| --- | --- |
| DLL を読み込めない | 利用ソフトと DLL のビット数、Visual C++ ランタイム、DLL の配置場所 |
| 接続先が一覧に出ない | INI の名前と配置、`[Server:Port]` のキーの欠番・空欄、変更後の利用ソフト再起動 |
| リーダーは表示されるが接続できない | IPv4 アドレス・ポート、BonCasLink サーバーの稼働状態、ネットワーク・ファイアウォール設定 |
| ローカル DLL のリーダーが出ない | `[Local] Enable=1`、DLL ファイルのフルパスとビット数、指定 DLL 側の設定 |

一覧への表示は、接続先サーバーが稼働していることを保証しません。接続失敗直後は、その接続先への再接続を1秒間抑制します。

## ビルドとテスト

Visual Studio の「C++ によるデスクトップ開発」、MSVC v143 の x86/x64 ビルドツール、Windows SDK を用意してください。CI では `windows-2022` 上の v143 を使用しています。

リポジトリのルートで、使用するアーキテクチャの Visual Studio 開発者 PowerShell から実行します。

```powershell
# x64 の開発者 PowerShell
./Test/Run.ps1 -Platform x64

# x86 の開発者 PowerShell
./Test/Run.ps1 -Platform Win32
```

このスクリプトは Release DLL のビルドと回帰テストを実行します。DLL の出力先は x64 が `x64/Release/BonCasClient.dll`、x86 が `Release/BonCasClient.dll` です。

テストはローカルの模擬 TCP サーバーを使用し、タイムアウト、接続失敗の分離、既存 INI 形式、バッファ保護、送受信中の切断・コンテキスト解放などを検証します。カードリーダーや BonCasServer は不要です。詳細は [Test/README.md](Test/README.md) を参照してください。

**検証範囲：** x86・x64 のビルドと自動回帰テストを確認しています。ローカル DLL のテストも模擬リーダーを使用します。実機カードリーダー、BonCasLink サーバー、各デコーダー・利用ソフトを組み合わせた動作確認は別途必要です。

## 開発者向け API

以下の9関数をエクスポートし、対応する `SCardXXXX` の名前でも公開しています。

- `CasLinkConnectA` / `CasLinkConnectW`
- `CasLinkDisconnect`
- `CasLinkEstablishContext`
- `CasLinkFreeMemory`
- `CasLinkListReadersA` / `CasLinkListReadersW`
- `CasLinkTransmit`
- `CasLinkReleaseContext`

WinSCard の全 API を実装するものではありません。公開名は [BonCasClient.def](BonCasClient/BonCasClient.def) を参照してください。

呼び出し時には次に注意してください。

- `SCardEstablishContext` が返したコンテキストを、一覧取得や接続に使用します。
- `SCARD_AUTOALLOCATE` で取得したリーダー一覧は、同じコンテキストの `SCardFreeMemory` で解放します。コンテキスト解放時にも、そのコンテキストに属する未解放の一覧とカードを解放します。
- 一覧や応答の出力バッファが不足する場合は、必要長と `SCARD_E_INSUFFICIENT_BUFFER` を返し、データを出力バッファへコピーしません。送受信の場合、サーバーとの通信自体は実行済みです。
- ネットワークリーダーの送信コマンド長は1〜255バイトです。ローカル DLL への送信にはこの255バイト制限を適用しません。
- 無効なハンドル、解放済みのハンドルやコンテキストはエラーになります。

互換性のため、次の関数も公開しています。

```cpp
BOOL WINAPI SetConfig(const DWORD dwIP, const WORD wPort);
```

**現在の `SetConfig` は `TRUE` を返すだけで、接続先を変更しません。** 接続先の設定には INI を使用してください。

## 由来と変更履歴

BonCasClient v1.1.1 に同梱されていたソースコードを基にしています。オリジナルの説明は [BonCasClient.txt](BonCasClient.txt) に保存しています。現在の fork と設定・動作が異なる場合は、この README と実装を参照してください。

ベースにしたもの：

- Multi2Dec Ver.2.10
- BonCasLink Ver.1.10 z1
- 既存の [BonCasClient_fix.diff.txt](BonCasClient_fix.diff.txt)

通信修正などの取り込み元は [Test/README.md](Test/README.md) に記載しています。

主な変更：

- 複数接続先の選択、Visual Studio v143 への更新。
- 通信・バッファ・ハンドル管理の改善と、x86/x64 の CI・回帰テスト追加。

オリジナルの履歴：

- 2011/09/25：64bit 用 DLL 追加。
- 2010/11/11：実験品。

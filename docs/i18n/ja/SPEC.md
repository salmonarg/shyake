## Shyake 技術仕様

[English](../../SPEC.md) | [简体中文](../zh-CN/SPEC.md) | 日本語

Copyright (c) 2026 Salmonization. BSD 2-Clause License.

<table>
<tr><td>バージョン</td><td>0.2</td></tr>
<tr><td>最終更新</td><td>2026-07-11</td></tr>
</table>

---

### 1. 概要

Shyake は、POSIX スタイルのコマンドラインインターフェースクライアントを備えた、耐量子・エンドツーエンド暗号化の非同期メールシステムであり、検閲や監視に対抗するための分散型コミュニケーション手段として設計されています。

主な特性：

- **エンドツーエンド暗号化**：サーバーは平文を一切保持しません。すべてのメッセージ内容は送信前にクライアント側で暗号化されます。
- **耐量子暗号**：鍵カプセル化には ML-KEM-768 を、認証には
  ML-DSA-65（CRYSTALS-Dilithium）を使用します。いずれも
  [liboqs](https://github.com/open-quantum-safe/liboqs) 由来です。
- **分散型**：どの運営者もほぼゼロコストで自分のインスタンスをホストできます。インスタンスは、サーバー間リレーモデルを用いて任意でフェデレーションします。
- **ステートレスなサーバー**：サーバーは暗号文と公開鍵のみを保存します。
- **保存時に暗号化される鍵**：クライアントの秘密鍵は、任意でパスフレーズ（scrypt + ChaCha20-Poly1305）によりディスク上で保護されます。

---

### 2. アーキテクチャ

#### 2.1 コンポーネント

```
shyake/
├── client/                 # C クライアント
│   ├── src/lib/            # コアロジック（ネットワーク、暗号、
│   │                       #   メール、アカウント、パスフレーズ、更新）
│   ├── src/cli/            # CLI 解析、表示、プロンプト、設定
│   ├── include/shyake.h    # 公開 API（不透明ポインタ）
│   ├── tests/              # ライブラリテストプログラム
│   └── Makefile
├── server/                 # Cloudflare Worker
│   ├── src/index.ts        # Hono ルート
│   ├── src/utils.ts        # ヘルパー（PoW、ユーザー名検証）
│   ├── migrations/         # D1 スキーママイグレーション
│   └── wrangler.toml       # Worker 設定
└── docs/
```

#### 2.2 クライアント

- **標準**：C11、POSIX.1-2008（`_POSIX_C_SOURCE=200809L`）
- **ビルドシステム**：GNU Make；クロスプラットフォーム（macOS、GNU/Linux、Termux）
- **成果物**：
  - `bin/shyake` — CLI バイナリ、`libshyake.a` を静的リンク
  - `lib/libshyake.a` — 静的ライブラリ
  - `lib/libshyake.so` / `libshyake.dylib` — FFI 用共有ライブラリ
- **依存関係**：
  - `liboqs`（常に静的リンク）— ML-KEM と ML-DSA
  - `libcurl` — HTTP トランスポート
  - `libcrypto`（OpenSSL）— SHA-256 フィンガープリント、
    SHA-1（PoW）、ChaCha20-Poly1305 AEAD、
    scrypt KDF（`EVP_PBE_scrypt`）
  - `cJSON`（同梱）— JSON 解析

#### 2.3 サーバー

- **ランタイム**：Cloudflare Workers
- **フレームワーク**：[Hono](https://hono.dev/)
- **データベース**：Cloudflare D1（SQLite）
- **署名検証**：WebAssembly にコンパイルされた ML-DSA-65
  （`mldsa65-wasm`）を、Wrangler の `CompiledWasm` ルール経由でロードします。

---

### 3. 暗号設計

#### 3.1 鍵ペア

各ユーザーは `liboqs` を用いてローカルで 2 つの独立した鍵ペアを生成します：

| 用途 | アルゴリズム | ファイル |
|---|---|---|
| 鍵カプセル化 | ML-KEM-768 | `kem_pk.bin`、`kem_sk.bin` |
| 認証／署名 | ML-DSA-65 | `sig_pk.bin`、`sig_sk.bin` |

公開鍵は生バイトとして保存され、登録時にサーバーへアップロードされます。秘密鍵は、パスフレーズが設定されている場合は §3.7 に記述する保存時暗号化フォーマットで、設定されていない場合は生バイトとして保存されます。

#### 3.2 メッセージ暗号化

1. ランダムな 256 ビット対称鍵を生成する。
2. その鍵を用いて `subject` と `body` を
   **ChaCha20-Poly1305** で暗号化する。それぞれ独立したランダムな
   96 ビット nonce を使用する。各暗号文は
   `base64(nonce || ciphertext || tag)` として送信される。
3. **受信者の ML-KEM 公開鍵**にカプセル化する：KEM カプセル化により KEM 暗号文と 32 バイトの共有秘密が得られる。対称鍵を共有秘密と XOR して連結する：
   `enc_key_recipient = base64(kem_ct || (sym_key XOR ss))`。
4. **送信者自身の ML-KEM 公開鍵**に対して同じカプセル化を繰り返す → `enc_key_sender`（送信者が自分の送信ボックスを読めるようにするため）。

復号はこの逆の手順です：クライアントは自身の KEM 秘密鍵で共有秘密をデカプセル化し、それを暗号化鍵フィールドと XOR して対称鍵を復元し、その後コンテンツを復号します。

単体ファイル暗号化コマンド（`enc` / `dec`）は、同じ
ML-KEM-768 + ChaCha20-Poly1305 構成を、長さプレフィックス付きのバイナリコンテナ（`.enc` ファイル）で使用します。

#### 3.3 認証プロトコル

認証を要するすべての操作は ML-DSA-65 で署名されます。2 つの運搬形式が使われます：

**ヘッダーベース**（登録とメール送信を除くすべての認証エンドポイント）：

```
X-Shyake-Username:  <username>
X-Shyake-Timestamp: <unix seconds>
X-Shyake-Signature: <base64(ML-DSA-65 signature)>
X-Shyake-Pow:       <Hashcash token>
```

署名対象のメッセージは、HTTP メソッド、エンドポイント（クエリ文字列を含む）、ユーザー名、タイムスタンプから構成される決定的な文字列です——例：

```
GET:/api/mail?type=inbox:salmon:1749513600
```

**ボディベース**（`POST /api/register` と `POST /api/mail`）：署名と PoW トークンはリクエストボディの JSON フィールドとして運ばれます。署名対象のメッセージは、以下のペイロード部分集合のコンパクト JSON シリアライゼーションです（フィールド順はクライアントが生成した通り）：

`POST /api/register`：

```json
{
  "username": "...",
  "kem_pubkey": "...",
  "sig_pubkey": "...",
  "timestamp": "1749513600"
}
```

`POST /api/mail`：

```json
{
  "sender": "...",
  "recipient": "...",
  "recipient_kem_fingerprint": "...",
  "enc_subject": "...",
  "enc_body": "...",
  "timestamp": "1749513600",
  "size": 512
}
```

完全なリクエストボディはさらに `enc_key_sender`、
`enc_key_recipient`、`signature`、`pow` を運びますが、これらは署名対象の部分集合には含まれません。

サーバーは、D1 に保存された送信者の `sig_pubkey`
（フェデレーションメールの場合は送信者のインスタンスから取得）を用い、WASM ML-DSA モジュール経由で署名を検証します。

#### 3.4 リプレイ対策

署名対象のすべてのメッセージにタイムスタンプが含まれます。サーバーは、タイムスタンプがサーバー時刻から **300 秒（5 分）**
を超えて乖離しているリクエストを拒否します。

#### 3.5 プルーフ・オブ・ワーク（PoW）

読み取りを含むすべての認証リクエストには、SHA-1 難易度
**20 ビット**の Hashcash-v1 スタイルの PoW トークンが必要です：

```
1:<bits>:<yymmdd>:<resource>::<rand>:<counter-hex>
```

`resource` は操作するユーザーのユーザー名です。トークンはクライアント側で生成（マイニング）され、署名検証やデータベース処理より前にサーバー側で検証されます。

#### 3.6 鍵フィンガープリント

フィンガープリントは、生の（デコード済み）ML-KEM 公開鍵バイトの
**SHA-256** を小文字 16 進エンコードしたものです。クライアントは信頼済みの鍵を `~/.config/shyake/known_hosts` にキャッシュします。
1 行につき 1 エントリで、スペース区切りです：

```
<username> <fingerprint-hex> <kem_pubkey-base64>
```

クライアントは送信のたびに受信者の最新の鍵を `known_hosts` と比較し、さらにペイロードに `recipient_kem_fingerprint` を埋め込みます。これによりサーバーは保存済みの鍵と比較し、古い鍵に基づく送信を `KEY_MISMATCH`（HTTP 409）で拒否できます。

#### 3.7 秘密鍵の保存時保護

ユーザーが空でないパスフレーズを設定すると、秘密鍵ファイル（`kem_sk.bin`、`sig_sk.bin`）は `SHYK` コンテナフォーマットで書き込まれます：

| オフセット | サイズ | フィールド |
|---|---|---|
| 0 | 4 B | マジック `"SHYK"` |
| 4 | 1 B | バージョン `0x01` |
| 5 | 1 B | KDF id `0x01`（scrypt） |
| 6 | 32 B | ソルト（ランダム） |
| 38 | 4 B | scrypt `N`（LE u32、デフォルト 65536） |
| 42 | 4 B | scrypt `r`（LE u32、デフォルト 8） |
| 46 | 4 B | scrypt `p`（LE u32、デフォルト 1） |
| 50 | 12 B | ChaCha20-Poly1305 nonce（ランダム） |
| 62 | — | 暗号文（平文の鍵と同じ長さ） |
| 末尾 | 16 B | Poly1305 タグ |

62 バイトのヘッダーは AAD として結び付けられるため、KDF
パラメータへのいかなる改ざんも認証に失敗します。KDF はパスフレーズから 256 ビットの ChaCha20-Poly1305 鍵を導出します。

`SHYK` マジックを持たないファイルはレガシーな生鍵として扱われ、そのままロードされます。空のパスフレーズの場合は生の（暗号化されていない）鍵が書き込まれます。

パスフレーズは対話的に入力（ターミナルエコー無効）するか、非対話用途では `SHYAKE_PASSPHRASE` 環境変数で渡します。
`rotate` は現在のパスフレーズと新しいパスフレーズの入力を求め、新しい鍵ペアはサーバーがローテーションを確認した後にのみ、新しいパスフレーズで保存されます。

#### 3.8 ローカル暗号化下書き

`shyake compose` は下書き（私的な日記としても使えます）を設定ディレクトリ内の `drafts/<id>.json` に保存します。下書きがサーバーに触れることはありません。各下書きはメールと同じハイブリッド方式（§3.2）を使用します：ランダムな 32 バイトの対称鍵が各フィールドを ChaCha20-Poly1305 で暗号化し、その鍵はユーザー自身の KEM 公開鍵に ML-KEM-768 でカプセル化されます。

```json
{
  "version": 1,
  "draft_id": "3",
  "created": 1752400000,
  "modified": 1752400000,
  "size": 123,
  "enc_key": "<b64: kem_ct || (sym_key XOR ss)>",
  "enc_recipient": "<b64: nonce||ct||mac>",
  "enc_subject": "<b64: nonce||ct||mac>",
  "enc_body": "<b64: nonce||ct||mac>"
}
```

宛先・件名・本文はすべて暗号化された状態で保存されます。平文なのはタイムスタンプ、サイズ、id だけです。空の `enc_recipient` / `enc_subject` 文字列は空フィールドを表します——宛先のない下書きは日記のエントリです。

保存には公開鍵しか必要ないため、`compose` はパスフレーズ不要です。一覧表示・閲覧・編集・送信には KEM 秘密鍵のアンロックが必要です。下書き id はローカルで割り当てられる小さな整数です（既存の最大 id + 1、`O_EXCL` で作成）。

compose のエディタが扱う平文一時ファイルは `mkstemp`（モード 0600）で設定ディレクトリ内に作成されます——`/tmp` は決して使いません。終了後、ファイルはゼロで上書きされてから削除されます。エディタが `vim`/`nvim` の場合は `-n -i NONE` 付きで起動され、平文が swap や viminfo に漏れることはありません。

---

### 4. データベーススキーマ

Cloudflare D1（SQLite）で管理されます。マイグレーション：
`migrations/0001_initial.sql`。

#### `users`

| カラム | 型 | 備考 |
|---|---|---|
| `username` | TEXT PK | 正規表現 `^(?=.*[a-zA-Z])[a-zA-Z0-9_]{4,16}$` |
| `kem_pubkey` | TEXT | Base64 エンコードされた ML-KEM-768 公開鍵 |
| `sig_pubkey` | TEXT | Base64 エンコードされた ML-DSA-65 公開鍵 |
| `created_at` | INTEGER | UNIX タイムスタンプ |

`destroy` 時、`kem_pubkey` と `sig_pubkey` は空文字列に設定され、そのユーザーに関わるすべてのメール行とブロック行が削除されます。ユーザー行自体は、ユーザー名を永久にロックするために
**保持されます**。

#### `mail`

| カラム | 型 | 備考 |
|---|---|---|
| `mail_id` | TEXT PK | 10 文字の base58 文字列、サーバーが割り当て |
| `sender` | TEXT | ローカル名、フェデレーションでは `user@domain` |
| `recipient` | TEXT | ローカル名、フェデレーションでは `user@domain` |
| `enc_key_sender` | TEXT | 送信者向けに KEM カプセル化された鍵 |
| `enc_key_recipient` | TEXT | 受信者向けに KEM カプセル化された鍵 |
| `enc_subject` | TEXT | ChaCha20-Poly1305 暗号文、base64 |
| `enc_body` | TEXT | ChaCha20-Poly1305 暗号文、base64 |
| `size` | INTEGER | 平文本文のバイト数（UI 表示専用） |
| `signature` | TEXT | 送信者の ML-DSA-65 署名、base64 |
| `timestamp` | INTEGER | サーバーが割り当てる UNIX タイムスタンプ |

ローカルアドレスは裸の名前で保存されます：`@<INSTANCE_DOMAIN>`
サフィックスは挿入前に取り除かれます。`recipient` と `sender`
のインデックスがメールボックスクエリを支えます。`rotate` はそのユーザーが送信者または受信者であるすべてのメール行を削除します。

#### `blocks`

| カラム | 型 | 備考 |
|---|---|---|
| `blocker` | TEXT | ブロックするユーザーのユーザー名 |
| `blocked` | TEXT | ユーザー名、`user@domain`、または裸のドメイン |
| `created_at` | INTEGER | UNIX タイムスタンプ |
| PK | | `(blocker, blocked)` 複合主キー |

メール送信時、受信者が送信者のローカル名または送信者のドメインをブロックしている場合、サーバーは HTTP 403 で送信を拒否します。

---

### 5. HTTP API

すべてのエンドポイントは Cloudflare Worker 上でホストされます。ベース URL は設定された `INSTANCE_DOMAIN` です。

#### 5.1 公開エンドポイント

| メソッド | パス | 説明 |
|---|---|---|
| `GET` | `/health` | 死活チェック；D1 に問い合わせ |
| `GET` | `/api/pubkey/:username` | `kem_pubkey`、`sig_pubkey` を返す |
| `GET` | `/api/client/version` | 最新のクライアントリリースタグ |

`/api/pubkey/:username` は `user@domain` 構文をサポートします。ドメインがローカルインスタンスと異なる場合、サーバーはリクエストをリモートインスタンスへプロキシします（フェデレーションの有効化が必要）。

`/api/client/version` は GitHub Releases API をプロキシし、
`{"release": "vX.Y.Z", "pre_release": "vX.Y.Z-..."}` を返します（どちらのフィールドも欠けることがあります）。結果は KV に
1 時間キャッシュされます。§12 を参照。

#### 5.2 認証エンドポイント

すべてのエンドポイントは PoW トークン、タイムスタンプウィンドウ、
ML-DSA-65 署名（§3.3）を検証します。`POST /api/register` と
`POST /api/mail` は認証フィールドを JSON ボディで運び、その他はすべて `X-Shyake-*` ヘッダーを使用します。

| メソッド | パス | 説明 |
|---|---|---|
| `POST` | `/api/register` | 新規ユーザーを登録 |
| `POST` | `/api/mail` | メールを送信 |
| `GET` | `/api/mail?type=inbox\|sent` | メールボックスのメタデータを一覧表示 |
| `GET` | `/api/mail/:id` | 単一のメールを取得（完全な暗号文） |
| `DELETE` | `/api/mail/:id` | メールを焼却（削除） |
| `POST` | `/api/block` | ユーザーまたはドメインをブロック |
| `DELETE` | `/api/block` | ユーザーまたはドメインのブロックを解除 |
| `POST` | `/api/rotate` | 公開鍵をローテーション |
| `DELETE` | `/api/destroy` | アカウントを抹消 |

`POST /api/mail` の注目すべきステータスコード：`409`
（`KEY_MISMATCH`、送信された受信者フィンガープリントがもはや一致しない）、`410`（`USER_DESTROYED`）、`413`（ペイロードが大きすぎる）、`403`（ブロック済み、不正な PoW、または古いタイムスタンプ）。

#### 5.3 サイズ制限

サーバーは `POST /api/mail` の生の HTTP リクエストボディにハードキャップを課します。デフォルトは
**196608 バイト（192 KiB）**で、`wrangler.toml` の
`MAX_MAIL_SIZE` で設定可能です。絶対上限は 786432 バイト（768 KiB）で、Cloudflare D1 の単一行制限によるものです。

---

### 6. フェデレーション

#### 6.1 アドレッシング

- **ローカルユーザー**：`username`（`@` なし）
- **リモートユーザー**：`username@instance.domain`

クライアントは常にユーザー自身のインスタンスとのみ通信します。リモートの受信者へ送信する際、クライアントは自身の送信者文字列を
`username@<own-domain>` として修飾します。

#### 6.2 送信メールリレー

`recipient` がリモートインスタンスに属する場合：

1. クライアントは署名・暗号化済みペイロードを**送信者自身のインスタンス**に POST します（`POST /api/mail`）。
2. 送信者のインスタンスはメールをローカルの D1 データベースに保存します。
3. 同一リクエストのライフサイクル内で（`executionCtx.waitUntil` 経由）、サーバーは元の生ペイロードを `https://<recipientDomain>/api/mail` へ転送します。

受信者のインスタンスは、送信者のインスタンスから送信者の公開鍵を取得して（`GET /api/pubkey/<sender>`）、送信者の署名を独立に検証します。

送信者と受信者の両方のデータベースがメールを保存します。これにより、リモートインスタンスの可用性に関わらず、送信者側の原子性（送信ボックスの可用性）が保証されます。

#### 6.3 フェデレーションの切り替え

`wrangler.toml` の `FEDERATION_ENABLED` で設定できます。
`false` の場合、インスタンスはリモートユーザーの解決を拒否し、受信のリレーメールと送信のインスタンス間送信の両方が拒否されます。

---

### 7. 信頼モデル（TOFU + OOB）

Shyake は公開鍵管理に **Trust On First Use（TOFU）** を採用しています：

- **初回接触**：クライアントは `GET /api/pubkey/<recipient>` を問い合わせ、KEM フィンガープリントを計算し、
  `~/.config/shyake/known_hosts` に黙って追記します。
- **以降の接触**：送信のたびに、取得した鍵を `known_hosts` のエントリと比較します。不一致の場合、何も送信される前にローカルで `KEY_MISMATCH` により中止します。
- **サーバー側のダブルチェック**：ペイロードに
  `recipient_kem_fingerprint` が埋め込まれており、保存済みの鍵と一致しなくなった場合、サーバーは独立に HTTP 409 で拒否します。
- **鍵ローテーションの検出**：クライアントは致命的エラーを表示して停止します：

```
FATAL: Remote public key of recipient has changed!
RUN 'shyake fingerprint <username>' to inspect and update trust.
```

`fingerprint` コマンドは**帯域外（OOB）検証**を提供します：サーバーから現在の公開鍵を取得し、フィンガープリントを計算して
`known_hosts` と比較します。出力には GPG スタイルの 16 進グループと OpenSSH スタイルの randomart イメージが表示されます。
`--update` フラグは、ユーザーが信頼できるチャネルで新しいフィンガープリントを検証した後に `known_hosts` を書き換えます。

---

### 8. クライアントライブラリ ABI

コアライブラリは `include/shyake.h` を通じて安定した C API を公開します。ABI の破壊を防ぐため、内部状態は不透明ポインタの背後に隠されています：

```c
typedef struct shyake_ctx shyake_ctx;

shyake_ctx* shyake_init_ctx(const shyake_config *config);
void        shyake_free_ctx(shyake_ctx *ctx);

/* passphrase for secret key files (§3.7) */
void shyake_set_passphrase(shyake_ctx *ctx, const char *pp);
void shyake_set_new_passphrase(shyake_ctx *ctx, const char *pp);
```

内部構造体の定義は `src/lib/lib_internal.h` にあり、呼び出し側には公開されません。セマンティックなエラーコードは型付き列挙型（`shyake_err`）として返され、後方互換のため `SHYAKE_OK = 0`
です：

| コード | 意味 |
|---|---|
| `SHYAKE_OK` | 成功 |
| `SHYAKE_ERR` | 汎用／内部エラー |
| `SHYAKE_ERR_NETWORK` | libcurl トランスポート障害 |
| `SHYAKE_ERR_HTTP` | 予期しない HTTP ステータス |
| `SHYAKE_ERR_KEY_MISMATCH` | HTTP 409：受信者の鍵がローテーション済み |
| `SHYAKE_ERR_GONE` | HTTP 410：受信者がアカウントを抹消済み |
| `SHYAKE_ERR_NOT_FOUND` | HTTP 404 |
| `SHYAKE_ERR_FORBIDDEN` | HTTP 403 |
| `SHYAKE_ERR_CRYPTO` | 暗号操作の失敗 |
| `SHYAKE_ERR_NO_INSTANCE` | インスタンス URL が未設定 |

API グループ：コンテキストのライフサイクル、鍵生成、PoW 生成、登録、メール（`shyake_send`、`shyake_check`、`shyake_fetch`、
`shyake_check_one`、`shyake_burn`）、ローカル保存メール（`shyake_save_mail`、`shyake_read_saved`、
`shyake_check_saved_one`、`shyake_list_saved`）、ローカル下書き（`shyake_save_draft`、`shyake_list_drafts`、`shyake_read_draft`、`shyake_delete_draft`）、アカウント（`shyake_block`、`shyake_rotate`、`shyake_destroy`）、フィンガープリント（`shyake_fingerprint`）、単体ファイル暗号化（`shyake_enc_file`、`shyake_dec_file`）、自己更新（`shyake_get_latest_version`、`shyake_version_cmp`、
`shyake_self_update`）。

共有ライブラリ（`libshyake.so` / `libshyake.dylib`）はサードパーティの FFI 利用者向けです。CLI バイナリは単一ファイル配布のため、静的アーカイブ（`libshyake.a`）にリンクされます。

---

### 9. ローカル設定

設定ディレクトリ：`~/.config/shyake/`（デフォルト）、または
`-c` / `--config` で指定するカスタムパス。

| ファイル | 内容 |
|---|---|
| `config` | シェル形式の key=value 設定 |
| `kem_pk.bin` / `sig_pk.bin` | 公開鍵（生バイト） |
| `kem_sk.bin` / `sig_sk.bin` | 秘密鍵（生バイトまたは `SHYK`、§3.7） |
| `known_hosts` | 1 行につき `username fingerprint kem_pubkey` |
| `saved/<id>.json` | `shyake save` で保存された暗号化メール |
| `drafts/<id>.json` | `shyake compose` が書き込む暗号化下書き（§3.8） |

`saved/<id>.json` は `GET /api/mail/:id` が返す暗号文 JSON
そのままであり、`shyake read` の実行時にのみ復号されます。

主な `config` フィールド：

| キー | デフォルト | 説明 |
|---|---|---|
| `INSTANCE` | — | インスタンスのベース URL |
| `USERNAME` | — | 登録済みユーザー名（`register` が設定） |
| `TIME_FORMAT` | `%Y-%m-%d %H:%M` | `strftime` フォーマット |
| `TIME_FORMAT_RECENT` | — | 180 日未満のメール用フォーマット |
| `TIME_ZONE` | `auto` | 整数の時間オフセットまたは `auto` |
| `CHECK_COLUMNS` | `id,sender,subject,size,date` | `check` のレイアウト |
| `NO_COLOR` | `0` | `1` で ANSI カラーを無効化 |
| `DEFAULT_ACTION` | `0` | 0=man、1=check inbox、2=inbox --count |
| `EDITOR` | — | `compose` 用エディタ（`$VISUAL`、`$EDITOR`、`vim` の順にフォールバック） |

認識される環境変数：

| 変数 | 効果 |
|---|---|
| `SHYAKE_PASSPHRASE` | 鍵のパスフレーズを非対話的に供給 |
| `NO_COLOR` | ANSI カラーを無効化（空でない任意の値） |

---

### 10. CLI リファレンス

#### グローバルオプション

| フラグ | 説明 |
|---|---|
| `-c, --config <dir>` | 代替の設定ディレクトリを使用 |
| `--plain` | ページャー、カラー、切り詰めを無効化 |
| `--no-color` | ANSI カラー出力を無効化 |
| `--debug` | 詳細な curl ログを stderr に出力 |

#### コマンド

| コマンド | 説明 |
|---|---|
| `init [-c <dir>]` | 設定ディレクトリと鍵ペアを生成 |
| `register -u <user> -i <url>` | インスタンスに登録 |
| `whoami` | 現在のプロファイルを表示（ネットワーク不使用） |
| `send -t <to> [-s <subj>] [file]` | メールを送信（テキストのみ） |
| `send --draft <id> [-t <to>] [-s <subj>]` | 保存済み下書きを送信（成功時に削除） |
| `compose [<id>]` | 暗号化下書きの作成・編集（§3.8） |
| `check inbox\|sent [opts]` | メールボックスのメタデータを一覧表示 |
| `check <id>` | 単一メールのヘッダーを確認 |
| `check saved [<id>]` | ローカル保存メールの一覧／確認 |
| `check drafts [<id>]` | 下書きの一覧／ヘッダー確認 |
| `fetch [-r] <id>` | メールを復号して表示 |
| `save <id>` | 暗号化メールをローカルに保存 |
| `read [-r] <id>` | 保存済みメールを復号して表示 |
| `read [-r] drafts <id>` | 下書きを復号して表示 |
| `burn <id>` | メールを削除（送信者・受信者どちらでも可） |
| `block <target>` | ユーザーまたはドメインをブロック |
| `unblock <target>` | ユーザーまたはドメインのブロックを解除 |
| `rotate` | 鍵ペアをローテーション（自分の全メールを消去） |
| `fingerprint [<user>] [--update]` | 鍵フィンガープリントを比較 |
| `destroy` | アカウントとローカル設定を抹消 |
| `enc <file> [-t <user>] [-o <out>]` | 単体ファイルを暗号化 |
| `dec <file> [-o <out>]` | 単体ファイルを復号 |
| `update [stable\|preview]` | バージョン表示／自己更新 |
| `man [<command>]` | ドキュメントを表示 |
| `version` | バージョン文字列を表示 |

`check inbox|sent` は `--count`、`--json`、`--csv`、
`--no-header` を受け付けます。`send` の宛先はローカル（`username`）またはリモート（`username@instance`）を指定でき、バイナリデータは呼び出し側で base64 エンコードする必要があります。`enc`/`dec` はデバッグとテスト用途を想定しています。

---

### 11. Worker 設定（`wrangler.toml`）

| 変数 | デフォルト | 説明 |
|---|---|---|
| `INSTANCE_DOMAIN` | — | このインスタンスの正規ドメイン |
| `REGISTRATION_ENABLED` | `true` | 新規ユーザー登録を受け付ける |
| `RESERVED_USERNAMES` | `admin,system,...` | 予約済みの名前（CSV） |
| `FEDERATION_ENABLED` | `true` | フェデレーションメールの受信とリレー |
| `MAX_MAIL_SIZE` | `196608` | `POST /api/mail` の最大ペイロードバイト数 |

必須のバインディング：

| バインディング | 型 | 用途 |
|---|---|---|
| `DB` | D1 データベース | ユーザー、メール、ブロック（§4） |
| `VERSION_CACHE` | KV ネームスペース | リリース検索キャッシュ（§12） |

`CompiledWasm` ビルドルールが `mldsa65-wasm` モジュールをロードします。

---

### 12. リリースチャネルと自己更新

リリースは GitHub で 2 つのチャネルで公開されます：**stable**
（通常リリース）と **preview**（プレリリース）。サーバーエンドポイント `GET /api/client/version` は GitHub Releases API
をプロキシし、各チャネルの最新タグを選択して、結果を KV に
1 時間キャッシュします。

`shyake update` はこのエンドポイントを取得し、semver 順序（`vX.Y.Z`；同じベースバージョンではリリースがプレリリースより上位）でタグを比較します。preview チャネルは stable より新しい場合にのみ提示されます。

`shyake update stable|preview` は自己更新を実行します：

1. OS／アーキテクチャに一致するリリースアセット（`shyake-<os>-<arch>.tar.gz`）と `sha256sums.txt` を
   GitHub Releases からダウンロードする。
2. アーカイブの SHA-256 をチェックサムファイルと照合し、不一致なら中止する。
3. アーカイブを展開し、実行中のバイナリをその場で置き換える（パスは `/proc/self/exe`、`_NSGetExecutablePath`、または
   `which shyake` で解決）。

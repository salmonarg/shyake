## Shyake デプロイガイド

[English](../../DEPLOY.md) | [简体中文](../zh-CN/DEPLOY.md) | 日本語

サーバーは D1 データベースを備えた Cloudflare Worker として動作します。ただし、自分のハードウェア上でセルフホストすることも可能です。

サーバーのデプロイ方法は 2 通りあります：

* Cloudflare を使用する
* セルフホスティング

**フェデレーション**

2 つのインスタンスは、双方が `FEDERATION_ENABLED = true` になっていると自動的にフェデレーションします。追加の設定は不要です。インスタンス間のメールはサーバー間で直接ルーティングされ、クライアントは常に自分のインスタンスとのみ通信します。

受信・送信フェデレーションを無効にするには：

```toml
FEDERATION_ENABLED = false
```

### Cloudflare を使用する

前提条件：

- Node.js 18+
- Cloudflare アカウント

手順：

1. GitHub でこのリポジトリを **fork してクローン**します。

2. ターミナルで Cloudflare **Wrangler CLI** を**認証**します：

```sh
npx wrangler login
```

Wrangler が未インストールの場合、初回実行時に `npx` がインストールを促します——別途のインストール手順は不要です。

3. **D1 データベースを作成**します：

```sh
npx wrangler d1 create shyake-db
```

出力から `database_id` をコピーします。

4. **KV ネームスペースを作成**します（バージョン中継キャッシュ）：

```sh
npx wrangler kv namespace create VERSION_CACHE
```

出力から `id` をコピーします。各インスタンスは自身のクライアント向けに GitHub Releases API を中継して `shyake update`
を支えます。この KV ネームスペースはその照会結果を 1 時間キャッシュします。このバインディングは省略可能です——なくてもエンドポイントは動作しますが、リクエストごとに GitHub へアクセスします。

5. fork 内の **`server/wrangler.toml` を編集**します：

```toml
[vars]
INSTANCE_DOMAIN      = "your.domain.example" # ここを編集
REGISTRATION_ENABLED = true
RESERVED_USERNAMES   = "admin,system,support,noreply,shyake,root,postmaster"
FEDERATION_ENABLED   = true
MAX_MAIL_SIZE        = 196608 # 192 KiB；786432（768 KiB）を超えないこと

[[d1_databases]]
binding        = "DB"
database_name  = "shyake-db"
database_id    = "<your database_id>" # ここに database_id を貼り付け
migrations_dir = "migrations"

[[kv_namespaces]]
binding = "VERSION_CACHE"
id      = "<your kv namespace id>" # ここに KV ネームスペースの id を貼り付け
```

`[[d1_databases]]` ブロックは必ず存在し、正しい `database_id`
を含んでいる必要があります。これがないと Worker はデータベースバインディングを持たず、すべてのリクエストが失敗します。

カスタムドメインを持っていない場合は、デフォルトの
`*.workers.dev` URL を `INSTANCE_DOMAIN` として使用できます。

6. **データベースマイグレーションを適用**します（すべてのテーブルが作成されます）：

```sh
cd server
npx wrangler d1 migrations apply shyake-db --remote
```

Cloudflare の CI パイプラインはデータベースマイグレーションを自動では適用しません。`wrangler d1 migrations apply` を一度手動で実行する必要があります。これを省略するとデータベースが空のままになり、Worker はすべての API 呼び出しでエラーになります。

7. **デプロイ**

以下のいずれかを選択します：

**方法 A — ダッシュボード**：Cloudflare ダッシュボードで
`Compute → Workers & Pages → Create application → Continue with GitHub`
に進み（初回は `Add GitHub account` が必要な場合があります）、自分の fork を選択して次のように設定します：

| 項目 | 値 |
|-------|-------|
| Framework preset | None |
| Build command | `` |
| Deploy command | `npx wrangler deploy` |
| Root directory | `/server` |

以降、fork への push で自動的に再デプロイされます。

**方法 B — CLI のみ**：

```sh
cd server
npm install
npx wrangler deploy
```

8. **確認**

デプロイの完了を待ってから
`https://<worker>.workers.dev/health`（またはカスタムドメイン）を開きます。`200 OK` が返れば、Worker とデータベースが正常に動作しています。

### セルフホスティング

WIP

## Shyake 部署指南

[English](../../DEPLOY.md) | 简体中文 | [日本語](../ja/DEPLOY.md)

服务端以带有 D1 数据库的 Cloudflare Worker 形式运行。不过，你也可以在自己的硬件上自托管。

部署服务端有两种方式：

* 使用 Cloudflare
* 自托管

**联邦网络**

当两个实例都设置了 `FEDERATION_ENABLED = true` 时，它们会自动进行联邦网络通信，无需额外配置。跨实例邮件以
server-to-server 的方式路由；客户端始终只与自己的实例通信。

要禁用入站和出站的联邦网络通信：

```toml
FEDERATION_ENABLED = false
```

### 使用 Cloudflare

前提条件：

- Node.js 18+
- 一个 Cloudflare 账户

步骤：

1. 在 GitHub 上 **fork 并克隆**本仓库。

2. 在终端中**认证** Cloudflare **Wrangler CLI**：

```sh
npx wrangler login
```

如果 Wrangler 尚未安装，`npx` 会在首次运行时提示安装——无需单独的安装步骤。

3. **创建 D1 数据库**：

```sh
npx wrangler d1 create shyake-db
```

从输出中复制 `database_id`。

4. **创建 KV 命名空间**（版本中继缓存）：

```sh
npx wrangler kv namespace create VERSION_CACHE
```

从输出中复制 `id`。每个实例都会为自己的客户端中继 GitHub
Releases API 以支持 `shyake update`；此 KV 命名空间将查询结果缓存一小时。该绑定是可选的——没有它端点仍然可用，只是每次请求都会访问 GitHub。

5. **编辑你 fork 中的 `server/wrangler.toml`**：

```toml
[vars]
INSTANCE_DOMAIN      = "your.domain.example" # 修改此处
REGISTRATION_ENABLED = true
RESERVED_USERNAMES   = "admin,system,support,noreply,shyake,root,postmaster"
FEDERATION_ENABLED   = true
MAX_MAIL_SIZE        = 196608 # 192 KiB；不要超过 786432（768 KiB）

[[d1_databases]]
binding        = "DB"
database_name  = "shyake-db"
database_id    = "<your database_id>" # 在此粘贴你的 database_id
migrations_dir = "migrations"

[[kv_namespaces]]
binding = "VERSION_CACHE"
id      = "<your kv namespace id>" # 在此粘贴你的 KV 命名空间 id
```

`[[d1_databases]]` 块必须存在且包含正确的 `database_id`。缺少它的话 Worker 没有数据库绑定，所有请求都会失败。

如果没有自定义域名，可以使用默认的 `*.workers.dev` URL 作为
`INSTANCE_DOMAIN`。

6. **应用数据库迁移**（创建所有表）：

```sh
cd server
npx wrangler d1 migrations apply shyake-db --remote
```

Cloudflare 的 CI 流水线不会自动应用数据库迁移。你必须手动运行一次 `wrangler d1 migrations apply`。跳过这一步会导致数据库为空，
Worker 的每次 API 调用都会报错。

7. **部署**

选择以下方式之一：

**方式 A —— 控制台（Dashboard）**：在 Cloudflare 控制台中进入
`Compute → Workers & Pages → Create application → Continue with GitHub`
（首次使用可能需要先 `Add GitHub account`），选择你的 fork，并设置：

| 字段 | 值 |
|-------|-------|
| Framework preset | None |
| Build command | `` |
| Deploy command | `npx wrangler deploy` |
| Root directory | `/server` |

之后推送到你的 fork 时会自动重新部署。

**方式 B —— 仅使用 CLI**：

```sh
cd server
npm install
npx wrangler deploy
```

8. **验证**

等待部署完成，然后打开
`https://<worker>.workers.dev/health`（或你的自定义域名）。返回 `200 OK` 即表示 Worker 和数据库工作正常。

### 自托管

WIP

## Shyake

[English](../../README.md) | 简体中文 | [日本語](./README.ja.md)

### 概述

Shyake 是一个由**后量子密码学**驱动的**端到端加密邮件系统**，旨在提供一种去中心化，抗审查，防嗅探的通信方式。

服务端运行在 Cloudflare Workers 上，因此任何人都可以零成本托管自己的实例。你也可以在自己的硬件上自托管服务端，而不使用 Cloudflare 全球网络。

### 文档

部署自己的实例：

* [**部署指南**](./zh-CN/DEPLOY.md)

面向开发者：

* [**开发者指南**](./zh-CN/DEV.md)
* [**技术规格**](./zh-CN/SPEC.md)

### 安装

从 [GitHub Releases](https://github.com/salmonization/shyake/releases)
下载二进制文件，解压后复制到 `$PATH` 中的目录：

```sh
sudo cp ./shyake /usr/local/bin/
```

测试：

```sh
shyake version
```

安装完成后，可以使用 `shyake update` 进行客户端更新（参见下文的 update 命令）。

### 使用方法

**首次使用**：

```sh
# 初始化本地配置并生成密钥对
shyake init

# 在实例上注册，-u 指定用户名，-i 指定实例 URL
shyake register -u salmon -i https://shyake.eee.coffee
```

`init` 会要求你设置一个用于保护私钥的口令（passphrase），留空则不设置口令。使用密钥的命令会提示输入该口令。

配置存储在 `~/.config/shyake/`。

你可以在初始化时指定目录来创建多个用户配置：

```sh
shyake init -c path/to/your/dir
```

这种情况下，使用该配置文件时总是需要加上 `-c` 选项。

使用 `whoami` 命令查看当前配置文件。

```sh
shyake whoami
```

运行 `shyake man` 查看所有命令列表，运行 `shyake man <command>`
查看每个命令的详细用法。

**check 命令**：

```sh
shyake check inbox
shyake check sent
```

可以使用 `--csv` 和 `--json` 将输出格式化，以便于机器解析。也可以使用 `--no-header` 关闭列标题，或使用 `--count` 输出数量。

查看某封邮件的邮件头：

```sh
shyake check fQBjZnvJ56
```

列出本地保存的邮件（参见下文的 save 命令），或查看某封已保存邮件的邮件头：

```sh
shyake check saved
shyake check saved fQBjZnvJ56
```

**send 命令**：

```sh
shyake send -s "This is the subject" -t flat_white < body.txt
```

如果缺少 `-s`，输入文件的第一行将作为主题。

```sh
shyake send -t flat_white < content.txt
```

请注意，主题长度不得超过 128 字节。

使用 `username@instance` 作为收件人，可将邮件发送给外部实例上的用户。

```sh
shyake send -s "Hello" -t flat_white@shyake.example.com < body.txt
```

你也可以使用 heredoc，但请留意你的 shell 历史记录。

```sh
shyake send -s "This is the subject" -t flat_white <<EOF
Hello, this is the mail body.
EOF
```

Shyake 只传输文本。二进制数据在发送前必须先进行 base64 编码。

```sh
# 发送一张小图片
base64 image.png | shyake send -t flat_white -s "image.png"

# 发送一个小压缩包
tar czf - ./source | base64 | shyake send -t flat_white -s "source.tar.gz"
```

**fetch 命令**：

获取一封邮件并解密。

```sh
shyake fetch fQBjZnvJ56
```

如果想将某封邮件导出为包含邮件头的纯文本（不要与下文的 `save`
命令混淆，后者存储的是加密邮件）：

```sh
shyake fetch fQBjZnvJ56 --no-color > exported-mail.txt
```

只输出正文时，使用 `-r` 或 `--raw`。

```sh
shyake fetch fQBjZnvJ56 --raw
shyake fetch fQBjZnvJ56 -r > exported-mail.txt
```

解码收到的 base64 编码二进制数据：

```sh
# 解码收到的图片
shyake fetch fQBjZnvJ56 -r | base64 -d > image.png

# 解码并解压收到的压缩包
mkdir -p ./output
shyake fetch fQBjZnvJ56 -r | base64 -d | tar xzf - -C ./output
```

**save 和 read 命令**：

`save` 从服务器获取加密邮件并存储到
`~/.config/shyake/saved/<id>.json`。此阶段邮件不会被解密。

```sh
shyake save fQBjZnvJ56
```

`read` 解密并显示一封已保存的邮件。输出与 `fetch` 相同，同样支持 `-r`/`--raw`。

```sh
shyake read fQBjZnvJ56
```

**fingerprint 命令**：

显示你自己的指纹：

```sh
shyake fingerprint
```

查看通信对象的指纹：

```sh
shyake fingerprint flat_white
```

如果通信对象轮换了密钥对，你可以更新他们的指纹。**警告：在运行更新命令之前，请务必通过额外的可信带外渠道（例如当面确认或通过其他平台）核实新指纹，以防身份冒充。**

```sh
shyake fingerprint flat_white --update
```

**burn 命令**：

删除一封邮件。

```sh
shyake burn fQBjZnvJ56
```

**block 和 unblock 命令**：

屏蔽或取消屏蔽某个用户或实例。目标可以是用户名或实例 URL。

```sh
shyake block flat_white
shyake block https://bad.example.com
shyake unblock flat_white
```

**update 命令**：

`shyake update` 显示已安装版本和可用版本。使用 `stable` 或
`preview` 安装对应渠道的最新版本。

```sh
shyake update
shyake update stable
shyake update preview
```

**rotate 命令**：

轮换你的密钥对，并清除所有与你相关的收发邮件。

```sh
shyake rotate
```

**destroy 命令**：

删除你的本地配置和密钥对，并销毁你在实例上的账户。所有与你相关的收发邮件都会被清除。你的用户名将被永久锁定，无法在该实例上再次注册。

```sh
shyake destroy
```

### 高级用法

可以使用 `--no-color` 关闭彩色输出。标准的 `NO_COLOR` 环境变量亦受支持。

```sh
shyake check inbox --no-color
shyake check fQBjZnvJ56 --no-color
shyake fetch fQBjZnvJ56 --no-color
```

对 `check` 和 `fetch` 命令使用 `--plain` 可以禁用分页器、颜色和截断。

你可以编辑配置文件（位于 `~/.config/shyake/config` 或其他配置文件所在目录）来优化设置，例如修改 `check` 命令的列布局。

```sh
# Date & Time format (strftime format)
# RECENT: less than 180 days old.
# ISO 8601 format
TIME_FORMAT="%Y-%m-%d %H:%M"
# POSIX format
# TIME_FORMAT="%b %d  %Y"
# TIME_FORMAT_RECENT="%b %d %H:%M"

# Time zone (default: auto)
# Integer offset in hours: 0=UTC, 8=UTC+8, -6=UTC-6
TIME_ZONE=auto

# Display columns for `check` command
CHECK_COLUMNS=id,sender,subject,size,date

# Disable colors (1 = disable)
# NO_COLOR=0

# Default action when running without arguments
# 0 = man, 1 = check inbox, 2 = check inbox --count
DEFAULT_ACTION=0
```

设置 `SHYAKE_PASSPHRASE` 环境变量可以非交互式地提供密钥口令，可用于脚本。

使用 `enc` 和 `dec` 可以通过 ML-KEM-768 + ChaCha20-Poly1305
加密或解密独立文件。这些命令用于调试/测试目的。

```sh
# 使用你自己的公钥加密（输出默认为 <file>.enc）
shyake enc secret.txt

# 为某个收件人加密，并指定输出路径
shyake enc secret.txt -t flat_white -o secret.enc

# 使用你的 KEM 私钥解密（未指定 -o 时输出到 stdout）
shyake dec secret.enc -o secret.txt
```

使用 `--debug` 将详细的 `curl` 日志（握手、HTTP 头和内部变量）输出到 `stderr`。

### 许可证

BSD 2-Clause License

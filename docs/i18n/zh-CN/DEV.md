## Shyake 开发者指南

[English](../../DEV.md) | 简体中文 | [日本語](../ja/DEV.md)

> Translated by Claude Fable 5

本文档帮助你参与 Shyake 的开发，或二次开发。

**目录**：

- [客户端](#客户端)
  * [依赖](#依赖)
  * [构建](#构建)
  * [安装](#安装)
  * [测试](#测试)
- [服务端](#服务端)
  * [本地开发](#本地开发)

## 客户端

### 依赖

`liboqs` 在所有平台上都是静态链接，因此二进制文件对它没有运行时依赖。`libcurl` 和 `libcrypto` 在所有平台上仍为动态链接。

依赖（仅构建时需要）：

| 库 | 用途 |
|---------|---------|
| `liboqs` | ML-KEM-768 与 ML-DSA-65 |
| `libcurl` | HTTP 传输 |
| `openssl`（`libcrypto`） | SHA-256 指纹计算 |

macOS 上使用 Homebrew：

```sh
brew install liboqs curl openssl@3
```

Arch Linux 上：

```sh
sudo pacman -S cmake curl openssl
# 从源码构建 liboqs：参见下方说明
```

Debian/Ubuntu 上：

```sh
sudo apt install cmake libcurl4-openssl-dev libssl-dev
# 从源码构建 liboqs：参见下方说明
```

Termux（Android）上：

```sh
pkg install clang cmake make curl-dev openssl-dev
# 从源码构建 liboqs：参见下方说明
```

**构建 liboqs**

从源码编译 `liboqs` 时（例如在 GNU/Linux 或 Termux 上），必须进行最小化构建。启用全部算法构建 `liboqs` 会使二进制体积急剧膨胀（约 20MB）。

只构建 Shyake 所需算法（ML-KEM-768 和 ML-DSA-65）的 `liboqs`，运行：

```sh
git clone --depth 1 \
          --single-branch -b main \
          https://github.com/open-quantum-safe/liboqs.git
cd liboqs
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release \
      -DOQS_BUILD_ONLY_LIB=ON \
      -DOQS_USE_OPENSSL=ON \
      -DOQS_MINIMAL_BUILD="KEM_ml_kem_768;SIG_ml_dsa_65" \
      ..
make -j$(nproc)
sudo make install
```

在 **Termux** 中编译时，必须指定安装前缀（`$PREFIX`）并省略
`sudo`：

```sh
cmake -DCMAKE_BUILD_TYPE=Release \
      -DOQS_BUILD_ONLY_LIB=ON \
      -DOQS_USE_OPENSSL=ON \
      -DOQS_MINIMAL_BUILD="KEM_ml_kem_768;SIG_ml_dsa_65" \
      -DCMAKE_INSTALL_PREFIX=$PREFIX \
      ..
make -j4
make install
```

### 构建

```sh
cd client
make
```

产物：

| 文件 | 说明 |
|------|-------------|
| `bin/shyake` | CLI 二进制文件（所有平台均静态链接 `liboqs`） |
| `lib/libshyake.a` | 用于 FFI 的静态库 |
| `lib/libshyake.so` 或 `lib/libshyake.dylib` | 用于 FFI 的共享库 |

### 安装

将二进制文件复制到 `$PATH` 中的任意目录：

```sh
cp bin/shyake /usr/local/bin/
```

### 测试

针对本地开发服务器运行端到端测试套件：

```sh
# 终端 1
cd server && npx wrangler dev --local

# 终端 2
cd client && make
bash tests/e2e_test.sh
```

## 服务端

### 本地开发

```sh
npx wrangler dev --local
```

Worker 默认监听 `http://localhost:8787`。

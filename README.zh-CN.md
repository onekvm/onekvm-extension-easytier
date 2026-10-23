# OneKVM EasyTier 扩展

[English](README.md) | 简体中文

通过 [EasyTier](https://github.com/EasyTier/EasyTier) 把 OneKVM 加入去中心化
组网，而不把 `easytier-core` 放进基础镜像。本仓库只包含生命周期控制器和设置
页面；上游二进制由 `onekvm-distro` 的 OpenEmbedded 配方打包。

> **构建说明：** 正式软件包由 `onekvm-distro` 中的 OE 配方构建；本仓库不提供
> 独立的正式构建流程。

## 功能

- 用相同的网络名称和密钥加入 EasyTier 网络
- 只发起出站连接，连到公共或自建共享节点
- 可选自动分配或静态虚拟 IPv4
- 网络密钥写入私有配置文件，不会出现在进程参数里

本扩展只做客户端，不提供 EasyTier 共享节点控制面。

## 安装与启用

```sh
onekvm-extension enable easytier
onekvm-extension disable easytier
```

NanoKVM 的 `extension_memory_budget` 是 64M。启用 EasyTier（32M）时，请先关掉
容器或 RustDesk 等占用较大的插件。

## 许可证

GNU General Public License v3.0，见 [`LICENSE`](LICENSE)。随包提供的
EasyTier 继续使用上游 LGPL-3.0 许可证。

可选屏幕 App 的 TypeScript/TSX 源码位于
[`screen-app/src`](screen-app/src)，按组件、状态和语言包拆分。
构建方式见 [`screen-app/README.md`](screen-app/README.md)；JS/PAK 由打包流程生成，不提交到 Git。

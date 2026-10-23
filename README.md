# OneKVM EasyTier Extension

English | [简体中文](README.zh-CN.md)

Connect OneKVM to a decentralized [EasyTier](https://github.com/EasyTier/EasyTier)
mesh without adding `easytier-core` to the base image. This repository only
contains the lifecycle controller and settings page. OpenEmbedded in
`onekvm-distro` packages the upstream binaries.

> **Build note:** Release packages are built by the OpenEmbedded recipes in
> `onekvm-distro`; this repository does not provide a standalone release build.

## Features

- Joins an EasyTier network with a shared network name and secret
- Connects outbound to public or self-hosted shared nodes
- Optional DHCP or static virtual IPv4
- Network secret is written to a private config file; it is never passed on
  the command line

This extension is a client only. It does not run an EasyTier shared-node
control plane.

## Install and enable

```sh
onekvm-extension enable easytier
onekvm-extension disable easytier
```

NanoKVM `extension_memory_budget` is 64M. Enable EasyTier (32M) only when
another large plugin such as container or RustDesk is disabled.

## License

GNU General Public License v3.0, see [`LICENSE`](LICENSE). Upstream EasyTier
keeps its own LGPL-3.0 license.

The optional screen App has modular TypeScript/TSX source in
[`screen-app/src`](screen-app/src). See [`screen-app/README.md`](screen-app/README.md)
for its components and build command. JS/PAK runtime files are generated during
packaging and are not tracked in Git.

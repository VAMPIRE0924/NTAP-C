# NTAP-C

NTAP-C 是远端客户机客户端。Windows 端面向普通用户提供图形界面：双击 `ntap-c.exe`，输入连接信息，点击 Connect 后自动写入配置、检查 TAP，并在需要时拉起管理员权限准备 TAP-Windows6 适配器。

Linux 端保留命令行客户端，适合服务器、测试环境和自动化部署。

## 下载和部署

正式部署请下载 GitHub Release 里的编译产物，不要直接拿源码目录里的临时构建文件部署。

最新版本：

https://github.com/VAMPIRE0924/NTAP-C/releases/latest

Windows 客户端下载：

```text
NTAP-C-<version>-windows-x64.zip
```

解压后客户只需要运行：

```text
bin\ntap-c.exe
```

窗口里填写：

- NTAP-A 地址
- TAP 用户名
- TAP 密码
- Network ID
- TAP 适配器名称，默认 `ntap-c0`

点击 Connect 后，GUI 会调用同目录的 `ntap-c-cli.exe` 执行实际连接。`ntap-c-cli.exe` 不面向普通客户，只用于校验、自动化和服务化部署。

## Windows TAP

当前 Windows 数据面支持 TAP-Windows6/OpenVPN 风格适配器。Wintun/WireGuard 适配器目前只做发现，后续需要单独的数据面后端。

如果客户机器没有 TAP-Windows6，GUI 会尝试调用 Release 包里的：

```text
install\ensure-tap-windows.ps1
```

该脚本需要管理员权限，并会在系统已有 OpenVPN TAP 工具或驱动文件时创建/准备 TAP 适配器。没有驱动时，需要先安装 OpenVPN TAP-Windows6 驱动。

验证包内容和 TAP 环境：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\validate\validate-tap-windows.ps1 -PackageZip .\NTAP-C-<version>-windows-x64.zip
```

在必须存在 TAP-Windows6 的机器上使用严格校验：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\validate\validate-tap-windows.ps1 -PackageZip .\NTAP-C-<version>-windows-x64.zip -RequireTap
```

## Linux 客户端

Linux 客户端下载：

```text
NTAP-C-<version>-linux-x64.tar.gz
```

基本命令：

```sh
bin/ntap-c -c conf/ntap-c.conf.example check-env
bin/ntap-c -c conf/ntap-c.conf.example run
```

Linux 端需要 `/dev/net/tun` 和创建 TAP 的权限。

## 源码构建

```sh
make
make config-test
```

在 Windows/MSYS2 下会生成两个入口：

```text
build/msys2/bin/ntap-c.exe      客户图形界面
build/msys2/bin/ntap-c-cli.exe  命令行和自动化入口
```

在 Linux 下生成：

```text
build/linux/bin/ntap-c
```

## 目录

```text
src/c/          NTAP-C 客户端源码
src/common/     三端共享协议、日志、网络、时间、buffer 等公共代码
conf/           最小配置示例
scripts/windows Windows TAP 检查、安装和远程验证脚本
Makefile        单仓库构建入口
```

## 部署注意

- 客户使用 Windows GUI，不要求客户手敲长命令。
- TAP 密码不要写进命令历史；GUI 会把配置写到本机 ProgramData 目录。
- Windows 真机严格验证需要实际安装 TAP-Windows6 适配器。
- Release 包和源码提交是两条线：源码仓库保持干净，编译产物只放 GitHub Release。

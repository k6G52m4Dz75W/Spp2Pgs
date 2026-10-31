# 更新记录

[English](CHANGELOG.md) | [简体中文](CHANGELOG.zh.md)

本文件记录本项目的所有重要变更。
格式遵循 [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)，
版本号遵循 [语义化版本](https://semver.org/lang/zh-CN/)。

本记录从 0.10.0 开始。此前的版本由上游
[subelf/Spp2Pgs](https://github.com/subelf/Spp2Pgs) 发布，最后一个为 2016 年 5 月的 0.9.3.7。

## [0.10.0] - 2026-10-31

### 新增
- `-H[<nits>]` 输出 HDR 调色板，即 BT.2020 搭配 ST 2084（PQ）传递函数，用于与 HDR 视频配对。
  不加该参数则保持 BT.709 SDR，与以往版本逐字节一致。
  - 数值表示白色映射到多少尼特。可省略，可紧贴也可用空格分隔，因此 `-H`、`-HDR`、`-H203`
    与 `-H 203` 产出相同的流。后缀若不是数字会给出提示，而不是默默忽略。
  - 默认 203 尼特，即 ITU-R BT.2408 的图形白。超过 500 尼特的值会被钳制并给出警告。
- `build_sppf.bat` 与 `build_core.bat`，用 Visual Studio Build Tools 完成全量编译，
  产物输出到 `bin\`。
- `README.zh.md` 与 `CHANGELOG.zh.md`，与英文原版并列。

### 变更
- 版本号只在 `Version.h` 一处定义，三个资源脚本与 .NET 程序集信息均从那里读取。
  此前各文件各自手写，数值已经互相不一致。
- 版本号提升至 0.10.0，采用三段而非四段。Windows 版本资源的第四段固定为 0。
- xy-VSFilter 子模块指向本项目维护的分叉，以便携带下述修复。

### 修复
- 末行没有换行符的字幕不再被丢弃。视时间轴情况，该行会整体消失，或文字保留而画面丢失。
- 修复从上游继承的 x64 光栅化器指针截断问题。

## 链接

[0.10.0]: https://github.com/k6G52m4Dz75W/Spp2Pgs/releases/tag/0.10.0

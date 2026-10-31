# Spp2Pgs

[![build](https://github.com/k6G52m4Dz75W/Spp2Pgs/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/k6G52m4Dz75W/Spp2Pgs/actions/workflows/build.yml)

[English](README.md) | [简体中文](README.zh.md)

把 ASS/SSA、SRT、SAMI 与 DVD 字幕转换为蓝光 PGS sup 文件，可选输出
HDR（BT.2020 ST 2084）调色板。

本项目产出一个命令行程序和一个 CLR 动态库，两者的核心是一个静态库 libspp2pgs。

另有一个外部 DLL `xy-VSSppf` 负责处理字幕文件。它基于 xy-VSFilter 的一个分叉版本，
把字幕在指定时刻渲染成位图，字幕格式的解析与特效渲染都在这里完成。它必须与 exe
放在同一目录，否则程序无法启动编码。

## 最新版本

**0.11.0** — 2026-10-31 · [完整更新记录](CHANGELOG.zh.md) · [发布页](https://github.com/k6G52m4Dz75W/Spp2Pgs/releases/tag/0.11.0)

- **渲染器换代。** xy-VSFilter 同步到上游最新提交 `532b756`（2023-02），此前分叉
  基于 2015-10 的上游状态。所合入的改动除下一条外均为播放器侧行为，PGS 输出不受影响。
- **开始识别 `LayoutResX` / `LayoutResY`。** 字幕声明这两个头时，渲染先按该分辨率
  栅格化再放大到输出画布：字号、位置、换行不变，字形与描边的轮廓尺寸不变（仅抗锯齿
  过渡带变宽），`.sup` 约大 5–6%。未声明时输出与 0.10.1 逐字节相同。见下方
  「PlayRes 与 LayoutRes」。
- **构建可全量重建。** `build_sppf.bat rebuild` 走 `/t:Rebuild`，日志与增量构建
  分开保存，两者的警告数量因此可以直接对照。
- **消除 LNK4315。** 调试信息不再使用工具链已移除的 `/DEBUG:FASTLINK` 格式。
- **仓库侧**：两份 README 顶部加了构建徽章；工作流在每次推送到 `main` 时编译两个
  二进制并跑回归，超时 30 分钟；`.gitattributes` 把工作流文件钉为 LF。
- **退出码 3** 的表述在各处统一为「未处理的异常」。

以上只是要点，条目完整见 [`CHANGELOG.zh.md`](CHANGELOG.zh.md)。

下面分别说明这四个部分。

## Spp2Pgs*.exe
把字幕转换为 PGS sup 文件的可执行程序。  
用法：  
`Spp2Pgs -i "X:\Saya1011con.ass" -s 1080 -r 23 "X:\Saya1011con.sup"`  
选项字母不区分大小写。执行 `Spp2Pgs -help` 可查看完整帮助。

只要 xy-VSSppf 能打开的文本字幕都可以作为输入，格式由解析文件判定，不看后缀。
实测通过的有 ASS 与 SSA、SRT、SAMI（`.smi`）、MicroDVD（`.sub`）、MPL2（`.txt`）
与 RealText（`.rt`）。VobSub 需要 `.idx` 与 `.sub` 成对提供，尚未实测。
`.sup` 不能作为输入，它是图形流而不是文本。

## libspp2pgs & Spp2PgsNet*.dll
核心库及其 .NET 框架的 CLR 包装。三个类承担主要职责：  
* PgsEncoder，编码并写出 pgs 文件。
* FrameStream，读取字幕并把字幕渲染为图像。
* Spp2Pgs，其余一切的入口；同时负责从 FrameStream 读取图像并交给 PgsEncoder。

C# 示例：  
```csharp
//class MySettings : IS2PSettings { ... }
//class MyLogger : IS2PLogger { ... }

using (Spp2Pgs tS2P = new Spp2Pgs(new MySettings(), new MyLogger()))	//创建 Spp2Pgs 对象
using (var tCtx = tS2P.CreateSubPicProviderContext())	//创建 SppContext 对象，创建 Spp 时必需
using (var tOutputStream = new FileStream(@"X:\Sayas1011con.sup", FileMode.Create))	//创建输出文件
using (var tOutput = tS2P.CreatePgsEncoder(tOutputStream, BdViFormat.Vi1080i, BdViFrameRate.Vi23)) //创建 PgsEncoder
{
    tOutput.RegistAnchor(0);    //确保在 pts==0 处有一个 epoch

	//创建 Spp，处理字幕文件时必需
	using (var tSpp = tS2P.CreateSubPicProvider(tCtx, new FileInfo(@"X:\Illya.ass")))
	{
		//创建 Advisor，为 FrameStream 提供额外信息
		var tAdv =
			tS2P.CreateSppFrameStreamAdvisor(tSpp, BdViFormat.Vi1080p, BdViFrameRate.Vi23,	//渲染格式与帧率
			-1, -1, 0);	//从头渲染到末尾，不使用偏移
		//创建 FrameStream
		using (var tInput = tS2P.CreateFrameStream(tSpp, tAdv))
		{
			//从 FrameStream 编码到 PgsEncoder
			tS2P.Encode(tInput, tOutput, null);
			//完成
		}
	}
}
```

## xy-VSSppf*.dll
xy-VobSub SubPicProvider Factory。这是一个 COM 动态库，向 Spp2Pgs 暴露
`ISubPicProviderAlfa` 接口，是 Spp2Pgs 运行所必需的组件。


## HDR 输出（BT.2020 / ST 2084）
UHD 蓝光在同一张盘上保留两套字幕流：SDR 图形位于 PID `0x1200-0x121F`，
HDR 图形位于 `0x12A0-0x12BF`。PG 流本身没有任何字段声明色彩空间，
因此调色板里的数值就是播放器判断如何解读的唯一依据。这也是一份 .ass
若要同时服务 SDR 与 HDR 视频，必须导出两个 .sup 文件的原因。

`-h` 输出 BT.2020 ST 2084（PQ）版本。不加该参数则保持 BT.709 SDR。

`-h[<nits>]` 指定白色映射到多少尼特。数值可省略，可紧贴也可用空格分隔，
所以 `-h`、`-hdr`、`-h203`、`-h 203` 产出完全相同的流；后缀若不是数字会给出提示，
而不是默默忽略。默认值为 203 尼特，即 BT.2408 的图形白 —— 平坦白色图形应处的电平，
也是 HDR 制作中白色字幕的推荐电平。UHD 蓝光格式本身只规定色彩编码
（ST 2084 的 CLUT 数值乘以 4），并未规定字幕应该多亮，其白皮书亦自承
HDR 字幕流程当时仍在评估之中，这个空缺由 BT.2408 填补。

实测六张零售 UHD 盘，白点落在 201–401 尼特，另有一张离群值 667。默认值刻意取在
偏低且依据明确的一端：字幕稍暗无碍，刺眼则不适。若某张盘更亮，可用 `-h250` 或
`-h300` 对齐。上限为 500 尼特，即多数 HDR 母版峰值的一半，更大的值会被钳制。

PQ 是绝对亮度，因此峰值再高的显示设备也不会把偏暗的盘提亮 —— 色调映射只会向下压，
不会向上抬。而一条比画面更亮的字幕换来的是错误的注意力：它成为帧内最亮的物体，
自适应色调映射会把它当作该场景的峰值，从而把背后的画面压暗。

调色板仍为 8 位。PQ 作用于数值，不改变位深。请将此输出与 HDR 视频配对使用。

不做 PQ 解码的播放器会把这些数值当作普通 8 位亮度显示，结果偏暗。实测默认的
203 尼特输出：RGB 白落在 147/255，而同一段文字在 SDR 版本中落在 253/255，
因此 HDR 文件看起来像 58% 的灰 —— 也就是常见的「字幕发灰」现象。这既不是文件缺陷，
也不是重封装导致的信息丢失：直接用 ISO 播放原盘同样如此。正确的 HDR 链路下，
`-h203` 的 143 与 SDR 输出的 234 应显示为同一亮度，因为两者都是 203 尼特。
如果 HDR 版本看起来比 SDR 版本更暗，说明该播放器对 PG 流跳过了 PQ 处理 ——
在它支持之前，请使用 SDR 输出。

## PlayRes 与 LayoutRes

三个尺寸，来源各不相同：

| 名称 | 来源 | 缺省 | 作用 |
|---|---|---|---|
| 输出画布 | 命令行 `-s` | 无；缺少时该次转换报错 | PGS 图像的最终尺寸，另两项的缩放目标 |
| 脚本坐标系 `PlayRes` | 字幕头 `PlayResX` / `PlayResY` | 384×288 | 字号、边距、对齐的换算基准 |
| 栅格化基准 `LayoutRes` | 字幕头 `LayoutResX` / `LayoutResY` | 等于输出画布 | `\be`、`\blur`、`\frx`、`\fry` 与描边、阴影的缩放基准 |

`PlayRes` 与 `LayoutRes` 相互独立：前者定义坐标系，后者定义栅格化尺寸。

### PlayRes

记 k = 输出高 ÷ `PlayResY`，则

    字号(px) = Fontsize × k
    边距(px) = MarginV  × k

`PlayRes` 缺省为 384×288，1080p 输出下 k = 1080 ÷ 288 = 3.75，即 `Fontsize: 14`
渲染为 52.5 px。`PlayRes` 写成目标分辨率时 k = 1，数值即像素：

    PlayResX: 1920
    PlayResY: 1080

只写其中一个时，另一个按 4:3 推算：`PlayResX: 1280` 对应 `PlayResY: 1024`，
其余按 3/4 与 4/3 互换。

`ScaledBorderAndShadow: yes` 时，描边与阴影的横纵值分别缩放：

    描边(横) = Outline × (输出宽 ÷ PlayResX)
    描边(纵) = Outline × (输出高 ÷ PlayResY)
    阴影(横) = Shadow  × (输出宽 ÷ PlayResX)
    阴影(纵) = Shadow  × (输出高 ÷ PlayResY)

横纵之比 = (输出宽 ÷ 输出高) ÷ (PlayResX ÷ PlayResY)，即画面宽高比 ÷ `PlayRes`
宽高比，仅当两者相等时为 1。缺省的 384×288 为 4:3，配 16:9 画面时该比值为 4/3。

`ScaledBorderAndShadow` 不写时，`Outline` 与 `Shadow` 的数值即像素，不缩放。

### LayoutRes

`LayoutResX` / `LayoutResY` 定义栅格化尺寸。两者都存在且大于 0 时生效：先按该
尺寸栅格化，再放大到输出画布。

- 字号、位置、换行：不变
- 描边与阴影：`ScaledBorderAndShadow` 为 `yes` 时不变，否则按 `LayoutRes` 缩放
- 字形与描边的边缘：轮廓尺寸不变，抗锯齿过渡带随放大变宽
- `.sup` 体积：约增加 5–6%

`LayoutRes` 等于输出画布、或两个头都不存在时，输出与 0.10.1 逐字节相同。

本工具不忽略这两个头：它们声明的是作者选定的栅格化基准，忽略即偏离文件原意。
工具的输出画布固定，自身不需要跨分辨率复用，因此为本工具编写的字幕无需声明；
带这两个头的字幕通常出自其他工具，按声明渲染即与作者所见一致。0.10.1 不识别
它们，一律按输出画布栅格化，需要旧版输出时移除这两行。

## 编译
仓库根目录有两个脚本，按顺序执行：

1. `build_sppf.bat` 编译字幕提供器，把 `xy-VSSppf64.dll` 输出到 `bin\`。
2. `build_core.bat` 先编译 `libspp2pgs`，再编译 `Spp2Pgs64.exe`，同样输出到 `bin\`。

两者都要求带 v145 工具集与 MFC 库的 Visual Studio Build Tools，安装路径在脚本顶部设置。
`build_sppf.bat` 需要先拉取子模块：

```
git submodule update --init --recursive
```

## 版本
版本号只在 `Version.h` 一处定义，三个资源脚本与 .NET 程序集信息都从那里读取。
它有三个有效部分，即 `MAJOR.MINOR.PATCH`；Windows 版本资源强制要求的第四段固定为 0。
已发布版本见 [CHANGELOG.zh.md](CHANGELOG.zh.md)，最新版本在
[发布页](https://github.com/k6G52m4Dz75W/Spp2Pgs/releases)。

## 命名
`Spp` 是 `SubPicProvider` 的缩写，即 VSFilter 一系渲染器提供的 COM 接口。本工具通过
该接口取得位图，不自行解析字幕文件，因此名字指向输入接口，而不是某种字幕格式。

输出文件称为「PGS sup」：PGS 是格式（HDMV PG 流），`.sup` 是惯用后缀。`.pgs` 文件
字节相同，但许多工具按后缀筛选文件，故示例统一使用 `.sup`。HD DVD 曾把 `.sup` 用于
另一种格式，故名称中显式写出格式。

## 特别致谢
* 本项目基于 [avs2bdnxml](http://www.ps-auxw.de/avs2bdnxml/) 起步
* xy-VSSppf 是 [xy-VSFilter](https://github.com/Cyberbeing/xy-VSFilter) 的一个分叉版本

## 许可证
Spp2Pgs 以 GNU 通用公共许可证第 3 版或其后续版本分发，见
[License.txt](License.txt)。版权归 Giton Xu（2016）。本分叉所做的修改沿用同一许可证。

`xy-VSSppf` 由 [xy-VSFilter](https://github.com/Cyberbeing/xy-VSFilter) 编译，后者以
GPL 第 2 版或其后续版本授权。该版本允许按第 3 版使用，故整体以 GPLv3 分发。该部分
的条款见 [License.txt](misc/xy-VSFilter/src/License.txt)，随二进制提供为
`xy-VSFilter-license.txt`。

发布包另附一份源码归档，包含 xy-VSFilter 源码树 —— GitHub 自动生成的归档不含该部分。

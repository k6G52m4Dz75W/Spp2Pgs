# Spp2Pgs

[English](README.md) | [简体中文](README.zh.md)

把 ASS/SSA、SRT、SAMI 与 DVD 字幕转换为蓝光 PGS sup 文件，可选输出
HDR（BT.2020 ST 2084）调色板。

本项目产出一个命令行程序和一个 CLR 动态库，两者的核心是一个静态库 libspp2pgs。

另有一个外部 DLL `xy-VSSppf` 负责处理字幕文件。它基于 xy-VSFilter 的一个分叉版本，
把字幕在指定时刻渲染成位图，字幕格式的解析与特效渲染都在这里完成。它必须与 exe
放在同一目录，否则程序无法启动编码。

## 最新版本

**0.10.1** — 2026-10-31 · [完整更新记录](CHANGELOG.zh.md) · [发布页](https://github.com/k6G52m4Dz75W/Spp2Pgs/releases/tag/0.10.1)

- **HDR 输出。** `-h[<nits>]` 输出 BT.2020 ST 2084（PQ）调色板，用于与 HDR 视频配对。
  白色默认 203 尼特，即 BT.2408 的图形白，上限 500。不加该选项则输出不变。
- **末行不再丢失**：字幕文件结尾没有换行符时，最后一行照常编码。
- **修复 x64 光栅化器的指针截断**，该问题继承自上游。
- **出错时说清原因。** 临时目录、可用空间与所需空间、Windows 错误码及通俗提示、
  输入流打开失败的 HRESULT，以及 xy-VSSppf DLL 缺失或位数不符，都会明确报出 ——
  旧版只给出 `code 0` 或「Unknown exception」。
- **临时空间按视频时长估算**，不再每次固定预留 4 GiB。
- **重新可编译。** `build_sppf.bat` 与 `build_core.bat` 使用 Visual Studio 2026（v145）
  与 SDK 10.0.26100；解决方案自带的 v100/v140 工具集已无法安装。
- **仅 64 位。** 32 位构建与 .NET 包装库不再产出：工程配置仍然存在，
  但已无任何构建会生成它们。
- **选项字母不区分大小写**，帮助移至 `-help`，以便把 `-h` 让给 HDR 开关。`-H203` 依然有效。
- **退出码有了明确含义。** 0 表示文件已写出，1 表示命令行有误，2 表示转换失败，
  3 表示抛出异常。此前成功转换返回 1，与命令行写错的值相同。
- **改由 GitHub Actions 编译**：从打了 tag 的源码编译两个二进制并运行回归检查，
  不再依赖本机上传的产物。

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

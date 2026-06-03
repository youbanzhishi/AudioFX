# VC-SpaceMaker — 动态频率避让插件

对标 [Wavesfactory TrackSpacer](https://www.wavesfactory.com/audio-plugins/trackspacer/)

## 核心原理

VC-SpaceMaker 通过分析侧链信号的频谱，对主信号施加**反向EQ曲线**——当侧链中某频率能量增大时，主信号中对应频率被自动衰减，从而在混音中为关键轨道"腾出空间"。

与传统多段压缩器不同，VC-SpaceMaker 使用 **32 个独立频段**，精度远超 3-5 段的多段压缩，且效果更透明、更自然。

## 功能特性

| 特性 | 说明 |
|------|------|
| **32频段动态EQ** | 对数分布 20Hz-20kHz，实时响应侧链频谱 |
| **Amount 旋钮** | 0-100% 控制避让强度 |
| **Attack / Release** | 毫秒级响应速度控制，如同压缩器 |
| **Low-Cut / High-Cut** | 限定频率范围，只在目标频段生效 |
| **Stereo / Mid-Side** | L/R 全立体声、仅 Mid、仅 Side、Mid+Side |
| **Freeze** | 冻结当前EQ曲线，静态应用 |
| **频谱显示** | 实时侧链频谱 + 衰减曲线可视化 |
| **内部侧链** | 无外部侧链时自动使用内部信号 |

## 典型用法

1. **Kick vs Bass**：在 Bass 轨道插入 VC-SpaceMaker，侧链设为 Kick → 低频自动避让
2. **Vocal vs 伴奏**：在伴奏总线插入，侧链设为 Vocal → 人声频段自动腾出
3. **Guitar vs Synth**：频率冲突的任意两轨之间

## 技术规格

- FFT 大小：2048（32频段 × 64 bins/频段）
- 重叠因子：4x（512 samples/hop）
- 最大衰减：18dB
- 延迟：约 11ms @ 44.1kHz
- 格式：VST3 / AU / Standalone CLI

## 构建状态

- Gen2 DSP 引擎（standalone + JUCE 双模式）
- CI 自动检测并构建
- macOS (Intel + Apple Silicon) / Windows / Linux

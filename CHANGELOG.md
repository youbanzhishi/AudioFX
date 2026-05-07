# Changelog

All notable changes to VocalChain (AudioFX) will be documented in this file.

## [0.2.0] - 2026-05-07

### Added
- **VC-Distortion** — 5种失真模式 (soft/hard/clip/fuzz/rectifier)
- **VC-Noise** — 6种信号生成 (white/pink/brown noise, sine/square/saw LFO)
- **VC-SurgicalDeEsser** — 两遍扫描精准去齿音 (detection pass + correction pass)

### Fixed
- VC-DeEsser致命bug: reduction当压缩比算→增益增强齿音 (commit f91c937)
- VC-Reverb wet信号滤波器: 添加wetLPF+wetHPF只处理湿信号 (commit f91c937)
- VC-Comp包络检测器bug: releaseTime未调用+信号低估6dB (commit f3a8548)

## [0.1.0] - 2026-05-07

### Added
- **VC-Plugin-Template** — 新插件脚手架模板
- **VC-EQ** — 5段参量均衡器 (LowShelf/HighShelf/Parametric/LP/HP)
- **VC-Comp** — 压缩器+侧链
- **VC-Smooth** — 平滑器
- **VC-Gain** — 增益控制
- **VC-DeEsser** — 去齿音 (5-9kHz频段压缩)
- **VC-Saturator** — 饱和器
- **VC-Limiter** — 限制器
- **VC-Delay** — 延迟 (BPM同步)
- **VC-Reverb** — 混响 (Schroeder comb+allpass)
- **VC-DynamicEQ** — 动态均衡器
- **三层CLI测试**: Tier1可运行 + Tier2功能验证 + Tier3信号链完整性
- **CI workflow**: 自动检测变更插件 → g++编译 → 三层测试 → release

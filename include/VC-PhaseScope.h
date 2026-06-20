/**
 * VC-PhaseScope - 相位抵消检测插件
 * 
 * 功能：
 * 1. 相位相关度检测 (-1 到 +1)
 * 2. 立体声相位矢量显示
 * 3. 底鼓/贝斯相位冲突检测
 * 
 * 作者：系统开发者
 * 日期：2026-05-23
 */

#pragma once

#include <cmath>
#include <vector>
#include <algorithm>

namespace audiofx {

/**
 * 相位相关度计算器
 * 使用互相关算法计算立体声信号的相位关系
 */
class PhaseCorrelation {
public:
    PhaseCorrelation() = default;
    
    /**
     * 计算单帧相位相关度
     * @param left 左声道样本
     * @param right 右声道样本
     * @return 相位相关度 [-1, +1]
     *   +1: 完全同相 (mono compatible)
     *    0: 不相关 (stereo)
     *   -1: 完全反相 (phase cancelled)
     */
    static float compute(const float* left, const float* right, size_t samples) {
        float sum_lr = 0.0f;
        float sum_l2 = 0.0f;
        float sum_r2 = 0.0f;
        
        for (size_t i = 0; i < samples; ++i) {
            float l = left[i];
            float r = right[i];
            sum_lr += l * r;
            sum_l2 += l * l;
            sum_r2 += r * r;
        }
        
        float denominator = std::sqrt(sum_l2 * sum_r2);
        if (denominator < 1e-10f) {
            return 0.0f;  // 静音或接近静音
        }
        
        return sum_lr / denominator;
    }
    
    /**
     * 计算短期相位相关度（用于实时显示）
     * 使用滑动窗口
     */
    static float computeShortTerm(const std::vector<float>& left, 
                                   const std::vector<float>& right,
                                   size_t windowSize = 1024) {
        if (left.size() < windowSize || right.size() < windowSize) {
            return 0.0f;
        }
        
        // 使用最近的一个窗口
        size_t offset = left.size() - windowSize;
        return compute(left.data() + offset, right.data() + offset, windowSize);
    }
};

/**
 * 底鼓/贝斯冲突检测器
 * 检测低频段（20-150Hz）的相位问题
 */
class KickBassDetector {
public:
    enum class ConflictLevel {
        None,       // 无冲突
        Low,        // 轻微
        Medium,     // 中等
        High        // 严重
    };
    
    KickBassDetector() 
        : kickFrequency_(60.0f)    // 底鼓典型频率
        , bassFrequency_(80.0f)    // 贝斯典型频率
        , thresholdLow_(0.3f)
        , thresholdMedium_(0.0f)
        , thresholdHigh_(-0.5f)
    {}
    
    void setKickFrequency(float freq) { kickFrequency_ = freq; }
    void setBassFrequency(float freq) { bassFrequency_ = freq; }
    
    /**
     * 检测低频相位冲突
     * @param left 左声道
     * @param right 右声道
     * @param sampleRate 采样率
     * @param samples 样本数
     * @return 冲突级别
     */
    ConflictLevel detect(const float* left, const float* right, 
                         float sampleRate, size_t samples) {
        // 提取低频成分（简单移动平均滤波）
        std::vector<float> lowL(samples), lowR(samples);
        extractLowFrequency(left, lowL.data(), sampleRate, samples);
        extractLowFrequency(right, lowR.data(), sampleRate, samples);
        
        // 计算低频段相位相关度
        float correlation = PhaseCorrelation::compute(lowL.data(), lowR.data(), samples);
        
        // 判断冲突级别
        if (correlation >= thresholdLow_) {
            return ConflictLevel::None;
        } else if (correlation >= thresholdMedium_) {
            return ConflictLevel::Low;
        } else if (correlation >= thresholdHigh_) {
            return ConflictLevel::Medium;
        } else {
            return ConflictLevel::High;
        }
    }
    
    /**
     * 获取冲突严重程度描述
     */
    static const char* getLevelName(ConflictLevel level) {
        switch (level) {
            case ConflictLevel::None:    return "OK";
            case ConflictLevel::Low:     return "Low";
            case ConflictLevel::Medium:  return "Medium";
            case ConflictLevel::High:    return "HIGH";
            default:                      return "Unknown";
        }
    }

private:
    /**
     * 提取低频成分（IIR低通滤波器简化版）
     */
    void extractLowFrequency(const float* input, float* output, 
                            float sampleRate, size_t samples) {
        float alpha = 2.0f * 150.0f / sampleRate;  // 150Hz 截止
        alpha = std::min(alpha, 1.0f);
        
        float prev = 0.0f;
        for (size_t i = 0; i < samples; ++i) {
            // 简单的一阶低通
            output[i] = prev + alpha * (input[i] - prev);
            prev = output[i];
        }
    }
    
    float kickFrequency_;
    float bassFrequency_;
    float thresholdLow_;
    float thresholdMedium_;
    float thresholdHigh_;
};

/**
 * 相位矢量（用于极坐标显示）
 * 计算左右声道的角度差
 */
class PhaseVector {
public:
    /**
     * 计算相位矢量角度
     * @param left 左声道 RMS
     * @param right 右声道 RMS
     * @return 角度 [-180, +180] 度
     */
    static float computeAngle(float leftRMS, float rightRMS) {
        if (leftRMS < 1e-10f && rightRMS < 1e-10f) {
            return 0.0f;
        }
        
        float mid = (leftRMS + rightRMS) * 0.5f;
        float side = (leftRMS - rightRMS) * 0.5f;
        
        // M/S 编码
        float m = mid;
        float s = side * 2.0f;
        
        // 计算角度
        float angle = std::atan2(s, m) * 180.0f / M_PI;
        return angle;
    }
    
    /**
     * 获取相位状态描述
     */
    static const char* getStateName(float correlation) {
        if (correlation > 0.5f)     return "Mono";
        if (correlation > 0.0f)     return "Wide";
        if (correlation > -0.5f)    return "Reverse";
        return "Out of Phase";
    }
};

} // namespace audiofx

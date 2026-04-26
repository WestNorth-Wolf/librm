/*
  Copyright (c) 2026 XDU-IRobot

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

/**
 * @file  librm/modules/encoder_counter.hpp
 * @brief 编码器圈数计数器
 */

#ifndef LIBRM_MODULES_ENCODER_COUNTER_HPP
#define LIBRM_MODULES_ENCODER_COUNTER_HPP

#include "librm/core/typedefs.hpp"

namespace rm::modules {

/**
 * @brief 编码器圈数计数器。
 *        通过检测编码器读数跨越周期边界来累计圈数，计算只有单圈编码器的电机驱动关节的实际累计位置。
 */
class EncoderCounter {
 public:
  /**
   * @brief 构造函数
   * @param encoder_cycle 编码器单圈的量程（周期）
   */
  explicit EncoderCounter(f32 encoder_cycle) : encoder_cycle_(encoder_cycle) {}

  /**
   * @brief  获取关节实际累计位置
   * @return 电机累计位置 = 圈数 × 编码器周期 + 当前编码器读数
   */
  f32 real_position() const { return rotation_count_ * encoder_cycle_ + last_encoder_; }

  /**
   * @brief        更新编码器读数，自动检测并累计跨圈
   * @param encoder 当前编码器读数
   */
  void Update(f32 encoder) {
    if (first_update_) {
      last_encoder_ = encoder;
      first_update_ = false;
      return;
    }

    const f32 delta = encoder - last_encoder_;

    if (delta > encoder_cycle_ / 2.0f) {
      // 编码器从大值跳到小值，向下跨越（逆时针转动穿过 0 点）
      rotation_count_--;
    } else if (delta < -encoder_cycle_ / 2.0f) {
      // 编码器从小值跳到大值，向上跨越（顺时针转动穿过 0 点）
      rotation_count_++;
    }

    last_encoder_ = encoder;
  }

  /**
   * @brief 重置计数器，清除圈数与编码器读数
   */
  void Reset() {
    last_encoder_ = 0.0f;
    rotation_count_ = 0;
    first_update_ = true;
  }

 private:
  f32 encoder_cycle_;        ///< 编码器单圈周期
  f32 last_encoder_{0.0f};   ///< 上一次编码器读数
  int rotation_count_{0};    ///< 累计圈数
  bool first_update_{true};  ///< 是否为第一次更新
};

}  // namespace rm::modules

#endif  // LIBRM_MODULES_ENCODER_COUNTER_HPP

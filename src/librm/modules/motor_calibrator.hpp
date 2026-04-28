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
 * @file  librm/modules/motor_calibrator.hpp
 * @brief 电机位置校准器
 */

#ifndef LIBRM_MODULES_MOTOR_CALIBRATOR_HPP
#define LIBRM_MODULES_MOTOR_CALIBRATOR_HPP

#include "librm/core/typedefs.hpp"

namespace rm::modules {

/**
 * @brief 电机位置校准器，用于处理电机反转和零点偏移
 *
 * 使用方法：
 * 1. 校准零点：把电机转到规定的零点位置，把此时的编码器数值作为 zero_offset
 * 2. 校准转向：如果电机安装方向和逻辑方向相反，设置 reverse 为 true
 * 3. 使用：读取电机编码器之后，通过 RealToLogical 转换成逻辑位置；计算控制输出的逻辑位置之后，通过 LogicalToReal
 * 转换成电机实际需要的输出位置
 *
 * 坐标变换公式：
 *    real_pos = (reverse ? -logical_pos : logical_pos) + zero_offset
 *    logical_pos = (real_pos - zero_offset) * (reverse ? -1 : 1)
 */
class MotorCalibrator {
 public:
  struct MotorConfig {
    bool reverse{false};   ///< 反转？
    f32 zero_offset{0.f};  ///< 电机在想要的输出零点时，编码器读数是多少
  };

  MotorCalibrator(bool reverse, f32 zero_offset) : motor_config_{.reverse = reverse, .zero_offset = zero_offset} {}
  explicit MotorCalibrator(const MotorConfig &config) : motor_config_(config) {}

  f32 LogicalToReal(f32 logical_pos) const {
    f32 transformed = logical_pos;
    // 反转
    if (motor_config_.reverse) {
      transformed = -transformed;
    }
    // 零点offset
    transformed += motor_config_.zero_offset;
    return transformed;
  }

  f32 RealToLogical(f32 real_pos) const {
    // 对每个电机应用反转和零点偏移补偿
    f32 pos = real_pos;
    // 先减去零点偏移
    pos -= motor_config_.zero_offset;
    // 再应用反转
    if (motor_config_.reverse) {
      pos = -pos;
    }
    return pos;
  }

  void UpdateConfig(const MotorConfig &new_config) { motor_config_ = new_config; }

  bool reverse() const { return motor_config_.reverse; }
  f32 zero_offset() const { return motor_config_.zero_offset; }

 private:
  MotorConfig motor_config_{};
};

}  // namespace rm::modules

#endif  // LIBRM_MODULES_MOTOR_CALIBRATOR_HPP
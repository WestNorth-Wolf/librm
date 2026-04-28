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
 * @file  librm/device/actuator/ht04_motor.hpp
 * @brief HT-04电机（MIT CAN协议）类库
 */

#ifndef LIBRM_DEVICE_ACTUATOR_HT04_MOTOR_HPP
#define LIBRM_DEVICE_ACTUATOR_HT04_MOTOR_HPP

#include <cstring>
#include <utility>

#include "librm/device/can_device.hpp"

namespace rm::device {

/**
 * @brief HT-04电机（MIT CAN协议）类库
 */
class Ht04Motor final : public CanDevice {
 public:
  /**
   * @brief 电机参数配置
   *
   * master_id 和 slave_id 需要与电机上位机软件中设置的值一致。
   * 其余参数使用默认值即可。
   */
  struct Settings {
    i16 master_id;     ///< 电机反馈报文CAN帧ID（CAN Master ID）
    i16 slave_id;      ///< 电机控制报文CAN帧ID（CAN ID）
    f32 p_max{95.5f};  ///< 最大位置（rad），与电机固件中P_MAX一致
    f32 v_max{45.0f};  ///< 最大速度（rad/s），与电机固件中V_MAX一致
    f32 t_max{18.0f};  ///< 控制帧/反馈帧电流字段的量程（A），与电机固件中T_MAX一致
    std::pair<f32, f32> kp_range{0.0f, 500.0f};  ///< Kp的取值范围（N·m/rad）
    std::pair<f32, f32> kd_range{0.0f, 5.0f};    ///< Kd的取值范围（N·m·s/rad）
    f32 torque_coef{3.5f};                       ///< 转矩系数 Kt（N·m/A），用于力矩↔电流换算
    f32 speed_bias{-0.0109901428f};              ///< 速度反馈零偏补偿（rad/s），从解码值中减去此值
  };

  /**
   * @brief 特殊功能指令，对应控制报文最后一个字节的值
   */
  enum class Instruction : u8 {
    kEnable = 0xfc,           ///< 使能电机
    kDisable = 0xfd,          ///< 失能电机
    kSetZeroPosition = 0xfe,  ///< 将当前位置设置为机械零点
  };

  Ht04Motor() = delete;
  Ht04Motor(Ht04Motor &&other) noexcept = default;
  ~Ht04Motor() override = default;

  /**
   * @param can      CAN外设对象
   * @param settings 电机参数配置
   * @param reversed 是否反转方向（取反位置、速度和力矩的符号）
   */
  Ht04Motor(hal::CanInterface &can, Settings settings, bool reversed = false);

  /**
   * @brief  MIT模式下发送控制指令
   *
   * @param position_rad      目标位置（rad）
   * @param speed_rad_per_sec 目标速度（rad/s）
   * @param torque_ff_nm      前馈力矩（N·m），内部转换为电流填入报文
   * @param kp                位置误差比例系数（N·m/rad）
   * @param kd                速度误差比例系数（N·m·s/rad）
   */
  void SetMitCommand(f32 position_rad, f32 speed_rad_per_sec, f32 torque_ff_nm, f32 kp, f32 kd);

  /**
   * @brief 向电机发送特殊功能指令
   * @param instruction 要发送的指令，见Instruction枚举
   */
  void SendInstruction(Instruction instruction);

  /** 取值函数 **/
  [[nodiscard]] f32 pos() const { return position_; }   ///< 当前位置（rad）
  [[nodiscard]] f32 vel() const { return velocity_; }   ///< 当前速度（rad/s），已零偏补偿
  [[nodiscard]] f32 torque() const { return torque_; }  ///< 当前估算力矩（N·m），由反馈电流×Kt换算
                                                        /**************/

 private:
  /**
   * @brief CAN回调函数，解码收到的反馈报文
   * @param msg 收到的CAN帧
   */
  void RxCallback(const hal::CanFrame *msg) override;

  Settings settings_{};
  bool reversed_{};
  u8 tx_buffer_[8]{0};

  /**   FEEDBACK DATA   **/
  f32 position_{};  ///< 当前位置（rad）
  f32 velocity_{};  ///< 当前速度（rad/s），已零偏补偿
  f32 torque_{};    ///< 当前估算力矩（N·m），由反馈电流×Kt换算
  /***********************/
};

}  // namespace rm::device

#endif  // LIBRM_DEVICE_ACTUATOR_HT04_MOTOR_HPP

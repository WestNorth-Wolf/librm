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
 * @file  librm/device/sensor/znsv6t1.hpp
 * @brief Znsv6T1 数字称重模块驱动（Modbus-RTU over RS485）
 */

#ifndef LIBRM_DEVICE_SENSOR_ZNSV6T1_HPP
#define LIBRM_DEVICE_SENSOR_ZNSV6T1_HPP

#include <etl/span.h>

#include "librm/core/typedefs.hpp"
#include "librm/device/device.hpp"
#include "librm/hal/serial.hpp"

namespace rm::device {

/**
 * @brief Znsv6T1 数字称重变送器
 * @note  通信参数默认：9600bps, 8N1, 站地址1
 *        支持 Modbus-RTU 协议功能码 03（读保持寄存器）和 06（写单寄存器）
 *        半双工 RS485，硬件收发器自动管理方向
 */
class Znsv6T1 : public Device {
 public:
  /**
   * @brief 采样速率枚举
   */
  enum class SamplingRate : u8 {
    k10Hz = 0,    ///< 10Hz
    k40Hz = 1,    ///< 40Hz
    k640Hz = 2,   ///< 640Hz
    k1280Hz = 3,  ///< 1280Hz
  };

  /**
   * @brief 波特率枚举（寄存器值 → 实际波特率）
   */
  enum class BaudRate : u8 {
    k2400 = 1,
    k4800 = 2,
    k9600 = 3,
    k19200 = 4,
    k28800 = 5,
    k38400 = 6,
    k57600 = 7,
    k115200 = 8,
    k256000 = 9,
    k500000 = 10,
  };

  /**
   * @brief 分度值枚举
   */
  enum class Division : u8 {
    k1 = 1,
    k2 = 2,
    k5 = 5,
    k10 = 10,
    k20 = 20,
    k50 = 50,
    k100 = 100,
  };

  /**
   * @brief 零位跟踪使能模式
   */
  enum class ZeroTrackingMode : u8 {
    kOff = 0,          ///< 关闭开机置零和动态追零
    kPowerOnOnly = 1,  ///< 只开启开机置零，关闭动态追零
    kDynamicOnly = 2,  ///< 关闭开机置零，只开启动态追零
    kBoth = 3,         ///< 开启开机置零和动态追零
  };

  /**
   * @brief 构造函数
   * @param serial      串口接口引用（需支持同步写和异步读）
   * @param modbus_addr Modbus 从机地址，范围 1~255，默认 1
   */
  Znsv6T1(hal::SerialInterface &serial, u8 modbus_addr = 1);

  /**
   * @brief 启动异步接收
   */
  void Begin() const;

  // ========== 读取数据 ==========

  /**
   * @brief 发送读取实时重量请求（寄存器 0x0000~0x0001，32 位有符号）
   * @note  响应到达后通过 weight() 获取结果
   */
  void RequestWeight();

  /**
   * @brief 发送读取 AD 内码值请求（寄存器 0x0002~0x0003，32 位无符号）
   */
  void RequestRawAd();

  /**
   * @brief 发送读取当前零位 AD 内码值请求（寄存器 0x0004~0x0005，32 位无符号）
   */
  void RequestZeroPosition();

  /**
   * @brief 获取最近一次读取的实时重量值（原始整数，不含小数点）
   * @return 32 位有符号重量值
   * @note  如实际重量 100.00，则返回 10000；小数点位由调用方自行处理
   */
  [[nodiscard]] i32 weight() const { return weight_; }

  /**
   * @brief 获取最近一次读取的 AD 内码值
   */
  [[nodiscard]] u32 raw_ad() const { return raw_ad_; }

  /**
   * @brief 获取最近一次读取的零位 AD 内码值
   */
  [[nodiscard]] u32 zero_position() const { return zero_position_; }

  // ========== 控制指令 ==========

  /**
   * @brief 零点校准（将 1 写入命令寄存器 0x0016）
   * @note  需先调用 OpenWriteProtect()
   */
  void SetZeroCalibration();

  /**
   * @brief 恢复出厂设置（将 9 写入命令寄存器 0x0016）
   * @note  需先调用 OpenWriteProtect()；会清除所有校准参数
   */
  void RestoreFactorySettings();

  /**
   * @brief 去皮（将 1 写入去皮寄存器 0x0015）
   */
  void Tare();

  /**
   * @brief 取消去皮（将 2 写入去皮寄存器 0x0015）
   */
  void CancelTare();

  // ========== 参数配置 ==========

  /**
   * @brief 设置砝码值用于单点标定（写入寄存器 0x0006）
   * @param weight 砝码重量值（不带小数点），范围 20~65535
   * @note  需先调用 OpenWriteProtect()
   */
  void SetCalibrationWeight(u16 weight);

  /**
   * @brief 设置追零强度（寄存器 0x0009）
   * @param strength 范围 1~20，默认 5；越小越慢越准
   * @note  需先调用 OpenWriteProtect()
   */
  void SetZeroTrackingStrength(u8 strength);

  /**
   * @brief 设置动态追零范围（寄存器 0x000A）
   * @param range 范围 1~50，默认 3
   * @note  需先调用 OpenWriteProtect()
   */
  void SetDynamicZeroRange(u8 range);

  /**
   * @brief 设置追零使能模式（寄存器 0x000B）
   * @param mode 追零模式
   * @note  需先调用 OpenWriteProtect()
   */
  void SetZeroTrackingMode(ZeroTrackingMode mode);

  /**
   * @brief 设置分度值（寄存器 0x000C）
   * @param div 分度值，限制输出重量最小跳动值
   * @note  需先调用 OpenWriteProtect()
   */
  void SetDivision(Division div);

  /**
   * @brief 设置中值滤波强度（寄存器 0x000D）
   * @param filter 可选 1/3/5/7/9，默认 3
   * @note  需先调用 OpenWriteProtect()
   */
  void SetMedianFilter(u8 filter);

  /**
   * @brief 设置采样速率（寄存器 0x000E）
   * @param rate 采样速率
   * @note  需先调用 OpenWriteProtect()；>40Hz 建议使用 115200 以上波特率
   */
  void SetSamplingRate(SamplingRate rate);

  /**
   * @brief 修改模块软件地址（寄存器 0x000F）
   * @param addr 新地址，范围 1~255
   * @note  需先调用 OpenWriteProtect()；254 为广播地址
   */
  void SetModuleAddress(u8 addr);

  /**
   * @brief 修改波特率（寄存器 0x0010）
   * @param rate 波特率
   * @note  需先调用 OpenWriteProtect()；修改后立即生效，需断电重连
   */
  void SetBaudRate(BaudRate rate);

  /**
   * @brief 设置平均滤波强度（寄存器 0x0011）
   * @param count 范围 1~50，默认 10
   * @note  需先调用 OpenWriteProtect()
   */
  void SetAverageFilter(u8 count);

  /**
   * @brief 设置动态跟踪范围（寄存器 0x0012）
   * @param range 范围 0~50，默认 1
   * @note  需先调用 OpenWriteProtect()
   */
  void SetDynamicTrackingRange(u8 range);

  /**
   * @brief 设置蠕变跟踪范围（寄存器 0x0013）
   * @param range 范围 0~10，默认 5
   * @note  需先调用 OpenWriteProtect()
   */
  void SetCreepTrackingRange(u8 range);

  /**
   * @brief 设置稳定重量输出开关（寄存器 0x0014）
   * @param on true=打开，false=关闭
   * @note  需先调用 OpenWriteProtect()
   */
  void SetStableWeightOutput(bool on);

  // ========== 写保护管理 ==========

  /**
   * @brief 关闭写保护（写 0 到寄存器 0x0017）
   * @note  所有需要写保护的寄存器操作前必须先调用此方法
   */
  void OpenWriteProtect();

  /**
   * @brief 开启写保护（写 1 到寄存器 0x0017）
   */
  void CloseWriteProtect();

  /**
   * @brief 获取模块地址
   */
  [[nodiscard]] u8 modbus_addr() const { return modbus_addr_; }

 private:
  /// Modbus 功能码
  static constexpr u8 kFuncRead = 0x03;   ///< 读保持寄存器
  static constexpr u8 kFuncWrite = 0x06;  ///< 写单寄存器

  /// 寄存器地址（16 进制）
  static constexpr u16 kRegWeight = 0x0000;            ///< 实时重量
  static constexpr u16 kRegRawAd = 0x0002;             ///< AD 内码值
  static constexpr u16 kRegZeroPosition = 0x0004;      ///< 当前零位
  static constexpr u16 kRegCalibWeight = 0x0006;       ///< 砝码值
  static constexpr u16 kRegZeroStrength = 0x0009;      ///< 追零强度
  static constexpr u16 kRegDynamicZeroRange = 0x000A;  ///< 动态追零范围
  static constexpr u16 kRegZeroEnable = 0x000B;        ///< 追零使能
  static constexpr u16 kRegDivision = 0x000C;          ///< 分度值
  static constexpr u16 kRegMedianFilter = 0x000D;      ///< 中值滤波
  static constexpr u16 kRegSamplingRate = 0x000E;      ///< 采样速率
  static constexpr u16 kRegModuleAddr = 0x000F;        ///< 模块地址
  static constexpr u16 kRegBaudRate = 0x0010;          ///< 波特率
  static constexpr u16 kRegAvgFilter = 0x0011;         ///< 平均滤波
  static constexpr u16 kRegDynamicTrack = 0x0012;      ///< 动态跟踪范围
  static constexpr u16 kRegCreepTrack = 0x0013;        ///< 蠕变跟踪范围
  static constexpr u16 kRegStableOutput = 0x0014;      ///< 稳定重量开关
  static constexpr u16 kRegTare = 0x0015;              ///< 去皮
  static constexpr u16 kRegCommand = 0x0016;           ///< 命令寄存器
  static constexpr u16 kRegWriteProtect = 0x0017;      ///< 写保护

  /// 命令寄存器值
  static constexpr u16 kCmdZeroCalib = 1;     ///< 零点校准
  static constexpr u16 kCmdFactoryReset = 9;  ///< 恢复出厂设置

  /// 去皮寄存器值
  static constexpr u16 kTareEnable = 1;  ///< 去皮
  static constexpr u16 kTareCancel = 2;  ///< 取消去皮

  /// 最大响应帧长度（03 响应：地址1+功能码1+字节数1+数据4+CRC2=9 字节）
  static constexpr usize kMaxRxFrameLen = 32;

  /**
   * @brief 期待的响应类型
   */
  enum class PendingRequest : u8 {
    kNone,
    kReadWeight,
    kReadRawAd,
    kReadZeroPosition,
    kWrite,
  };

  /**
   * @brief 串口接收回调
   */
  void RxCallback(etl::span<const u8> data);

  /**
   * @brief 发送 Modbus 03 读保持寄存器请求
   * @param reg_addr 起始寄存器地址
   * @param reg_count 寄存器数量
   */
  void SendRequest03(u16 reg_addr, u16 reg_count);

  /**
   * @brief 发送 Modbus 06 写单寄存器请求
   * @param reg_addr 寄存器地址
   * @param value    写入值
   */
  void SendRequest06(u16 reg_addr, u16 value);

  /**
   * @brief 计算 Modbus CRC16 并追加到缓冲区末尾（小端序）
   * @param buf  数据缓冲区
   * @param len  数据长度（不含 CRC）
   * @return     追加 CRC 后的总长度（len + 2）
   */
  usize AppendCrc16(u8 *buf, usize len);

  hal::SerialInterface *serial_;
  u8 modbus_addr_;

  PendingRequest pending_{PendingRequest::kNone};

  i32 weight_{0};
  u32 raw_ad_{0};
  u32 zero_position_{0};

  u8 tx_buffer_[16]{};  ///< 发送缓冲区
};

}  // namespace rm::device

#endif  // LIBRM_DEVICE_SENSOR_ZNSV6T1_HPP

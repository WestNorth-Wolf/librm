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
 * @file    librm/hal/serial_interface.hpp
 * @brief   串口 HAL 能力接口定义
 */

#ifndef LIBRM_HAL_SERIAL_INTERFACE_HPP
#define LIBRM_HAL_SERIAL_INTERFACE_HPP

#include <functional>

#include <etl/span.h>

#include "librm/core/typedefs.hpp"

namespace rm::hal {

/**
 * @brief 串口接收完成回调函数类型
 * @note  span 覆盖本次实际接收到的字节，span.size() == 本次接收到的字节数
 */
using SerialRxCallbackFunction = std::function<void(etl::span<const u8>)>;

/**
 * @name 能力接口
 * @brief 四个正交的串口读写能力接口
 * @{
 */

/**
 * @brief 同步写接口
 * @note  Write() 保证在所有字节完全发出（或超时）后才返回，对半双工 GPIO 切换方向安全
 */
class SyncWritable {
 public:
  /// 无超时限制（等待到完成为止）
  static constexpr u32 kNoTimeout = 0xFFFFFFFFU;

  virtual ~SyncWritable() = default;

  /**
   * @brief 阻塞发送
   * @param data       数据指针
   * @param size       数据长度
   * @param timeout_ms 超时时间（毫秒），默认 kNoTimeout（永不超时）
   */
  virtual void Write(const u8 *data, usize size, u32 timeout_ms) = 0;

  /// 无超时限制的便捷重载（不带 timeout_ms 的调用方无需修改）
  void Write(const u8 *data, usize size) { Write(data, size, kNoTimeout); }
};

/**
 * @brief 异步写接口
 * @note  WriteAsync() 立即返回；on_done 在发送完成时调用（可为 nullptr）
 */
class AsyncWritable {
 public:
  virtual ~AsyncWritable() = default;
  virtual void WriteAsync(const u8 *data, usize size, std::function<void()> on_done) = 0;
  /**
   * @brief 查询硬件当前是否正忙于上一次的异步写请求
   * @return true  上次 WriteAsync() 尚未完成
   * @return false 空闲，可以发起新的 WriteAsync()
   */
  virtual bool IsTxBusy() const = 0;
};

/**
 * @brief 同步读接口
 * @note  Read() 阻塞直到 buf 被填满或超时，返回实际读取的字节数
 */
class SyncReadable {
 public:
  /// 无超时限制
  static constexpr u32 kNoTimeout = 0xFFFFFFFFU;

  virtual ~SyncReadable() = default;

  /**
   * @brief 阻塞接收
   * @param buf        接收缓冲区
   * @param size       期望接收的字节数
   * @param timeout_ms 超时时间（毫秒），默认 kNoTimeout（永不超时）
   * @return           实际接收到的字节数
   */
  virtual usize Read(u8 *buf, usize size, u32 timeout_ms) = 0;

  /// 无超时限制的便捷重载
  usize Read(u8 *buf, usize size) { return Read(buf, size, kNoTimeout); }
};

/**
 * @brief 异步读接口
 * @note  Start() 后开始持续接收；每次收到数据触发所有已注册回调；Stop() 停止
 */
class AsyncReadable {
 public:
  virtual ~AsyncReadable() = default;
  virtual void AttachRxCallback(SerialRxCallbackFunction callback) = 0;
  virtual void Start() = 0;
  virtual void Stop() = 0;
};

/** @} */

/**
 * @name  组合接口
 * @brief Tx/Rx 四种能力接口的常用组合类型
 *
 * @note  组合接口均使用 virtual 继承能力接口，具体实现类（如 stm32::Uart）可以同时继承全部四种组合接口而不产生歧义。
 *        这样，一个实现类的实例可以隐式转换为任意组合接口的指针/引用，按需传递给只关心特定能力的调用方。
 * @{
 */

/**
 * @brief 同步写 + 同步读
 */
class SyncSerialInterface : public virtual SyncWritable, public virtual SyncReadable {};

/**
 * @brief 同步写 + 异步读（最常用的嵌入式串口组合）
 */
class SerialInterface : public virtual SyncWritable, public virtual AsyncReadable {};

/**
 * @brief 异步写 + 同步读
 */
class AsyncTxSyncRxSerialInterface : public virtual AsyncWritable, public virtual SyncReadable {};

/**
 * @brief 异步写 + 异步读
 */
class AsyncSerialInterface : public virtual AsyncWritable, public virtual AsyncReadable {};

/** @} */

}  // namespace rm::hal

#endif  // LIBRM_HAL_SERIAL_INTERFACE_HPP

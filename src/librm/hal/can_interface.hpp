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
 * @file    librm/hal/can_interface.hpp
 * @brief   CAN的接口类
 */

#ifndef LIBRM_HAL_CAN_INTERFACE_HPP
#define LIBRM_HAL_CAN_INTERFACE_HPP

#include "librm/core/typedefs.hpp"

#include <array>
#include <vector>
#include <unordered_map>

namespace rm::device {
class CanDevice;
}

namespace rm::hal {

struct CanFrame {
  std::array<u8, 64> data;  ///< 数据
  u16 rx_std_id;            ///< 标准ID
  u32 dlc;                  ///< 数据长度码
  bool is_fd_frame{false};  ///< 是否为FD帧
};

/**
 * @brief CAN接口类
 */
class CanInterface {
  friend class device::CanDevice;

 public:
  virtual ~CanInterface() = default;

  /**
   * @brief 立即向总线上发送数据
   * @param id      数据帧ID
   * @param data    数据指针
   * @param size    数据长度
   */
  virtual void Write(u16 id, const u8 *data, usize size) = 0;

  /**
   * @brief 设置过滤器
   * @param id
   * @param mask
   */
  virtual void SetFilter(u16 id, u16 mask) = 0;

  /**
   * @brief 启动CAN外设
   */
  virtual void Begin() = 0;

  /**
   * @brief 停止CAN外设
   */
  virtual void Stop() = 0;

  // 设备注册机制相关
 protected:
  /**
   * @brief 注册CAN设备
   * @param device    设备对象
   * @param rx_stdid  该设备要接收的标准帧ID
   * @note  同一设备在同一ID下只能注册一次（以UUID为准）
   */
  void RegisterDevice(device::CanDevice &device, u16 rx_stdid);

  /**
   * @brief 取消注册CAN设备
   * @param device 要取消注册的设备对象
   * @note  以UUID为准查找并移除，与设备对象的内存地址无关
   */
  void UnregisterDevice(device::CanDevice &device);

  /**
   * @brief 根据接收ID获取已注册的设备列表
   */
  const std::vector<device::CanDevice *> &GetDeviceListByRxStdid(u16 rx_stdid) const;

 private:
  std::vector<device::CanDevice *> empty_device_list_{};  ///< 用于在GetDeviceList中返回空列表
  std::unordered_map<u16, std::vector<device::CanDevice *>>
      rx_id_to_device_list_map_{};  ///< 订阅ID到设备对象列表的映射
};

}  // namespace rm::hal

#endif  // LIBRM_HAL_CAN_INTERFACE_HPP

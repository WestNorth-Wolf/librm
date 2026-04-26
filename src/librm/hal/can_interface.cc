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
 * @file    librm/hal/can_interface.cc
 * @brief   CAN接口类的非内联实现（设备注册/注销）
 */

#include "can_interface.hpp"

#include "librm/device/can_device.hpp"

namespace rm::hal {

void CanInterface::RegisterDevice(device::CanDevice &device, u16 rx_stdid) {
  // 不允许同一个设备（以UUID为准）在同一个ID下注册多次
  const auto target_device_array = rx_id_to_device_list_map_.find(rx_stdid);
  if (target_device_array != rx_id_to_device_list_map_.end()) {
    for (const auto *registered_device : target_device_array->second) {
      if (registered_device->uuid() == device.uuid()) {
        return;
      }
    }
  }

  if (target_device_array == rx_id_to_device_list_map_.end()) {
    rx_id_to_device_list_map_[rx_stdid] = std::vector<device::CanDevice *>{};
  }
  rx_id_to_device_list_map_[rx_stdid].push_back(&device);
}

void CanInterface::UnregisterDevice(device::CanDevice &device) {
  const u64 target_uuid = device.uuid();
  for (auto &[rx_stdid, device_array] : rx_id_to_device_list_map_) {
    for (auto it = device_array.begin(); it != device_array.end(); ++it) {
      if ((*it)->uuid() == target_uuid) {
        device_array.erase(it);
        break;  // 一个设备只能在一个ID下注册一次，找到即可退出这一层循环
      }
    }
  }
}

const std::vector<device::CanDevice *> &CanInterface::GetDeviceListByRxStdid(u16 rx_stdid) const {
  const auto target_device_array = rx_id_to_device_list_map_.find(rx_stdid);
  if (target_device_array != rx_id_to_device_list_map_.end()) {
    return target_device_array->second;
  }
  return empty_device_list_;
}

}  // namespace rm::hal

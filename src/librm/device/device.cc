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
 * @file  librm/device/device.cc
 * @brief 驱动框架的设备基类，主要功能为监视设备在线状态
 */

#include "device.hpp"

#include <etl/to_string.h>

namespace rm::device {

/**
 * @brief 分配全局唯一UUID，单调递增，从1开始（0保留为无效值，用于标记移动后的源对象）
 */
static u64 AllocateUuid() {
  static u64 next_uuid{1u};
  return next_uuid++;
}

Device::Device() {
  uuid_ = AllocateUuid();

  // 把这个设备的地址作为它的默认名称
  etl::format_spec format;
  format.hex().width(16).fill('0');
  etl::to_string(reinterpret_cast<uintptr_t>(this), name_, format);
}

Device::Device(const Device &other)
    : name_(other.name_),
      online_status_{kUnknown},
      last_seen_{time_point::min()},
      heartbeat_timeout_{other.heartbeat_timeout_},
      uuid_{AllocateUuid()} {}

Device &Device::operator=(const Device &other) {
  if (this != &other) {
    name_ = other.name_;
    heartbeat_timeout_ = other.heartbeat_timeout_;
    // 重置运行时状态，配置已更新但设备尚未被观测到
    online_status_ = kUnknown;
    last_seen_ = time_point::min();
    // uuid_ 不变：对象身份在构造时已确立，赋值不改变身份
  }
  return *this;
}

Device::Device(Device &&other) noexcept
    : name_(std::move(other.name_)),
      online_status_{other.online_status_},
      last_seen_{other.last_seen_},
      heartbeat_timeout_{other.heartbeat_timeout_},
      uuid_{other.uuid_} {
  // 源对象的UUID置0，标记为已移动（无效状态）
  other.uuid_ = 0;
  other.online_status_ = kUnknown;
  other.last_seen_ = time_point::min();
}

Device &Device::operator=(Device &&other) noexcept {
  if (this != &other) {
    name_ = std::move(other.name_);
    online_status_ = other.online_status_;
    last_seen_ = other.last_seen_;
    heartbeat_timeout_ = other.heartbeat_timeout_;
    uuid_ = other.uuid_;
    other.uuid_ = 0;
    other.online_status_ = kUnknown;
    other.last_seen_ = time_point::min();
  }
  return *this;
}

void Device::SetHeartbeatTimeout(duration timeout) { heartbeat_timeout_ = timeout; }

[[nodiscard]] Device::Status Device::online_status() {
  const auto now = std::chrono::steady_clock::now();
  if (now - last_seen_ > heartbeat_timeout_) {
    // 如果距离上次设备上报状态时间超过心跳超时时间，则认为设备离线
    online_status_ = kOffline;
  }
  return online_status_;
}

[[nodiscard]] Device::time_point Device::last_seen() const { return last_seen_; }

void Device::ReportStatus(Status status) {
  last_seen_ = std::chrono::steady_clock::now();
  online_status_ = status;
}

void Device::SetName(etl::string<kMaxNameLength> name) { name_ = std::move(name); }

[[nodiscard]] etl::string<Device::kMaxNameLength> Device::name() const { return name_; }

[[nodiscard]] u64 Device::uuid() const { return uuid_; }

bool Device::operator==(const Device &other) const { return uuid_ == other.uuid_; }

bool Device::operator!=(const Device &other) const { return uuid_ != other.uuid_; }

}  // namespace rm::device

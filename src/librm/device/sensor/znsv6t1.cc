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
 * @file  librm/device/sensor/znsv6t1.cc
 * @brief Znsv6T1 数字称重模块驱动实现
 */

#include "znsv6t1.hpp"

#include <etl/crc16_modbus.h>

namespace rm::device {

Znsv6T1::Znsv6T1(hal::SerialInterface &serial, u8 modbus_addr) : serial_(&serial), modbus_addr_(modbus_addr) {
  serial_->AttachRxCallback([this](etl::span<const u8> data) { RxCallback(data); });
}

void Znsv6T1::Begin() const { serial_->Start(); }

usize Znsv6T1::AppendCrc16(u8 *buf, usize len) {
  etl::crc16_modbus crc;
  crc.add(buf, buf + len);
  const u16 crc_val = crc.value();
  // Modbus CRC 以小端序追加
  buf[len] = static_cast<u8>(crc_val & 0xFF);
  buf[len + 1] = static_cast<u8>((crc_val >> 8) & 0xFF);
  return len + 2;
}

void Znsv6T1::SendRequest03(u16 reg_addr, u16 reg_count) {
  u8 idx = 0;
  tx_buffer_[idx++] = modbus_addr_;
  tx_buffer_[idx++] = kFuncRead;
  tx_buffer_[idx++] = static_cast<u8>((reg_addr >> 8) & 0xFF);
  tx_buffer_[idx++] = static_cast<u8>(reg_addr & 0xFF);
  tx_buffer_[idx++] = static_cast<u8>((reg_count >> 8) & 0xFF);
  tx_buffer_[idx++] = static_cast<u8>(reg_count & 0xFF);

  idx = AppendCrc16(tx_buffer_, idx);
  serial_->Write(tx_buffer_, idx);
}

void Znsv6T1::SendRequest06(u16 reg_addr, u16 value) {
  u8 idx = 0;
  tx_buffer_[idx++] = modbus_addr_;
  tx_buffer_[idx++] = kFuncWrite;
  tx_buffer_[idx++] = static_cast<u8>((reg_addr >> 8) & 0xFF);
  tx_buffer_[idx++] = static_cast<u8>(reg_addr & 0xFF);
  tx_buffer_[idx++] = static_cast<u8>((value >> 8) & 0xFF);
  tx_buffer_[idx++] = static_cast<u8>(value & 0xFF);

  idx = AppendCrc16(tx_buffer_, idx);
  serial_->Write(tx_buffer_, idx);
}

void Znsv6T1::RxCallback(etl::span<const u8> data) {
  const usize rx_len = data.size();

  // 最小合法帧：地址(1) + 功能码(1) + CRC(2) = 4 字节
  if (rx_len < 4) {
    return;
  }

  // 校验地址匹配（广播地址 0 的响应目前不处理）
  if (data[0] != modbus_addr_) {
    return;
  }

  // 校验 CRC16（CRC 覆盖地址到 CRC 之前的全部字节）
  etl::crc16_modbus crc;
  crc.add(data.data(), data.data() + rx_len - 2);
  const u16 calc_crc = crc.value();
  const u16 recv_crc = static_cast<u16>(data[rx_len - 2]) | (static_cast<u16>(data[rx_len - 1]) << 8);

  if (calc_crc != recv_crc) {
    return;
  }

  const u8 func_code = data[1];

  // 上报在线
  ReportStatus(kOk);

  if (func_code == kFuncRead) {
    // 03 响应：地址(1) + 功能码(1) + 字节数(1) + 数据(N) + CRC(2)
    const u8 byte_count = data[2];

    switch (pending_) {
      case PendingRequest::kReadWeight: {
        // 重量：32 位有符号，高字在前低字在后（大端序）
        // 响应数据：data[3]高字高字节, data[4]高字低字节, data[5]低字高字节, data[6]低字低字节
        if (byte_count >= 4) {
          const u32 raw = (static_cast<u32>(data[3]) << 24) | (static_cast<u32>(data[4]) << 16) |
                          (static_cast<u32>(data[5]) << 8) | static_cast<u32>(data[6]);
          weight_ = static_cast<i32>(raw);
        }
        break;
      }
      case PendingRequest::kReadRawAd: {
        // AD 内码值：32 位无符号，同上大端序
        if (byte_count >= 4) {
          raw_ad_ = (static_cast<u32>(data[3]) << 24) | (static_cast<u32>(data[4]) << 16) |
                    (static_cast<u32>(data[5]) << 8) | static_cast<u32>(data[6]);
        }
        break;
      }
      case PendingRequest::kReadZeroPosition: {
        if (byte_count >= 4) {
          zero_position_ = (static_cast<u32>(data[3]) << 24) | (static_cast<u32>(data[4]) << 16) |
                           (static_cast<u32>(data[5]) << 8) | static_cast<u32>(data[6]);
        }
        break;
      }
      default:
        break;
    }
  } else if (func_code == kFuncWrite) {
    // 06 响应即为请求回显，已通过 CRC 校验，无需进一步处理
  }

  pending_ = PendingRequest::kNone;
}

// ========== 读取数据 ==========

void Znsv6T1::RequestWeight() {
  pending_ = PendingRequest::kReadWeight;
  SendRequest03(kRegWeight, 2);
}

void Znsv6T1::RequestRawAd() {
  pending_ = PendingRequest::kReadRawAd;
  SendRequest03(kRegRawAd, 2);
}

void Znsv6T1::RequestZeroPosition() {
  pending_ = PendingRequest::kReadZeroPosition;
  SendRequest03(kRegZeroPosition, 2);
}

// ========== 控制指令 ==========

void Znsv6T1::SetZeroCalibration() {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegCommand, kCmdZeroCalib);
}

void Znsv6T1::RestoreFactorySettings() {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegCommand, kCmdFactoryReset);
}

void Znsv6T1::Tare() {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegTare, kTareEnable);
}

void Znsv6T1::CancelTare() {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegTare, kTareCancel);
}

// ========== 参数配置 ==========

void Znsv6T1::SetCalibrationWeight(u16 weight) {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegCalibWeight, weight);
}

void Znsv6T1::SetZeroTrackingStrength(u8 strength) {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegZeroStrength, strength);
}

void Znsv6T1::SetDynamicZeroRange(u8 range) {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegDynamicZeroRange, range);
}

void Znsv6T1::SetZeroTrackingMode(ZeroTrackingMode mode) {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegZeroEnable, static_cast<u16>(mode));
}

void Znsv6T1::SetDivision(Division div) {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegDivision, static_cast<u16>(div));
}

void Znsv6T1::SetMedianFilter(u8 filter) {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegMedianFilter, filter);
}

void Znsv6T1::SetSamplingRate(SamplingRate rate) {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegSamplingRate, static_cast<u16>(rate));
}

void Znsv6T1::SetModuleAddress(u8 addr) {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegModuleAddr, addr);
}

void Znsv6T1::SetBaudRate(BaudRate rate) {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegBaudRate, static_cast<u16>(rate));
}

void Znsv6T1::SetAverageFilter(u8 count) {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegAvgFilter, count);
}

void Znsv6T1::SetDynamicTrackingRange(u8 range) {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegDynamicTrack, range);
}

void Znsv6T1::SetCreepTrackingRange(u8 range) {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegCreepTrack, range);
}

void Znsv6T1::SetStableWeightOutput(bool on) {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegStableOutput, on ? 1 : 0);
}

// ========== 写保护管理 ==========

void Znsv6T1::OpenWriteProtect() {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegWriteProtect, 0);
}

void Znsv6T1::CloseWriteProtect() {
  pending_ = PendingRequest::kWrite;
  SendRequest06(kRegWriteProtect, 1);
}

}  // namespace rm::device

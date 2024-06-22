#
# Copyright (c) 2024 XDU-IRobot
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

set(IROBOTEC_AVAIABLE_PLATFORMS "STM32" "Linux" "OrangePi" "Jetson")

# 尝试通过查找和平台有关的库来检测平台
find_package(WiringPi QUIET)      # raspberry pi/orange pi
find_package(JetsonGPIO QUIET)    # jetson

get_directory_property(DEFS COMPILE_DEFINITIONS)

if (DEFINED IROBOTEC_PLATFORM)                                      # 如果用户定义了IROBOTEC_PLATFORM变量
    if (NOT IROBOTEC_PLATFORM STREQUAL "STM32" OR
            NOT IROBOTEC_PLATFORM STREQUAL "Linux")
        message(WARNING "[irobotEC]: Invalid platform: ${IROBOTEC_PLATFORM}")   # 但是用户定义的平台不在可用平台列表里
    endif ()
else ()                                                                         # 就尝试自动检测平台
    if (${CMAKE_CROSSCOMPILING} AND                                 # 启用了交叉编译
            ${CMAKE_C_COMPILER} MATCHES "arm-none-eabi-gcc" AND     # 使用了arm-none-eabi-gcc编译器
            "${DEFS}" MATCHES "STM32")                              # 宏定义中包含STM32字样
        set(IROBOTEC_PLATFORM "STM32")                              # 那么就认为是STM32平台
    elseif (${CMAKE_HOST_LINUX})                                    # 如果是在Linux上编译
        if (WiringPi_FOUND)                                         # 根据有没有找到和平台有关的库来判断平台
            set(IROBOTEC_PLATFORM "OrangePi")
        elseif (JetsonGPIO_FOUND)
            set(IROBOTEC_PLATFORM "Jetson")
        else ()                                                     # 如果没有找到和平台有关的库
            set(IROBOTEC_PLATFORM "Linux")                          # 那么就fallback到Linux平台
        endif ()
    endif ()
endif ()

# 如果进行到这里IROBOTEC_PLATFORM还没有被定义，那么说明我们没有检测到平台，报错中断构建过程，并且提示用户手动设置IROBOTEC_PLATFORM变量
# 需要注意的是，如果已经到了上面的逻辑检测不到平台的地步，那就说明现在的环境或者工程多半有问题，即使手动设置了平台，也可能会有其他问题
# if IROBOTEC_PLATFORM is still not set, that means we failed to detect the platform
# in this case interrupt the build process and tell the user to set the IROBOTEC_PLATFORM variable manually
# NOTE THAT if we have reached this point where the detection above failed,
# it is likely that there are other issues with the environment or the project
if (NOT DEFINED IROBOTEC_PLATFORM)
    message(FATAL_ERROR "[irobotEC]: Failed to detect the platform, please specify manually by setting IROBOTEC_PLATFORM variable.\r\n"
            "[irobotEC]: Available platforms: ${IROBOTEC_AVAIABLE_PLATFORMS}")
endif ()

message(STATUS "[irobotEC]: Detected platform: ${IROBOTEC_PLATFORM}")

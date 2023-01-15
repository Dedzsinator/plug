/*
 * PLUG - software to operate Fender Mustang amplifier
 *        Linux replacement for Fender FUSE software
 *
 * Copyright (C) 2017-2023  offa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "com/ConnectionFactory.h"
#include "com/CommunicationException.h"
#include "com/UsbComm.h"
#include "com/UsbContext.h"
#include <algorithm>

namespace plug::com
{
    namespace
    {
        inline constexpr std::uint16_t usbVID{0x1ed8};

        namespace usbPID
        {
            inline constexpr std::uint16_t mustangI_II{0x0004};
            inline constexpr std::uint16_t mustangIII_IV_V{0x0005};
            inline constexpr std::uint16_t mustangBronco{0x000a};
            inline constexpr std::uint16_t mustangMini{0x0010};
            inline constexpr std::uint16_t mustangFloor{0x0012};
            inline constexpr std::uint16_t mustangI_II_v2{0x0014};
            inline constexpr std::uint16_t mustangIII_IV_V_v2{0x0016};
        }

        inline constexpr std::initializer_list<std::uint16_t> pids{
            usbPID::mustangI_II,
            usbPID::mustangIII_IV_V,
            usbPID::mustangBronco,
            usbPID::mustangMini,
            usbPID::mustangFloor,
            usbPID::mustangI_II_v2,
            usbPID::mustangIII_IV_V_v2};
    }

    std::shared_ptr<Connection> createUsbConnection()
    {
        auto devices = usb::listDevices();

        auto itr = std::find_if(devices.begin(), devices.end(), [](const auto& dev)
                                { return (dev.vendorId() == usbVID) && std::any_of(pids.begin(), pids.end(), [&dev](std::uint16_t pid)
                                                                                   { return dev.productId() == pid; }); });

        if (itr == devices.end())
        {
            throw CommunicationException{"No device found"};
        }

        return std::make_shared<UsbComm>(std::move(*itr));
    }
}

/*
 * PLUG - software to operate Fender Mustang amplifier
 *        Linux replacement for Fender FUSE software
 *
 * Copyright (C) 2017-2018  offa
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

#pragma once

#include <gmock/gmock.h>
#include <libusb-1.0/libusb.h>

namespace mock
{
    class UsbMock
    {
    public:
        MOCK_METHOD1(init, int(libusb_context**));
        MOCK_METHOD1(close, void(libusb_device_handle*));
        MOCK_METHOD3(open_device_with_vid_pid, libusb_device_handle*(libusb_context*, uint16_t, uint16_t));
        MOCK_METHOD1(exit, void(libusb_context*));
        MOCK_METHOD2(release_interface, int(libusb_device_handle*, int));
        MOCK_METHOD2(kernel_driver_active, int(libusb_device_handle*, int));
        MOCK_METHOD2(attach_kernel_driver, int(libusb_device_handle*, int));
        MOCK_METHOD6(interrupt_transfer, int(libusb_device_handle*, unsigned char, unsigned char*, int, int*, unsigned int));
        MOCK_METHOD2(claim_interface, int(libusb_device_handle*, int));
    };

    UsbMock* getUsbMock();
    UsbMock* resetUsbMock();
    void clearUsbMock();
}


extern "C" {

struct libusb_device_handle
{
    char dummy;
};
}

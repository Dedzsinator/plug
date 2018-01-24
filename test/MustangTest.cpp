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

#include "mustang.h"
#include "LibUsbMocks.h"
#include "common.h"
#include <array>
#include <gmock/gmock.h>

using namespace plug;
using namespace testing;
using mock::UsbMock;


namespace
{
    MATCHER_P(BufferIs, expected, "")
    {
        return std::equal(expected.cbegin(), expected.cend(), arg);
    }

    constexpr std::size_t posDsp{2};
    constexpr std::size_t posEffect{16};
    constexpr std::size_t posFxSlot{18};
    constexpr std::size_t posKnob1{32};
    constexpr std::size_t posKnob2{33};
    constexpr std::size_t posKnob3{34};
    constexpr std::size_t posKnob4{35};
    constexpr std::size_t posKnob5{36};
    constexpr std::size_t posKnob6{37};
}


class MustangTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m = std::make_unique<Mustang>();
        usbmock = mock::resetUsbMock();
    }

    void TearDown() override
    {
        mock::clearUsbMock();
    }

    void expectStart()
    {
        EXPECT_CALL(*usbmock, open_device_with_vid_pid(_, _, _)).WillOnce(Return(&handle));
        EXPECT_CALL(*usbmock, interrupt_transfer(_, _, _, _, _, _)).Times(AnyNumber());
        m->start_amp(nullptr, nullptr, nullptr, nullptr);
    }

    constexpr auto createEffectData(std::uint8_t slot, std::uint8_t effect, std::array<std::uint8_t, 6> values) const
    {
        std::array<std::uint8_t, packetSize> data{{0}};
        data[posDsp] = 8;
        data[posEffect] = effect;
        data[posFxSlot] = slot;
        data[posKnob1] = values[0];
        data[posKnob2] = values[1];
        data[posKnob3] = values[2];
        data[posKnob4] = values[3];
        data[posKnob5] = values[4];
        data[posKnob6] = values[5];
        return data;
    }


    std::unique_ptr<Mustang> m;
    mock::UsbMock* usbmock;
    libusb_device_handle handle;
    static constexpr std::size_t packetSize{64};
};

TEST_F(MustangTest, stopAmpDoesNothingIfNotStartedYet)
{
    m->stop_amp();
}

TEST_F(MustangTest, stopAmpClosesConnection)
{
    expectStart();
    EXPECT_CALL(*usbmock, release_interface(&handle, 0)).WillOnce(Return(LIBUSB_SUCCESS));
    EXPECT_CALL(*usbmock, attach_kernel_driver(&handle, 0));
    EXPECT_CALL(*usbmock, close(_));
    EXPECT_CALL(*usbmock, exit(nullptr));
    m->stop_amp();
}

TEST_F(MustangTest, stopAmpClosesConnectionIfNoDevice)
{
    expectStart();
    EXPECT_CALL(*usbmock, release_interface(&handle, 0)).WillOnce(Return(LIBUSB_ERROR_NO_DEVICE));
    EXPECT_CALL(*usbmock, close(_));
    EXPECT_CALL(*usbmock, exit(nullptr));
    m->stop_amp();
}

TEST_F(MustangTest, stopAmpTwiceDoesNothing)
{
    expectStart();
    EXPECT_CALL(*usbmock, release_interface(&handle, 0)).WillOnce(Return(LIBUSB_SUCCESS));
    EXPECT_CALL(*usbmock, attach_kernel_driver(&handle, 0));
    EXPECT_CALL(*usbmock, close(_));
    EXPECT_CALL(*usbmock, exit(nullptr));
    m->stop_amp();
    m->stop_amp();
}
TEST_F(MustangTest, loadMemoryBankSendsBankSelectionCommandAndReceivesPacket)
{
    constexpr int recvSize{1};
    constexpr int slot{8};
    constexpr std::size_t slotPos{4};
    std::array<std::uint8_t, packetSize> sendCmd;
    sendCmd.fill(0x00);
    sendCmd[0] = 0x1c;
    sendCmd[1] = 0x01;
    sendCmd[2] = 0x01;
    sendCmd[slotPos] = slot;
    sendCmd[6] = 0x01;
    std::array<std::uint8_t, packetSize> dummy{{0}};

    EXPECT_CALL(*usbmock, interrupt_transfer(_, 0x01, BufferIs(sendCmd), packetSize, _, _)).WillOnce(DoAll(SetArgPointee<4>(recvSize), Return(0)));
    EXPECT_CALL(*usbmock, interrupt_transfer(_, 0x81, _, packetSize, _, _)).WillOnce(DoAll(SetArrayArgument<2>(dummy.cbegin(), dummy.cend()), SetArgPointee<4>(recvSize - 1), Return(0)));

    const auto result = m->load_memory_bank(slot, nullptr, nullptr, nullptr);
    EXPECT_THAT(result, Eq(0));
}

TEST_F(MustangTest, loadMemoryBankReceivesName)
{
    constexpr int recvSize{1};
    constexpr int slot{8};
    std::array<std::uint8_t, packetSize> recvData;
    recvData.fill(0x00);
    recvData[16] = 'a';
    recvData[17] = 'b';
    recvData[18] = 'c';

    EXPECT_CALL(*usbmock, interrupt_transfer(_, 0x01, _, packetSize, _, _)).WillOnce(DoAll(SetArgPointee<4>(recvSize), Return(0)));
    EXPECT_CALL(*usbmock, interrupt_transfer(_, 0x81, _, packetSize, _, _)).WillOnce(DoAll(SetArrayArgument<2>(recvData.cbegin(), recvData.cend()), SetArgPointee<4>(0), Return(0)));

    std::array<char, packetSize> name{{0}};
    m->load_memory_bank(slot, name.data(), nullptr, nullptr);
    EXPECT_THAT(name.data(), StrEq("abc"));
}

TEST_F(MustangTest, loadMemoryBankReceivesAmpValues)
{
    constexpr int recvSize{2};
    constexpr int slot{8};
    constexpr std::size_t ampPos{16};
    constexpr std::size_t volumePos{32};
    constexpr std::size_t gainPos{33};
    constexpr std::size_t treblePos{36};
    constexpr std::size_t middlePos{37};
    constexpr std::size_t bassPos{38};
    constexpr std::size_t cabinetPos{49};
    constexpr std::size_t noiseGatePos{47};
    constexpr std::size_t thresholdPos{48};
    constexpr std::size_t masterVolPos{35};
    constexpr std::size_t gain2Pos{34};
    constexpr std::size_t presencePos{39};
    constexpr std::size_t depthPos{41};
    constexpr std::size_t biasPos{42};
    constexpr std::size_t sagPos{51};
    constexpr std::size_t brightnessPos{52};
    std::array<std::uint8_t, packetSize> dummy{{0}};
    std::array<std::uint8_t, packetSize> recvData;
    recvData.fill(0x00);
    recvData[ampPos] = 0x5e;
    recvData[volumePos] = 1;
    recvData[gainPos] = 2;
    recvData[treblePos] = 3;
    recvData[middlePos] = 4;
    recvData[bassPos] = 5;
    recvData[cabinetPos] = 6;
    recvData[noiseGatePos] = 7;
    recvData[thresholdPos] = 8;
    recvData[masterVolPos] = 9;
    recvData[gain2Pos] = 10;
    recvData[presencePos] = 11;
    recvData[depthPos] = 12;
    recvData[biasPos] = 13;
    recvData[sagPos] = 14;
    recvData[brightnessPos] = 0;

    EXPECT_CALL(*usbmock, interrupt_transfer(_, 0x01, _, packetSize, _, _)).WillOnce(DoAll(SetArgPointee<4>(recvSize), Return(0)));
    EXPECT_CALL(*usbmock, interrupt_transfer(_, 0x81, _, packetSize, _, _)).WillOnce(DoAll(SetArrayArgument<2>(dummy.cbegin(), dummy.cend()), SetArgPointee<4>(recvSize - 1), Return(0))).WillOnce(DoAll(SetArrayArgument<2>(recvData.cbegin(), recvData.cend()), SetArgPointee<4>(recvSize - 2), Return(0)));

    amp_settings settings{};
    m->load_memory_bank(slot, nullptr, &settings, nullptr);
    EXPECT_THAT(settings.amp_num, Eq(value(amps::BRITISH_80S)));
    EXPECT_THAT(settings.volume, Eq(recvData[volumePos]));
    EXPECT_THAT(settings.gain, Eq(recvData[gainPos]));
    EXPECT_THAT(settings.treble, Eq(recvData[treblePos]));
    EXPECT_THAT(settings.middle, Eq(recvData[middlePos]));
    EXPECT_THAT(settings.bass, Eq(recvData[bassPos]));
    EXPECT_THAT(settings.cabinet, Eq(recvData[cabinetPos]));
    EXPECT_THAT(settings.noise_gate, Eq(recvData[noiseGatePos]));
    EXPECT_THAT(settings.threshold, Eq(recvData[thresholdPos]));
    EXPECT_THAT(settings.master_vol, Eq(recvData[masterVolPos]));
    EXPECT_THAT(settings.gain2, Eq(recvData[gain2Pos]));
    EXPECT_THAT(settings.presence, Eq(recvData[presencePos]));
    EXPECT_THAT(settings.depth, Eq(recvData[depthPos]));
    EXPECT_THAT(settings.bias, Eq(recvData[biasPos]));
    EXPECT_THAT(settings.sag, Eq(recvData[sagPos]));
    EXPECT_THAT(settings.brightness, Eq(recvData[brightnessPos]));
}

TEST_F(MustangTest, loadMemoryBankReceivesEffectValues)
{
    constexpr int recvSize{6};
    constexpr int slot{8};
    std::array<std::uint8_t, packetSize> dummy{{0}};
    auto recvData0 = createEffectData(0x04, 0x4f, {{11, 22, 33, 44, 55, 66}});
    auto recvData1 = createEffectData(0x01, 0x13, {{0, 0, 0, 1, 1, 1}});
    auto recvData2 = createEffectData(0x02, 0x00, {{0, 0, 0, 0, 0, 0}});
    auto recvData3 = createEffectData(0x07, 0x2b, {{1, 2, 3, 4, 5, 6}});

    EXPECT_CALL(*usbmock, interrupt_transfer(_, 0x01, _, packetSize, _, _)).WillOnce(DoAll(SetArgPointee<4>(recvSize), Return(0)));
    EXPECT_CALL(*usbmock, interrupt_transfer(_, 0x81, _, packetSize, _, _))
        .WillOnce(DoAll(SetArrayArgument<2>(dummy.cbegin(), dummy.cend()), SetArgPointee<4>(recvSize - 1), Return(0)))
        .WillOnce(DoAll(SetArrayArgument<2>(dummy.cbegin(), dummy.cend()), SetArgPointee<4>(recvSize - 2), Return(0)))
        .WillOnce(DoAll(SetArrayArgument<2>(recvData0.cbegin(), recvData0.cend()), SetArgPointee<4>(recvSize - 3), Return(0)))
        .WillOnce(DoAll(SetArrayArgument<2>(recvData1.cbegin(), recvData1.cend()), SetArgPointee<4>(recvSize - 4), Return(0)))
        .WillOnce(DoAll(SetArrayArgument<2>(recvData2.cbegin(), recvData2.cend()), SetArgPointee<4>(recvSize - 5), Return(0)))
        .WillOnce(DoAll(SetArrayArgument<2>(recvData3.cbegin(), recvData3.cend()), SetArgPointee<4>(recvSize - 6), Return(0)));

    std::array<fx_pedal_settings, 4> settings;
    m->load_memory_bank(slot, nullptr, nullptr, settings.data());

    EXPECT_THAT(settings[0].fx_slot, Eq(0));
    EXPECT_THAT(settings[0].knob1, Eq(11));
    EXPECT_THAT(settings[0].knob2, Eq(22));
    EXPECT_THAT(settings[0].knob3, Eq(33));
    EXPECT_THAT(settings[0].knob4, Eq(44));
    EXPECT_THAT(settings[0].knob5, Eq(55));
    EXPECT_THAT(settings[0].knob6, Eq(66));
    EXPECT_THAT(settings[0].put_post_amp, Eq(true));
    EXPECT_THAT(settings[0].effect_num, Eq(value(effects::PHASER)));

    EXPECT_THAT(settings[1].fx_slot, Eq(1));
    EXPECT_THAT(settings[1].knob1, Eq(0));
    EXPECT_THAT(settings[1].knob2, Eq(0));
    EXPECT_THAT(settings[1].knob3, Eq(0));
    EXPECT_THAT(settings[1].knob4, Eq(1));
    EXPECT_THAT(settings[1].knob5, Eq(1));
    EXPECT_THAT(settings[1].knob6, Eq(1));
    EXPECT_THAT(settings[1].put_post_amp, Eq(false));
    EXPECT_THAT(settings[1].effect_num, Eq(value(effects::TRIANGLE_CHORUS)));

    EXPECT_THAT(settings[2].fx_slot, Eq(2));
    EXPECT_THAT(settings[2].knob1, Eq(0));
    EXPECT_THAT(settings[2].knob2, Eq(0));
    EXPECT_THAT(settings[2].knob3, Eq(0));
    EXPECT_THAT(settings[2].knob4, Eq(0));
    EXPECT_THAT(settings[2].knob5, Eq(0));
    EXPECT_THAT(settings[2].knob6, Eq(0));
    EXPECT_THAT(settings[2].put_post_amp, Eq(false));
    EXPECT_THAT(settings[2].effect_num, Eq(value(effects::EMPTY)));

    EXPECT_THAT(settings[3].fx_slot, Eq(3));
    EXPECT_THAT(settings[3].knob1, Eq(1));
    EXPECT_THAT(settings[3].knob2, Eq(2));
    EXPECT_THAT(settings[3].knob3, Eq(3));
    EXPECT_THAT(settings[3].knob4, Eq(4));
    EXPECT_THAT(settings[3].knob5, Eq(5));
    EXPECT_THAT(settings[3].knob6, Eq(6));
    EXPECT_THAT(settings[3].put_post_amp, Eq(true));
    EXPECT_THAT(settings[3].effect_num, Eq(value(effects::TAPE_DELAY)));
}

TEST_F(MustangTest, loadMemoryBankReturnsErrorOnTransferError)
{
    constexpr int errorCode{1};
    EXPECT_CALL(*usbmock, interrupt_transfer(_, _, _, _, _, _)).WillOnce(DoAll(SetArgPointee<4>(0), Return(errorCode)));
    const auto result = m->load_memory_bank(0, nullptr, nullptr, nullptr);
    EXPECT_THAT(result, Eq(errorCode));
}

TEST_F(MustangTest, saveOnAmp)
{
    constexpr int slot{8};
    constexpr std::size_t slotPos{4};
    std::array<std::uint8_t, packetSize> sendCmd{{0}};
    sendCmd[0] = 0x1c;
    sendCmd[1] = 0x01;
    sendCmd[2] = 0x03;
    sendCmd[slotPos] = slot;
    sendCmd[6] = 0x01;
    sendCmd[7] = 0x01;
    std::array<char, 34> nameOversized;
    nameOversized.fill('a');

    for (std::size_t i = 0; i < 31; ++i)
    {
        sendCmd[16 + i] = nameOversized[i];
    }
    nameOversized[31] = char{0x0f};
    nameOversized[32] = 'b';
    nameOversized[33] = '\0';


    EXPECT_CALL(*usbmock, interrupt_transfer(_, 0x01, BufferIs(sendCmd), packetSize, _, _)).WillOnce(DoAll(SetArgPointee<4>(0), Return(0)));
    EXPECT_CALL(*usbmock, interrupt_transfer(_, 0x81, _, packetSize, _, _)).WillOnce(DoAll(SetArgPointee<4>(0), Return(0)));

    std::array<unsigned char, 64> memBank;
    memBank.fill(0x00);
    memBank[0] = 0x1c;
    memBank[1] = 0x01;
    memBank[2] = 0x01;
    memBank[slotPos] = slot;
    memBank[6] = 0x01;
    EXPECT_CALL(*usbmock, interrupt_transfer(_, 0x01, BufferIs(memBank), _, _, _)).WillOnce(DoAll(SetArgPointee<4>(0), Return(0)));

    const auto result = m->save_on_amp(nameOversized.data(), slot);
    EXPECT_THAT(result, Eq(0));
}

#include <gtest/gtest.h>

#include <vector>

#include "EG5CD1_protocol.hpp"
#include "ring_buffer.hpp"
#include "test_helpers.hpp"

using test_helpers::pushLE16;
using test_helpers::withChecksum;

namespace {

// 帧头常量：指令帧头 EB 90，应答帧头 EE 16
constexpr uint8_t kHdr0 = 0xEB;
constexpr uint8_t kHdr1 = 0x90;
constexpr uint8_t kRespHdr0 = 0xEE;
constexpr uint8_t kRespHdr1 = 0x16;

EG5CD1_Protocol makeProto() {
    EG5CD1_Protocol p;
    p.setDeviceId(1);
    return p;
}

} // namespace

// 寄存器名 -> 地址 映射
TEST(EG5CD1Protocol, RegisterAddressLookup) {
    auto p = makeProto();
    EXPECT_EQ(p.getRegisterAddress("openLenSet"), 1020);
    EXPECT_EQ(p.getRegisterAddress("gripperStatusBlock"), 1120);
    EXPECT_EQ(p.getRegisterAddress("touchAct"), 1200);
    EXPECT_EQ(p.getRegisterAddress("不存在"), -1);
}

// 读命令字节序列正确（读命令 0x00）
TEST(EG5CD1Protocol, BuildReadCommand) {
    auto p = makeProto();
    auto cmd = p.buildReadCommand(1120, 14); // gripperStatusBlock, 一次读 14 字节
    std::vector<uint8_t> expected = withChecksum({kHdr0, kHdr1, 0x01, 0x04, 0x00, 0x60, 0x04, 0x0E});
    EXPECT_EQ(cmd, expected);
}

// 写单值命令（数据长度字段 = 3 + 值个数*2，写命令 0x01）
TEST(EG5CD1Protocol, BuildWriteCommandSingleValue) {
    auto p = makeProto();
    auto cmd = p.buildWriteCommand(1020, {500}); // openLenSet
    std::vector<uint8_t> body = {kHdr0, kHdr1, 0x01, static_cast<uint8_t>(3 + 1 * 2), 0x01, 0xFC, 0x03};
    pushLE16(body, 500);
    EXPECT_EQ(cmd, withChecksum(body));
}

// 写双值命令
TEST(EG5CD1Protocol, BuildWriteCommandTwoValues) {
    auto p = makeProto();
    auto cmd = p.buildWriteCommand(1020, {300, 400});
    std::vector<uint8_t> body = {kHdr0, kHdr1, 0x01, static_cast<uint8_t>(3 + 2 * 2), 0x01, 0xFC, 0x03};
    pushLE16(body, 300);
    pushLE16(body, 400);
    EXPECT_EQ(cmd, withChecksum(body));
}

// 空值或超过 2 个值应抛异常
TEST(EG5CD1Protocol, BuildWriteCommandInvalidCountThrows) {
    auto p = makeProto();
    EXPECT_THROW(p.buildWriteCommand(1020, {}), std::runtime_error);
    EXPECT_THROW(p.buildWriteCommand(1020, {1, 2, 3}), std::runtime_error);
}

// 校验和验证：应答帧头 EE 16
TEST(EG5CD1Protocol, ValidateChecksum) {
    auto p = makeProto();
    std::vector<uint8_t> body = {kRespHdr0, kRespHdr1, 0x01, 0x05, 0x00, 0x62, 0x04};
    pushLE16(body, 1234);
    auto frame = withChecksum(body);
    EXPECT_TRUE(p.validateChecksum(frame));

    frame.back() ^= 0xFF;
    EXPECT_FALSE(p.validateChecksum(frame));
}

// 解析读回复
TEST(EG5CD1Protocol, ParseReadResponse) {
    auto p = makeProto();
    std::vector<uint8_t> body = {kRespHdr0, kRespHdr1, 0x01, 0x05, 0x00, 0x62, 0x04};
    pushLE16(body, 1234); // openLenAct
    auto frame = withChecksum(body);

    RingBuffer rb(64);
    rb.push(frame.data(), frame.size());
    auto [ok, values] = p.parseResponse(rb);
    ASSERT_TRUE(ok);
    ASSERT_EQ(values.size(), 1u);
    EXPECT_EQ(values[0], 1234);
}

// 解析读回复：多值 + 负数还原
TEST(EG5CD1Protocol, ParseReadResponseBlock) {
    auto p = makeProto();
    std::vector<int> expected_vals = {10, -5, 1000, 0, 250, 32000, -2000}; // 7 个值=14 字节
    std::vector<uint8_t> body = {kRespHdr0, kRespHdr1, 0x01, static_cast<uint8_t>(14 + 3), 0x00, 0x60, 0x04};
    for (int v : expected_vals) {
        pushLE16(body, v);
    }
    auto frame = withChecksum(body);

    RingBuffer rb(64);
    rb.push(frame.data(), frame.size());
    auto [ok, values] = p.parseResponse(rb);
    ASSERT_TRUE(ok);
    EXPECT_EQ(values, expected_vals);
}

// 解析写回复（命令 0x01），成功且不返回数据
TEST(EG5CD1Protocol, ParseWriteResponse) {
    auto p = makeProto();
    auto frame = withChecksum({kRespHdr0, kRespHdr1, 0x01, 0x03, 0x01, 0x62, 0x04, 0x00});
    RingBuffer rb(64);
    rb.push(frame.data(), frame.size());
    auto [ok, values] = p.parseResponse(rb);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(values.empty());
}

// 应答帧头错误（用了指令帧头 EB 90）应解析失败
TEST(EG5CD1Protocol, ParseResponseRejectsWrongHeader) {
    auto p = makeProto();
    std::vector<uint8_t> body = {kHdr0, kHdr1, 0x01, 0x05, 0x00, 0x62, 0x04};
    pushLE16(body, 1234);
    auto frame = withChecksum(body);

    RingBuffer rb(64);
    rb.push(frame.data(), frame.size());
    auto [ok, values] = p.parseResponse(rb);
    EXPECT_FALSE(ok);
}

// 触觉块（1200 起 20 字节，version=1）：法向/切向方向/切向力 int16；接近觉 uint32
TEST(EG5CD1Protocol, ParseTouchDataVersion1) {
    auto p = makeProto();
    std::vector<uint8_t> body = {kRespHdr0, kRespHdr1, 0x01, static_cast<uint8_t>(20 + 3), 0x00, 0xB0, 0x04};
    pushLE16(body, 100);               // normal right
    pushLE16(body, 500);               // normal left
    pushLE16(body, 200);               // tang dir right
    pushLE16(body, 600);               // tang dir left
    test_helpers::pushLE32(body, 300); // proximity right
    test_helpers::pushLE32(body, 700); // proximity left
    pushLE16(body, 400);               // tang force right
    pushLE16(body, 800);               // tang force left
    auto frame = withChecksum(body);

    RingBuffer rb(128);
    rb.push(frame.data(), frame.size());
    auto [ok, touch] = p.parseTouchData(rb, 1);
    ASSERT_TRUE(ok);
    ASSERT_EQ(touch.fingerResults.count("right"), 1u);
    ASSERT_EQ(touch.fingerResults.count("left"), 1u);
    EXPECT_EQ(touch.fingerResults.at("right"), (std::vector<uint16_t>{100, 200, 400}));
    EXPECT_EQ(touch.fingerResults.at("left"), (std::vector<uint16_t>{500, 600, 800}));
    ASSERT_EQ(touch.proximityResults.count("right"), 1u);
    ASSERT_EQ(touch.proximityResults.count("left"), 1u);
    EXPECT_EQ(touch.proximityResults.at("right"), 300u);
    EXPECT_EQ(touch.proximityResults.at("left"), 700u);
    EXPECT_TRUE(touch.palmResults.empty());
}

TEST(EG5CD1Protocol, ParseTouchDataRejectsShortBlock) {
    auto p = makeProto();
    std::vector<uint8_t> body = {kRespHdr0, kRespHdr1, 0x01, static_cast<uint8_t>(16 + 3), 0x00, 0xB0, 0x04};
    for (int i = 0; i < 8; ++i) {
        pushLE16(body, i);
    }
    auto frame = withChecksum(body);

    RingBuffer rb(128);
    rb.push(frame.data(), frame.size());
    auto [ok, touch] = p.parseTouchData(rb, 1);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(touch.fingerResults.empty());
}

TEST(EG5CD1Protocol, ParseTouchDataRejectsUnsupportedVersion) {
    auto p = makeProto();
    std::vector<uint8_t> body = {kRespHdr0, kRespHdr1, 0x01, static_cast<uint8_t>(20 + 3), 0x00, 0xB0, 0x04};
    pushLE16(body, 0);
    pushLE16(body, 0);
    pushLE16(body, 0);
    pushLE16(body, 0);
    test_helpers::pushLE32(body, 0);
    test_helpers::pushLE32(body, 0);
    pushLE16(body, 0);
    pushLE16(body, 0);
    auto frame = withChecksum(body);

    RingBuffer rb(128);
    rb.push(frame.data(), frame.size());
    auto [ok, touch] = p.parseTouchData(rb, 2);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(touch.fingerResults.empty());
}

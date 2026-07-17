#include "ble_frame_parser.h"
#include <string.h>

static uint8_t calc_crc(const uint8_t *data, uint16_t len) {
    uint8_t crc = 0;
    for (uint16_t i = 0; i < len; i++) crc ^= data[i];
    return crc;
}

bool ble_frame_parse(const uint8_t *data, uint16_t data_len, ble_frame_t *out_frame) {
    if (data_len < 9) return false;
    if (data[0] != BLE_FRAME_SOF) return false;

    out_frame->sof = data[0];
    out_frame->ver = data[1];
    out_frame->type = data[2];
    out_frame->cmd = data[3];
    out_frame->seq = (data[4] << 8) | data[5];
    out_frame->len = (data[6] << 8) | data[7];

    uint16_t header_len = (out_frame->type == 0x02) ? 9 : 8;
    if (out_frame->type == 0x02) {
        out_frame->status = data[8];
    }

    if (data_len < header_len + out_frame->len + 1) return false;

    uint8_t expected_crc = data[header_len + out_frame->len];
    uint8_t actual_crc = calc_crc(data, header_len + out_frame->len);
    if (expected_crc != actual_crc) return false;

    out_frame->payload = (uint8_t*)(data + header_len);
    out_frame->crc = actual_crc;

    return true;
}

uint16_t ble_frame_build_resp(uint8_t cmd, uint16_t seq, uint8_t status, const uint8_t *payload, uint16_t payload_len, uint8_t *out_buf) {
    out_buf[0] = BLE_FRAME_SOF;
    out_buf[1] = 0x02; // ver
    out_buf[2] = 0x02; // RESPONSE
    out_buf[3] = cmd;
    out_buf[4] = (seq >> 8) & 0xFF;
    out_buf[5] = seq & 0xFF;
    out_buf[6] = (payload_len >> 8) & 0xFF;
    out_buf[7] = payload_len & 0xFF;
    out_buf[8] = status;

    if (payload_len > 0 && payload != NULL) {
        memcpy(&out_buf[9], payload, payload_len);
    }

    out_buf[9 + payload_len] = calc_crc(out_buf, 9 + payload_len);
    return 10 + payload_len;
}

uint16_t ble_frame_build_req(uint8_t cmd, uint16_t seq, const uint8_t *payload, uint16_t payload_len, uint8_t *out_buf) {
    out_buf[0] = BLE_FRAME_SOF;
    out_buf[1] = 0x02; // ver
    out_buf[2] = 0x01; // REQUEST
    out_buf[3] = cmd;
    out_buf[4] = (seq >> 8) & 0xFF;
    out_buf[5] = seq & 0xFF;
    out_buf[6] = (payload_len >> 8) & 0xFF;
    out_buf[7] = payload_len & 0xFF;

    if (payload_len > 0 && payload != NULL) {
        memcpy(&out_buf[8], payload, payload_len);
    }

    out_buf[8 + payload_len] = calc_crc(out_buf, 8 + payload_len);
    return 9 + payload_len;
}

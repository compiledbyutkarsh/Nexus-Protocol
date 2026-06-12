#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "protocol.h"

static const uint32_t crc32_table[256] = {
    0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,
    0xE963A535,0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91B,0x97D2D988,
    0x09B64C2B,0x7EB17CBF,0xE7B82D09,0x90BF1D9F,0x1DB71064,0x6AB020F2,
    0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
    0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,
    0xFA0F3D63,0x8D080DF5,0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,
    0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,0x35B5A8FA,0x42B2986C,
    0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
    0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F928,0x56B3C9BE,
    0xCFBA9599,0xB8BDA50F,0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,
    0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,0x76DC4190,0x01DB7106,
    0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
    0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6372BB,0x086D3D2D,
    0x91646C97,0xE6635C01,0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,
    0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,
    0x8BBEB8EA,0xFCB9887C,0x62DD1D7F,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
    0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,
    0xA4D1C46D,0xD3D6F4FB,0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,
    0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,0x5005713C,0x270241AA,
    0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
    0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,
    0xB7BD5C3B,0xC0BA6CAD,0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,
    0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,
    0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
    0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,
    0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,
    0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,0xD6D6A3E8,0xA1D1937E,
    0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
    0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA8670955,
    0x316658EF,0x46616879,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D,
    0x77073097,0xEE0E612F,0x990951BD,0x076DC41B,0x706AF48D,0xE963A533,
    0x9E6495A5,0x0EDB8836,0x79DCB8A2,0xE0D5E91D,0x97D2D98B,0x09B64C2D,
    0x7EB17CB9,0xE7B82D0F,0x90BF1D99,0x1DB71062,0x6AB020F4,0xF3B9714A,
    0x84BE41DC,0x1ADAD47B,0x6DDDE4ED,0xF4D4B553,0x83D385C5,0x136C9850,
    0x646BA8C6,0xFD62F97C,0x8A65C9EA,0x14015C49,0x63066CDF,0xFA0F3D65,
    0x8D080DF3,0x3B6E20CA,0x4C69105C,0xD56041E6,0xA2677174,0x3C03E4D3,
    0x4B04D441,0xD20D85FB,0xA50AB56D,0x35B5A8FC,0x42B2986A,0xDBBBC9D4,
    0xACBCF946,0x32D86CE5,0x45DF5C73,0xDCD60DC9,0xABD13D5F,0x26D930AA,
    0x51DE003C,0xC8D75182,0xBFD06110,0x21B4F92A,0x56B3C9B8,0xCFBA959B,
    0xB8BDA509,0x2802B89C,0x5F05880A,0xC60CD9B0,0xB10BE926,0x2F6F7C85,
    0x58684C13,0xC1611DA9,0xB6662D3F,0x76DC4192,0x01DB7104,0x98D220BE,
    0xEFD5102C,0x71B1858B,0x06B6B519,0x9FBFE4A7,0xE8B8D431,0x7807C9A0
};

uint32_t crc32_compute(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    return crc ^ 0xFFFFFFFF;
}

int frame_encode(const frame_t *frame, uint8_t *buf, size_t buf_len)
{
    if (!frame || !buf)
        return PROTO_ERR_IO;

    uint32_t plen  = frame->header.payload_len;
    size_t   total = PROTO_HEADER_SIZE + plen;

    if (buf_len < total)
        return PROTO_ERR_LEN;

    frame_header_t h;
    memset(&h, 0, sizeof(h));
    h.magic       = htonl(PROTO_MAGIC);
    h.version     = PROTO_VERSION;
    h.type        = frame->header.type;
    h.flags       = htons(frame->header.flags);
    h.seq         = htonl(frame->header.seq);
    h.payload_len = htonl(plen);
    h.checksum    = 0;

    memcpy(buf, &h, PROTO_HEADER_SIZE);

    if (plen > 0 && frame->payload)
        memcpy(buf + PROTO_HEADER_SIZE, frame->payload, plen);

    uint32_t crc    = crc32_compute(buf, total);
    uint32_t crc_be = htonl(crc);
    memcpy(buf + offsetof(frame_header_t, checksum), &crc_be, 4);

    return (int)total;
}

int frame_decode(const uint8_t *buf, size_t buf_len, frame_t *out)
{
    if (!buf || !out || buf_len < PROTO_HEADER_SIZE)
        return PROTO_ERR_LEN;

    frame_header_t h;
    memcpy(&h, buf, PROTO_HEADER_SIZE);

    if (ntohl(h.magic) != PROTO_MAGIC)
        return PROTO_ERR_MAGIC;

    uint32_t plen = ntohl(h.payload_len);
    if (buf_len < PROTO_HEADER_SIZE + plen)
        return PROTO_ERR_LEN;

    uint32_t recv_crc = ntohl(h.checksum);

    uint8_t *tmp = malloc(PROTO_HEADER_SIZE + plen);
    if (!tmp)
        return PROTO_ERR_IO;

    memcpy(tmp, buf, PROTO_HEADER_SIZE + plen);
    memset(tmp + offsetof(frame_header_t, checksum), 0, 4);

    uint32_t calc_crc = crc32_compute(tmp, PROTO_HEADER_SIZE + plen);
    free(tmp);

    if (recv_crc != calc_crc)
        return PROTO_ERR_CRC;

    out->header             = h;
    out->header.magic       = ntohl(h.magic);
    out->header.flags       = ntohs(h.flags);
    out->header.seq         = ntohl(h.seq);
    out->header.payload_len = plen;
    out->header.checksum    = recv_crc;
    out->payload            = NULL;

    if (plen > 0) {
        out->payload = malloc(plen);
        if (!out->payload)
            return PROTO_ERR_IO;
        memcpy(out->payload, buf + PROTO_HEADER_SIZE, plen);
    }

    return PROTO_OK;
}

void frame_free(frame_t *frame)
{
    if (frame && frame->payload) {
        free(frame->payload);
        frame->payload = NULL;
    }
}

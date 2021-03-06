// Copyright (C) 2017 Vasily Evseenko <svpcom@p2ptech.org>

/*
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 3.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#ifndef __WIFIBROADCAST_HPP__
#define __WIFIBROADCAST_HPP__

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <resolv.h>
#include <string.h>
#include <utime.h>
#include <unistd.h>
#include <getopt.h>
#include <pcap.h>
#include <endian.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <sodium.h>

#define MAX_PACKET_SIZE 1510
#define MAX_RX_INTERFACES 8

using namespace std;

template<typename ... Args>
string string_format( const char *format, Args ... args )
{
    size_t size = snprintf(nullptr, 0, format, args ...) + 1; // Extra space for '\0'
    unique_ptr<char[]> buf(new char[ size ]);
    snprintf(buf.get(), size, format, args ...);
    return string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}


/* this is the template radiotap header we send packets out with */


#define IEEE80211_RADIOTAP_MCS_HAVE_BW          0x01
#define IEEE80211_RADIOTAP_MCS_HAVE_MCS         0x02
#define IEEE80211_RADIOTAP_MCS_HAVE_GI          0x04
#define IEEE80211_RADIOTAP_MCS_HAVE_FMT         0x08

#define         IEEE80211_RADIOTAP_MCS_BW_20    0
#define         IEEE80211_RADIOTAP_MCS_BW_40    1
#define         IEEE80211_RADIOTAP_MCS_BW_20L   2
#define         IEEE80211_RADIOTAP_MCS_BW_20U   3
#define IEEE80211_RADIOTAP_MCS_SGI              0x04
#define IEEE80211_RADIOTAP_MCS_FMT_GF           0x08

#define MCS_KNOWN (IEEE80211_RADIOTAP_MCS_HAVE_MCS | IEEE80211_RADIOTAP_MCS_HAVE_BW | IEEE80211_RADIOTAP_MCS_HAVE_GI) // | IEEE80211_RADIOTAP_MCS_HAVE_FMT)
#define MCS_FLAGS (IEEE80211_RADIOTAP_MCS_BW_40 | IEEE80211_RADIOTAP_MCS_SGI) // | IEEE80211_RADIOTAP_MCS_FMT_GF)

static const uint8_t radiotap_header[] = {
    0x00, 0x00, // <-- radiotap version
    0x0d, 0x00, // <- radiotap header length
    0x00, 0x80, 0x08, 0x00, // <-- radiotap present flags:  RADIOTAP_TX_FLAGS + RADIOTAP_MCS
    0x08, 0x00,  // RADIOTAP_F_TX_NOACK
    MCS_KNOWN , MCS_FLAGS, 0x01  // MCS default is #1 -- QPSK 1/2 40MHz SGI -- 30 Mbit/s
};

//the last byte of the mac address is recycled as a port number
#define SRC_MAC_LASTBYTE 15
#define DST_MAC_LASTBYTE 21
#define FRAME_SEQ_LB 22
#define FRAME_SEQ_HB 23

static uint8_t ieee80211_header[] = {
    0x08, 0x01, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x13, 0x22, 0x33, 0x44, 0x55, 0x66,
    0x13, 0x22, 0x33, 0x44, 0x55, 0x66,
    0x00, 0x00,  // seq num << 4 + fragment num
};


// nounce:  56bit block_idx + 8bit fragment_idx

#define BLOCK_IDX_MASK ((1LU << 56) - 1)

#define WFB_PACKET_DATA 0x1
#define WFB_PACKET_KEY 0x2

#define SESSION_KEY_ANNOUNCE_MSEC 1000

typedef struct {
    uint8_t packet_type;
    uint64_t nonce;
}  __attribute__ ((packed)) wblock_hdr_t;

typedef struct {
    uint32_t seq;
    uint16_t packet_size;
}  __attribute__ ((packed)) wpacket_hdr_t;

typedef struct {
    uint8_t packet_type;
    uint8_t nonce[crypto_box_NONCEBYTES];
    uint8_t session_key[crypto_aead_chacha20poly1305_KEYBYTES + crypto_box_MACBYTES]; // encrypted session key
} __attribute__ ((packed)) wsession_key_t;

#define MAX_PAYLOAD_SIZE (MAX_PACKET_SIZE - sizeof(radiotap_header) - sizeof(ieee80211_header) - sizeof(wblock_hdr_t) - crypto_aead_chacha20poly1305_ABYTES - sizeof(wpacket_hdr_t))
#define MAX_FEC_PAYLOAD  (MAX_PACKET_SIZE - sizeof(radiotap_header) - sizeof(ieee80211_header) - sizeof(wblock_hdr_t) - crypto_aead_chacha20poly1305_ABYTES)
#define MAX_FORWARDER_PACKET_SIZE (MAX_PACKET_SIZE - sizeof(radiotap_header) - sizeof(ieee80211_header))

int open_udp_socket_for_rx(int port);

#endif

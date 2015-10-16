/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2015 The srsUE Developers. See the
 * COPYRIGHT file at the top-level directory of this distribution.
 *
 * \section LICENSE
 *
 * This file is part of the srsUE library.
 *
 * srsUE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsUE is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#include "upper/rlc_um.h"

#define RX_MOD_BASE(x) (x-vr_uh-rx_window_size)%rx_mod

using namespace srslte;

namespace srsue{

rlc_um::rlc_um()
{
  tx_sdu = NULL;
  rx_sdu = NULL;
  pool = buffer_pool::get_instance();

  vt_us    = 0;

  vr_ur    = 0;
  vr_ux    = 0;
  vr_uh    = 0;
}

void rlc_um::init(srslte::log        *log_,
                  uint32_t            lcid_,
                  pdcp_interface_rlc *pdcp_,
                  rrc_interface_rlc  *rrc_)
{
  log  = log_;
  lcid = lcid_;
  pdcp = pdcp_;
  rrc  = rrc_;
}

void rlc_um::configure(LIBLTE_RRC_RLC_CONFIG_STRUCT *cnfg)
{
  switch(cnfg->rlc_mode)
  {
  case LIBLTE_RRC_RLC_MODE_UM_BI:
    t_reordering        = cnfg->dl_um_bi_rlc.t_reordering;
    rx_sn_field_length  = (rlc_umd_sn_size_t)cnfg->dl_um_bi_rlc.sn_field_len;
    rx_window_size      = (RLC_UMD_SN_SIZE_5_BITS == rx_sn_field_length) ? 16 : 512;
    rx_mod              = (RLC_UMD_SN_SIZE_5_BITS == rx_sn_field_length) ? 32 : 1024;
    tx_sn_field_length  = (rlc_umd_sn_size_t)cnfg->ul_um_bi_rlc.sn_field_len;
    tx_mod              = (RLC_UMD_SN_SIZE_5_BITS == tx_sn_field_length) ? 32 : 1024;
    log->info("%s configured in %s mode: "
              "t_reordering=%d ms, rx_sn_field_length=%u bits, tx_sn_field_length=%u bits\n",
              srsue_rb_id_text[lcid], liblte_rrc_rlc_mode_text[cnfg->rlc_mode],
              liblte_rrc_t_reordering_num[t_reordering],
              rlc_umd_sn_size_num[rx_sn_field_length],
              rlc_umd_sn_size_num[tx_sn_field_length]);
    break;
  case LIBLTE_RRC_RLC_MODE_UM_UNI_UL:
    tx_sn_field_length  = (rlc_umd_sn_size_t)cnfg->ul_um_uni_rlc.sn_field_len;
    tx_mod              = (RLC_UMD_SN_SIZE_5_BITS == tx_sn_field_length) ? 32 : 1024;
    log->info("%s configured in %s mode: tx_sn_field_length=%u bits\n",
              srsue_rb_id_text[lcid], liblte_rrc_rlc_mode_text[cnfg->rlc_mode],
              rlc_umd_sn_size_num[tx_sn_field_length]);
    break;
  case LIBLTE_RRC_RLC_MODE_UM_UNI_DL:
    t_reordering        = cnfg->dl_um_uni_rlc.t_reordering;
    rx_sn_field_length  = (rlc_umd_sn_size_t)cnfg->dl_um_uni_rlc.sn_field_len;
    rx_window_size      = (RLC_UMD_SN_SIZE_5_BITS == rx_sn_field_length) ? 16 : 512;
    rx_mod              = (RLC_UMD_SN_SIZE_5_BITS == rx_sn_field_length) ? 32 : 1024;
    log->info("%s configured in %s mode: "
              "t_reordering=%d ms, rx_sn_field_length=%u bits\n",
              srsue_rb_id_text[lcid], liblte_rrc_rlc_mode_text[cnfg->rlc_mode],
              liblte_rrc_t_reordering_num[t_reordering],
              rlc_umd_sn_size_num[rx_sn_field_length]);
    break;
  default:
    log->error("RLC configuration mode not recognized\n");
  }
}

rlc_mode_t rlc_um::get_mode()
{
  return RLC_MODE_UM;
}

uint32_t rlc_um::get_bearer()
{
  return lcid;
}

/****************************************************************************
 * PDCP interface
 ***************************************************************************/

void rlc_um::write_sdu(srsue_byte_buffer_t *sdu)
{
  log->info_hex(sdu->msg, sdu->N_bytes, "%s Tx SDU", srsue_rb_id_text[lcid]);
  tx_sdu_queue.write(sdu);
}

bool rlc_um::read_sdu()
{
  int n_sdus = rx_sdu_queue.size();
  if(n_sdus == 0)
    return false;

  srsue_byte_buffer_t *sdu;
  for(int i=0; i<n_sdus; i++)
  {
    rx_sdu_queue.read(&sdu);
    log->info_hex(sdu->msg, sdu->N_bytes, "%s Rx SDU", srsue_rb_id_text[lcid]);
    pdcp->write_pdu(lcid, sdu);
  }

  return true;
}

/****************************************************************************
 * MAC interface
 ***************************************************************************/

uint32_t rlc_um::get_buffer_state()
{
  // Bytes needed for tx SDUs
  uint32_t n_sdus  = tx_sdu_queue.size();
  uint32_t n_bytes = tx_sdu_queue.size_bytes();
  if(tx_sdu)
  {
    n_sdus++;
    n_bytes += tx_sdu->N_bytes;
  }

  // Room needed for header extensions? (integer rounding)
  if(n_sdus > 1)
    n_bytes += ((n_sdus-1)*1.5)+0.5;

  // Room needed for fixed header?
  if(n_bytes > 0)
    n_bytes += 2;

  return n_bytes;
}

int rlc_um::read_pdu(uint8_t *payload, uint32_t nof_bytes)
{
  log->info("MAC opportunity - %d bytes\n", nof_bytes);
  return build_data_pdu(payload, nof_bytes);
}

void rlc_um::write_pdu(uint8_t *payload, uint32_t nof_bytes)
{
  boost::lock_guard<boost::mutex> lock(mutex);
  handle_data_pdu(payload, nof_bytes);
}

/****************************************************************************
 * Timeout callback interface
 ***************************************************************************/

void rlc_um::timeout_expired(uint32_t timeout_id)
{
  if(reordering_timeout_id == timeout_id)
  {
    boost::lock_guard<boost::mutex> lock(mutex);

    // 36.322 v10 Section 5.1.2.2.4
    log->debug("%s reordering timeout expiry - updating vr_ur\n", srsue_rb_id_text[lcid]);

    rx_sdu->reset();            // We only get here if we've lost a PDU
    while(RX_MOD_BASE(vr_ur) < RX_MOD_BASE(vr_ux))
    {
      vr_ur = (vr_ur + 1)%rx_mod;
      reassemble_rx_sdus();
    }
    reordering_timeout.reset();
    if(RX_MOD_BASE(vr_uh) > RX_MOD_BASE(vr_ur))
    {
      reordering_timeout.start(t_reordering, reordering_timeout_id, this);
      vr_ux = vr_uh;
    }

    debug_state();
  }
}

bool rlc_um::reordering_timeout_running()
{
  return reordering_timeout.is_running();
}

/****************************************************************************
 * Helpers
 ***************************************************************************/

int  rlc_um::build_data_pdu(uint8_t *payload, uint32_t nof_bytes)
{
  if(!tx_sdu && tx_sdu_queue.size() == 0)
  {
    log->info("No data available to be sent");
    return 0;
  }

  srsue_byte_buffer_t *pdu = pool->allocate();
  rlc_umd_pdu_header_t header;
  header.fi   = RLC_FI_FIELD_START_AND_END_ALIGNED;
  header.sn   = vt_us;
  header.N_li = 0;
  header.sn_size = tx_sn_field_length;

  uint32_t head_len  = rlc_um_packed_length(&header);
  uint32_t to_move   = 0;
  uint32_t last_li   = 0;
  uint32_t pdu_space = nof_bytes;
  uint8_t *pdu_ptr   = pdu->msg;

  if(pdu_space <= head_len)
  {
    log->warning("%s Cannot build a PDU - %d bytes available, %d bytes required for header\n",
                 srsue_rb_id_text[lcid], nof_bytes, head_len);
    return 0;
  }

  // Check for SDU segment
  if(tx_sdu)
  {
    to_move = ((pdu_space-head_len) >= tx_sdu->N_bytes) ? tx_sdu->N_bytes : pdu_space-head_len;
    memcpy(pdu_ptr, tx_sdu->msg, to_move);
    last_li          = to_move;
    pdu_ptr         += to_move;
    pdu->N_bytes    += to_move;
    tx_sdu->N_bytes -= to_move;
    tx_sdu->msg     += to_move;
    if(tx_sdu->N_bytes == 0)
    {
      pool->deallocate(tx_sdu);
      tx_sdu = NULL;
    }
    pdu_space -= to_move;
    header.fi |= RLC_FI_FIELD_NOT_START_ALIGNED; // First byte does not correspond to first byte of SDU
  }

  // Pull SDUs from queue
  while(pdu_space > head_len && tx_sdu_queue.size() > 0)
  {
    if(last_li > 0)
      header.li[header.N_li++] = last_li;
    head_len = rlc_um_packed_length(&header);
    tx_sdu_queue.read(&tx_sdu);
    to_move = ((pdu_space-head_len) >= tx_sdu->N_bytes) ? tx_sdu->N_bytes : pdu_space-head_len;
    memcpy(pdu_ptr, tx_sdu->msg, to_move);
    last_li          = to_move;
    pdu_ptr         += to_move;
    pdu->N_bytes    += to_move;
    tx_sdu->N_bytes -= to_move;
    tx_sdu->msg     += to_move;
    if(tx_sdu->N_bytes == 0)
    {
      pool->deallocate(tx_sdu);
      tx_sdu = NULL;
    }
    pdu_space -= to_move;
  }

  if(tx_sdu)
    header.fi |= RLC_FI_FIELD_NOT_END_ALIGNED; // Last byte does not correspond to last byte of SDU

  // Set SN
  header.sn = vt_us;
  vt_us = (vt_us + 1)%tx_mod;

  // Add header and TX
  rlc_um_write_data_pdu_header(&header, pdu);
  memcpy(payload, pdu->msg, pdu->N_bytes);

  debug_state();
  return pdu->N_bytes;
}

void rlc_um::handle_data_pdu(uint8_t *payload, uint32_t nof_bytes)
{
  std::map<uint32_t, rlc_umd_pdu_t>::iterator it;
  rlc_umd_pdu_header_t header;
  rlc_um_read_data_pdu_header(payload, nof_bytes, &header);

  log->info_hex(payload, nof_bytes, "%s Rx data PDU SN: %d",
                srsue_rb_id_text[lcid], header.sn);

  if(RX_MOD_BASE(header.sn) >= RX_MOD_BASE(vr_uh-rx_window_size) &&
     RX_MOD_BASE(header.sn) <  RX_MOD_BASE(vr_ur))
  {
    log->info("%s SN: %d outside rx window [%d:%d] - discarding\n",
              srsue_rb_id_text[lcid], header.sn, vr_ur, vr_uh);
    return;
  }
  it = rx_window.find(header.sn);
  if(rx_window.end() != it)
  {
    log->info("%s Discarding duplicate SN: %d\n",
              srsue_rb_id_text[lcid], header.sn);
    return;
  }

  // Write to rx window
  rlc_umd_pdu_t pdu;
  pdu.buf = pool->allocate();
  memcpy(pdu.buf->msg, payload, nof_bytes);
  pdu.buf->N_bytes = nof_bytes;
  //Strip header from PDU
  int header_len = rlc_um_packed_length(&header);
  pdu.buf->msg += header_len;
  pdu.buf->N_bytes -= header_len;
  pdu.header = header;
  rx_window[header.sn] = pdu;

  // Update vr_uh
  if(!inside_reordering_window(header.sn))
    vr_uh  = (header.sn + 1)%rx_mod;

  // Reassemble and deliver SDUs, while updating vr_ur
  reassemble_rx_sdus();

  // Update reordering variables and timers
  if(reordering_timeout.is_running())
  {
    if(RX_MOD_BASE(vr_ux) <= RX_MOD_BASE(vr_ur) ||
       (!inside_reordering_window(vr_ux) && vr_ux != vr_uh))
    {
      reordering_timeout.reset();
    }
  }
  if(!reordering_timeout.is_running())
  {
    if(RX_MOD_BASE(vr_uh) > RX_MOD_BASE(vr_ur))
    {
      reordering_timeout.start(t_reordering, reordering_timeout_id, this);
      vr_ux = vr_uh;
    }
  }

  debug_state();
}

void rlc_um::reassemble_rx_sdus()
{
  if(!rx_sdu)
    rx_sdu = pool->allocate();

  // First catch up with lower edge of reordering window
  while(!inside_reordering_window(vr_ur))
  {
    if(rx_window.end() == rx_window.find(vr_ur))
    {
      rx_sdu->reset();
    }else{
      // Handle any SDU segments (TODO: If the previous PDU was missing and first segment is not start aligned, discard)
      for(int i=0; i<rx_window[vr_ur].header.N_li; i++)
      {
        int len = rx_window[vr_ur].header.li[i];
        memcpy(&rx_sdu->msg[rx_sdu->N_bytes], rx_window[vr_ur].buf->msg, len);
        rx_sdu->N_bytes += len;
        rx_window[vr_ur].buf->msg += len;
        rx_window[vr_ur].buf->N_bytes -= len;
        rx_sdu_queue.write(rx_sdu);
        rx_sdu = pool->allocate();
      }

      // Handle last segment
      memcpy(&rx_sdu->msg[rx_sdu->N_bytes], rx_window[vr_ur].buf->msg, rx_window[vr_ur].buf->N_bytes);
      rx_sdu->N_bytes += rx_window[vr_ur].buf->N_bytes;
      if(rlc_um_end_aligned(rx_window[vr_ur].header.fi))
      {
        rx_sdu_queue.write(rx_sdu);
        rx_sdu = pool->allocate();
      }
    }

    vr_ur = (vr_ur + 1)%rx_mod;
  }


  // Now update vr_ur until we reach an SN we haven't yet received
  while(rx_window.end() != rx_window.find(vr_ur))
  {
    // Handle any SDU segments
    for(int i=0; i<rx_window[vr_ur].header.N_li; i++)
    {
      int len = rx_window[vr_ur].header.li[i];
      memcpy(&rx_sdu->msg[rx_sdu->N_bytes], rx_window[vr_ur].buf->msg, len);
      rx_sdu->N_bytes += len;
      rx_window[vr_ur].buf->msg += len;
      rx_window[vr_ur].buf->N_bytes -= len;
      rx_sdu_queue.write(rx_sdu);
      rx_sdu = pool->allocate();
    }

    // Handle last segment
    memcpy(&rx_sdu->msg[rx_sdu->N_bytes], rx_window[vr_ur].buf->msg, rx_window[vr_ur].buf->N_bytes);
    rx_sdu->N_bytes += rx_window[vr_ur].buf->N_bytes;
    if(rlc_um_end_aligned(rx_window[vr_ur].header.fi))
    {
      rx_sdu_queue.write(rx_sdu);
      rx_sdu = pool->allocate();
    }

    vr_ur = (vr_ur + 1)%rx_mod;
  }
}

bool rlc_um::inside_reordering_window(uint16_t sn)
{
  if(RX_MOD_BASE(sn) >= RX_MOD_BASE(vr_uh-rx_window_size) &&
     RX_MOD_BASE(sn) <  RX_MOD_BASE(vr_uh))
  {
    return true;
  }else{
    return false;
  }
}

void rlc_um::debug_state()
{
  log->debug("%s vt_us = %d, vr_ur = %d, vr_ux = %d, vr_uh = %d \n",
             srsue_rb_id_text[lcid], vt_us, vr_ur, vr_ux, vr_uh);

}

/****************************************************************************
 * Header pack/unpack helper functions
 * Ref: 3GPP TS 36.322 v10.0.0 Section 6.2.1
 ***************************************************************************/

void rlc_um_read_data_pdu_header(srsue_byte_buffer_t *pdu, rlc_umd_pdu_header_t *header)
{
  rlc_um_read_data_pdu_header(pdu->msg, pdu->N_bytes, header);
}

void rlc_um_read_data_pdu_header(uint8_t *payload, uint32_t nof_bytes, rlc_umd_pdu_header_t *header)
{
  uint8_t  ext;
  uint8_t *ptr = payload;

  // Fixed part
  if(RLC_UMD_SN_SIZE_5_BITS == header->sn_size)
  {
    header->fi = (rlc_fi_field_t)((*ptr >> 6) & 0x03);  // 2 bits FI
    ext        =                 ((*ptr >> 5) & 0x01);  // 1 bit EXT
    header->sn =                 *ptr & 0x1F;           // 5 bits SN
    ptr++;
  }else{
    header->fi = (rlc_fi_field_t)((*ptr >> 3) & 0x03);  // 2 bits FI
    ext        =                 ((*ptr >> 2) & 0x01);  // 1 bit EXT
    header->sn =                 (*ptr & 0x03) << 8;    // 2 bits SN
    ptr++;
    header->sn |=                (*ptr & 0xFF);         // 8 bits SN
    ptr++;
  }

  // Extension part
  header->N_li = 0;
  while(ext)
  {
    if(header->N_li%2 == 0)
    {
      ext = ((*ptr >> 7) & 0x01);
      header->li[header->N_li]  = (*ptr & 0x7F) << 4; // 7 bits of LI
      ptr++;
      header->li[header->N_li] |= (*ptr & 0xF0) >> 4; // 4 bits of LI
      header->N_li++;
    }
    else
    {
      ext = (*ptr >> 3) & 0x01;
      header->li[header->N_li] = (*ptr & 0x07) << 8; // 3 bits of LI
      ptr++;
      header->li[header->N_li] |= (*ptr & 0xFF);     // 8 bits of LI
      header->N_li++;
      ptr++;
    }
  }
}

void rlc_um_write_data_pdu_header(rlc_umd_pdu_header_t *header, srsue_byte_buffer_t *pdu)
{
  uint32_t i;
  uint8_t ext = (header->N_li > 0) ? 1 : 0;

  // Make room for the header
  uint32_t len = rlc_um_packed_length(header);
  pdu->msg -= len;
  uint8_t *ptr = pdu->msg;

  // Fixed part
  if(RLC_UMD_SN_SIZE_5_BITS == header->sn_size)
  {
    *ptr  = (header->fi & 0x03) << 6;   // 2 bits FI
    *ptr |= (ext        & 0x01) << 5;   // 1 bit EXT
    *ptr |= header->sn  & 0x1F;         // 5 bits SN
    ptr++;
  }else{
    *ptr  = (header->fi & 0x03) << 3;   // 3 Reserved bits | 2 bits FI
    *ptr |= (ext        & 0x01) << 2;   // 1 bit EXT
    *ptr |= (header->sn & 0x300) >> 8;  // 2 bits SN
    ptr++;
    *ptr  = (header->sn & 0xFF);        // 8 bits SN
    ptr++;
  }

  // Extension part
  i = 0;
  while(i < header->N_li)
  {
    ext = ((i+1) == header->N_li) ? 0 : 1;
    *ptr  = (ext           &  0x01) << 7; // 1 bit header
    *ptr |= (header->li[i] & 0x7F0) >> 4; // 7 bits of LI
    ptr++;
    *ptr  = (header->li[i] & 0x00F) << 4; // 4 bits of LI
    i++;
    if(i < header->N_li)
    {
      ext = ((i+1) == header->N_li) ? 0 : 1;
      *ptr |= (ext           &  0x01) << 3; // 1 bit header
      *ptr |= (header->li[i] & 0x700) >> 8; // 3 bits of LI
      ptr++;
      *ptr  = (header->li[i] & 0x0FF);      // 8 bits of LI
      ptr++;
      i++;
    }
  }
  // Pad if N_li is odd
  if(header->N_li%2 == 1)
    ptr++;

  pdu->N_bytes += ptr-pdu->msg;
}

uint32_t rlc_um_packed_length(rlc_umd_pdu_header_t *header)
{
  uint32_t len = 0;
  if(RLC_UMD_SN_SIZE_5_BITS == header->sn_size)
  {
    len += 1; // Fixed part is 1 byte
  }else{
    len += 2; // Fixed part is 2 bytes
  }
  len += header->N_li * 1.5 + 0.5;  // Extension part - integer rounding up
  return len;
}

bool rlc_um_start_aligned(uint8_t fi)
{
  return (fi == RLC_FI_FIELD_START_AND_END_ALIGNED || fi == RLC_FI_FIELD_NOT_END_ALIGNED);
}

bool rlc_um_end_aligned(uint8_t fi)
{
  return (fi == RLC_FI_FIELD_START_AND_END_ALIGNED || fi == RLC_FI_FIELD_NOT_START_ALIGNED);
}

} // namespace srsue

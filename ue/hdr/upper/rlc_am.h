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

#ifndef RLC_AM_H
#define RLC_AM_H

#include "common/buffer_pool.h"
#include "common/log.h"
#include "common/common.h"
#include "common/msg_queue.h"
#include "common/timeout.h"
#include "upper/rlc_entity.h"
#include <boost/thread/mutex.hpp>
#include <boost/circular_buffer.hpp>
#include <map>
#include <queue>

namespace srsue {

struct rlc_amd_rx_pdu_t{
  rlc_amd_pdu_header_t  header;
  srsue_byte_buffer_t  *buf;
  bool                  pdu_complete;
};

struct rlc_amd_tx_pdu_t{
  rlc_amd_pdu_header_t  header;
  srsue_byte_buffer_t  *buf;
  uint32_t              retx_count;
  bool                  is_acked;
};

class rlc_am
    :public rlc_entity
    ,public timeout_callback
{
public:
  rlc_am();
  void init(srslte::log        *rlc_entity_log_,
            uint32_t            lcid_,
            pdcp_interface_rlc *pdcp_,
            rrc_interface_rlc  *rrc_);
  void configure(LIBLTE_RRC_RLC_CONFIG_STRUCT *cnfg);

  rlc_mode_t    get_mode();
  uint32_t      get_bearer();

  // PDCP interface
  void write_sdu(srsue_byte_buffer_t *sdu);
  bool read_sdu();

  // MAC interface
  uint32_t get_buffer_state();
  int      read_pdu(uint8_t *payload, uint32_t nof_bytes);
  void     write_pdu(uint8_t *payload, uint32_t nof_bytes);

  // Timeout callback interface
  void timeout_expired(uint32_t timeout_id);

private:

  buffer_pool        *pool;
  srslte::log        *log;
  uint32_t            lcid;
  pdcp_interface_rlc *pdcp;
  rrc_interface_rlc  *rrc;

  // TX SDU buffers
  msg_queue            tx_sdu_queue;
  srsue_byte_buffer_t *tx_sdu;

  // Tx and Rx windows
  std::map<uint32_t, rlc_amd_tx_pdu_t>  tx_window;
  std::queue<uint32_t>                  retx_queue;
  std::map<uint32_t, rlc_amd_rx_pdu_t>  rx_window;

  // RX SDU buffers
  msg_queue            rx_sdu_queue;
  srsue_byte_buffer_t *rx_sdu;

  // Mutexes
  boost::mutex        mutex;

  bool                poll_received;
  bool                do_status;
  rlc_status_pdu_t    status;

  /****************************************************************************
   * Configurable parameters
   * Ref: 3GPP TS 36.322 v10.0.0 Section 7
   ***************************************************************************/

  // UL configs
  int32_t    t_poll_retx;      // Poll retx timeout (ms)
  int32_t    poll_pdu;         // Insert poll bit after this many PDUs
  int32_t    poll_byte;        // Insert poll bit after this much data (KB)
  int32_t    max_retx_thresh;  // Max number of retx

  // DL configs
  int32_t   t_reordering;       // Timer used by rx to detect PDU loss  (ms)
  int32_t   t_status_prohibit;  // Timer used by rx to prohibit tx of status PDU (ms)

  /****************************************************************************
   * State variables and counters
   * Ref: 3GPP TS 36.322 v10.0.0 Section 7
   ***************************************************************************/

  // Tx state variables
  uint32_t vt_a;    // ACK state. SN of next PDU in sequence to be ACKed. Low edge of tx window.
  uint32_t vt_ms;   // Max send state. High edge of tx window. vt_a + window_size.
  uint32_t vt_s;    // Send state. SN to be assigned for next PDU.
  uint32_t poll_sn; // Poll send state. SN of most recent PDU txed with poll bit set.

  // Tx counters
  uint32_t pdu_without_poll;
  uint32_t byte_without_poll;

  // Rx state variables
  uint32_t vr_r;  // Receive state. SN following last in-sequence received PDU. Low edge of rx window
  uint32_t vr_mr; // Max acceptable receive state. High edge of rx window. vr_r + window size.
  uint32_t vr_x;  // t_reordering state. SN following PDU which triggered t_reordering.
  uint32_t vr_ms; // Max status tx state. Highest possible value of SN for ACK_SN in status PDU.
  uint32_t vr_h;  // Highest rx state. SN following PDU with highest SN among rxed PDUs.

  /****************************************************************************
   * Timers
   * Ref: 3GPP TS 36.322 v10.0.0 Section 7
   ***************************************************************************/
  timeout poll_retx_timeout;
  timeout reordering_timeout;
  timeout status_prohibit_timeout;

  static const int reordering_timeout_id = 1;

  // Timer checks
  bool status_prohibited();
  bool poll_retx();

  // Helpers
  bool poll_required();

  int  prepare_status();
  int  build_status_pdu(uint8_t *payload, uint32_t nof_bytes);
  int  build_retx_pdu(uint8_t *payload, uint32_t nof_bytes);
  int  build_data_pdu(uint8_t *payload, uint32_t nof_bytes);

  void handle_data_pdu(uint8_t *payload, uint32_t nof_bytes);
  void handle_control_pdu(uint8_t *payload, uint32_t nof_bytes);

  void reassemble_rx_sdus();

  void debug_state();
};

/****************************************************************************
 * Header pack/unpack helper functions
 * Ref: 3GPP TS 36.322 v10.0.0 Section 6.2.1
 ***************************************************************************/
void        rlc_am_read_data_pdu_header(srsue_byte_buffer_t *pdu, rlc_amd_pdu_header_t *header);
void        rlc_am_read_data_pdu_header(uint8_t *payload, uint32_t nof_bytes, rlc_amd_pdu_header_t *header);
void        rlc_am_write_data_pdu_header(rlc_amd_pdu_header_t *header, srsue_byte_buffer_t *pdu);
void        rlc_am_read_status_pdu(srsue_byte_buffer_t *pdu, rlc_status_pdu_t *status);
void        rlc_am_read_status_pdu(uint8_t *payload, uint32_t nof_bytes, rlc_status_pdu_t *status);
void        rlc_am_write_status_pdu(rlc_status_pdu_t *status, srsue_byte_buffer_t *pdu );
int         rlc_am_write_status_pdu(rlc_status_pdu_t *status, uint8_t *payload);

uint32_t    rlc_am_packed_length(rlc_amd_pdu_header_t *header);
uint32_t    rlc_am_packed_length(rlc_status_pdu_t *status);
bool        rlc_am_is_control_pdu(srsue_byte_buffer_t *pdu);
bool        rlc_am_is_control_pdu(uint8_t *payload);
bool        rlc_am_status_has_nack(rlc_status_pdu_t *status, uint32_t sn);
std::string rlc_am_to_string(rlc_status_pdu_t *status);
bool        rlc_am_start_aligned(uint8_t fi);
bool        rlc_am_end_aligned(uint8_t fi);

} // namespace srsue


#endif // RLC_AM_H

/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 Software Radio Systems Limited
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

#ifndef UEPHYWORKERCOMMON_H
#define UEPHYWORKERCOMMON_H

#include <pthread.h>
#include <string.h>
#include <vector>
#include "srslte/srslte.h"
#include "common/mac_interface.h"
#include "radio/radio.h"
#include "common/log.h"
#include "phy/phy_params.h"

//#define CONTINUOUS_TX


namespace srsue {

struct phch_metrics_t
{
  float n;
  float sinr;
  float rsrp;
  float rsrq;
  float rssi;
  float turbo_iters;
  float dl_mcs;
};

struct phch_sync_metrics_t
{
  float cfo;
  float sfo;
};

/* Subclass that manages variables common to all workers */
  class phch_common {
  public:
    
    phch_common() {
      pathloss = 0; 
      cur_pathloss = 0; 
      rsrp_filtered = 0; 
      cur_pusch_power = 0; 
      p0_preamble = 0; 
      cur_radio_power = 0; 
      rx_gain_offset = 0;
      bzero(&metrics, sizeof(phch_metrics_t));
    }
    
    /* Common variables used by all phy workers */
    phy_params        *params_db; 
    srslte::log       *log_h;
    mac_interface_phy *mac;
    srslte_ue_ul_t     ue_ul; 
    
    /* Power control variables */
    float pathloss;
    float cur_pathloss;
    float p0_preamble;     
    float cur_radio_power; 
    float cur_pusch_power;
    float rsrp_filtered;
    float rx_gain_offset;

    phch_common(uint32_t nof_workers);
    void init(phy_params *_params, srslte::log *_log, srslte::radio *_radio, mac_interface_phy *_mac);
    
    /* For RNTI searches, -1 means now or forever */
    
    void               set_ul_rnti(srslte_rnti_type_t type, uint16_t rnti_value, int tti_start = -1, int tti_end = -1);
    uint16_t           get_ul_rnti(uint32_t tti);
    srslte_rnti_type_t get_ul_rnti_type();

    void               set_dl_rnti(srslte_rnti_type_t type, uint16_t rnti_value, int tti_start = -1, int tti_end = -1);
    uint16_t           get_dl_rnti(uint32_t tti);
    srslte_rnti_type_t get_dl_rnti_type();
    
    void set_rar_grant(uint32_t tti, uint8_t grant_payload[SRSLTE_RAR_GRANT_LEN]);
    bool get_pending_rar(uint32_t tti, srslte_dci_rar_grant_t *rar_grant = NULL);
    
    void reset_pending_ack(uint32_t tti);
    void set_pending_ack(uint32_t tti, uint32_t I_lowest, uint32_t n_dmrs);   
    bool get_pending_ack(uint32_t tti);    
    bool get_pending_ack(uint32_t tti, uint32_t *I_lowest, uint32_t *n_dmrs);
        
    void worker_end(uint32_t tti, bool tx_enable, cf_t *buffer, uint32_t nof_samples, srslte_timestamp_t tx_time);
    
    bool sr_enabled; 
    int  sr_last_tx_tti; 
   
    srslte::radio*    get_radio();

    void set_cell(const srslte_cell_t &c);
    uint32_t get_nof_prb();
    void set_metrics(const phch_metrics_t &m);
    void get_metrics(phch_metrics_t &m);
    void set_sync_metrics(const phch_sync_metrics_t &m);
    void get_sync_metrics(phch_sync_metrics_t &m);

    void reset_ul();
    
  private: 
    std::vector<pthread_mutex_t>    tx_mutex; 
    
    bool               is_first_of_burst;
    srslte::radio      *radio_h;
    float              cfo;
    
    
    bool               ul_rnti_active(uint32_t tti);
    bool               dl_rnti_active(uint32_t tti);
    uint16_t           ul_rnti, dl_rnti;  
    srslte_rnti_type_t ul_rnti_type, dl_rnti_type; 
    int                ul_rnti_start, ul_rnti_end, dl_rnti_start, dl_rnti_end; 
    
    float              time_adv_sec; 
    
    srslte_dci_rar_grant_t rar_grant; 
    bool                   rar_grant_pending; 
    uint32_t               rar_grant_tti; 
    
    typedef struct {
      bool enabled; 
      uint32_t I_lowest; 
      uint32_t n_dmrs;
    } pending_ack_t;
    pending_ack_t pending_ack[10];
    
    bool is_first_tx;

    uint32_t nof_workers;
    uint32_t nof_mutex;

    srslte_cell_t       cell;

    phch_metrics_t      metrics;
    phch_sync_metrics_t sync_metrics;
  };
  
} // namespace srsue

#endif // UEPHYWORKERCOMMON_H

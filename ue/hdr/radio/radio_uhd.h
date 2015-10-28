/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2015 The srsUE Developers. See the
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

#include "radio/radio.h"
#include "srslte/srslte.h"
#include "srslte/cuhd/cuhd.h"
#include "common/trace.h"

#ifndef RADIO_UHD_H
#define RADIO_UHD_H

// This is for the channel emulator
#include <fftw3.h>

namespace srslte {
  
/* Interface to the RF frontend. 
  */
  class radio_uhd : public radio
  {
    public: 
      radio_uhd() : tr_local_time(1024*10), tr_usrp_time(1024*10), tr_tx_time(1024*10), tr_is_eob(1024*10) {
        sf_len=0; 
        en_channel_emulator=false;        
      }
      
      bool init();
      bool init(char *args);
      bool init_agc();
      bool init_agc(char *args);

      void get_time(srslte_timestamp_t *now);
      bool tx(void *buffer, uint32_t nof_samples, srslte_timestamp_t tx_time);
      bool tx_end();
      bool rx_now(void *buffer, uint32_t nof_samples, srslte_timestamp_t *rxd_time);
      bool rx_at(void *buffer, uint32_t nof_samples, srslte_timestamp_t rx_time);

      void set_tx_gain(float gain);
      void set_rx_gain(float gain);
      void set_tx_rx_gain_offset(float offset); 
      double set_rx_gain_th(float gain);

      void set_tx_freq(float freq);
      void set_rx_freq(float freq);

      void set_master_clock_rate(float rate);
      void set_tx_srate(float srate);
      void set_rx_srate(float srate);

      float get_tx_gain();
      float get_rx_gain();
      
      float get_max_tx_power();
      float set_tx_power(float power);
      float get_rssi();
      bool  has_rssi();
      
      void start_trace();
      void write_trace(std::string filename);
      void start_rx();
      void stop_rx();
      
      void set_tti(uint32_t tti);
      void tx_offset(int offset);
      void set_tti_len(uint32_t sf_len);
      uint32_t get_tti_len();

      void register_msg_handler(cuhd_msg_handler_t h);
      
      bool channel_emulator_init(const char *filename, int Ntaps_, int Ncoeff_, int nsamples_, int ntti_); 
      
    private:
      
      void save_trace(uint32_t is_eob, srslte_timestamp_t *usrp_time);
      
      void *uhd; 
      
      static const double lo_offset = 0; // LO offset (in Hz)      
      static const double burst_settle_time = 0.4e-3; // Start of burst settle time (off->on RF transition time)      
      const static uint32_t burst_settle_max_samples = 30720000;  // 30.72 MHz is maximum frequency

      srslte_timestamp_t end_of_burst_time; 
      bool is_start_of_burst; 
      uint32_t burst_settle_samples; 
      double burst_settle_time_rounded; // settle time rounded to sample time
      cf_t zeros[burst_settle_max_samples]; 
      double cur_tx_srate;
      
      trace<uint32_t> tr_local_time;
      trace<uint32_t> tr_usrp_time;
      trace<uint32_t> tr_tx_time;
      trace<uint32_t> tr_is_eob;
      bool trace_enabled;
      uint32_t tti;
      bool agc_enabled;
      int offset;
      uint32_t sf_len;
      
      /* This is for the channel emulator */
      cf_t *in_ifft, *out_ifft, *taps; 
      fftwf_plan ifft_plan; 
      cf_t *temp, *temp_buffer_in, *temp_buffer_out, *read_buffer;
      int Ntaps, Ncoeff, nsamples, ntti, tti_cnt;
      bool en_channel_emulator;
      void channel_emulator(cf_t *input, cf_t *output); 
  }; 
}

#endif

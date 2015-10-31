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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <iostream>
#include <fstream>
#include <string>
#include <boost/program_options.hpp>
#include <boost/program_options/parsers.hpp>

#include "ue.h"

//TODO: Get version from cmake
#define VERSION "0.1.0"

using namespace std;
namespace bpo = boost::program_options;

static bool running = true;

void sig_int_handler(int signo)
{
  running = false;
}

/**********************************************************************
 *  Program arguments processing
 ***********************************************************************/
string config_file;

void parse_args(srsue::all_args_t *args, int argc, char* argv[]) {

    // Command line only options
    bpo::options_description general("General options");
    general.add_options()
        ("help,h", "Produce help message")
        ("version,v", "Print version information and exit")
        ;

    // Command line or config file options
    bpo::options_description common("Configuration options");
    common.add_options()
        ("usrp_args",         bpo::value<string>(&args->usrp_args),   "USRP args")
        ("rf.dl_freq",        bpo::value<float>(&args->rf.dl_freq)->default_value(2680000000),  "Downlink centre frequency")
        ("rf.ul_freq",        bpo::value<float>(&args->rf.ul_freq)->default_value(2560000000),  "Uplink centre frequency")
        ("rf.rx_gain",        bpo::value<float>(&args->rf.rx_gain)->default_value(-1),          "Front-end receiver gain")
        ("rf.tx_gain",        bpo::value<float>(&args->rf.tx_gain)->default_value(-1),          "Front-end transmitter gain")

        ("pcap.enable",       bpo::value<bool>(&args->pcap.enable)->default_value(false),           "Enable MAC packet captures for wireshark")
        ("pcap.filename",     bpo::value<string>(&args->pcap.filename)->default_value("ue.pcap"),   "MAC layer capture filename")

        ("trace.enable",      bpo::value<bool>(&args->trace.enable)->default_value(false),                  "Enable PHY and radio timing traces")
        ("trace.phy_filename",bpo::value<string>(&args->trace.phy_filename)->default_value("ue.phy_trace"), "PHY timing traces filename")
        ("trace.radio_filename",bpo::value<string>(&args->trace.radio_filename)->default_value("ue.radio_trace"), "Radio timing traces filename")

        ("gui.enable",        bpo::value<bool>(&args->gui.enable)->default_value(false),                  "Enable GUI plots")

        ("channel_emulator.enable",          bpo::value<bool>(&args->ch_emu.enable)->default_value(false),    "Enable fading channel emulator")
        ("channel_emulator.coeff_filename",  bpo::value<string>(&args->ch_emu.filename),                      "Pregenerated channel coefficients filename")
        ("channel_emulator.nof_paths",       bpo::value<int>(&args->ch_emu.nof_paths),                        "Number of pregenerated channel paths")
        ("channel_emulator.path_tap",        bpo::value<vector<int> >(&args->ch_emu.path_tap)->multitoken(),  "Tap position per path")
        ("channel_emulator.nof_coeffs",      bpo::value<int>(&args->ch_emu.nof_coeffs),                       "Number of pregenerated channel coefficients")
        ("channel_emulator.nof_samples",     bpo::value<int>(&args->ch_emu.nof_samples),                      "Number of samples per TTI (sampling rate/1000)")
        ("channel_emulator.nof_tti",         bpo::value<int>(&args->ch_emu.nof_tti),                          "Number of TTIs pregenerated in the file")
        
        ("log.phy_level",     bpo::value<string>(&args->log.phy_level),   "PHY log level")
        ("log.phy_hex_limit", bpo::value<int>(&args->log.phy_hex_limit),  "PHY log hex dump limit")
        ("log.mac_level",     bpo::value<string>(&args->log.mac_level),   "MAC log level")
        ("log.mac_hex_limit", bpo::value<int>(&args->log.mac_hex_limit),  "MAC log hex dump limit")
        ("log.rlc_level",     bpo::value<string>(&args->log.rlc_level),   "RLC log level")
        ("log.rlc_hex_limit", bpo::value<int>(&args->log.rlc_hex_limit),  "RLC log hex dump limit")
        ("log.pdcp_level",    bpo::value<string>(&args->log.pdcp_level),  "PDCP log level")
        ("log.pdcp_hex_limit",bpo::value<int>(&args->log.pdcp_hex_limit), "PDCP log hex dump limit")
        ("log.rrc_level",     bpo::value<string>(&args->log.rrc_level),   "RRC log level")
        ("log.rrc_hex_limit", bpo::value<int>(&args->log.rrc_hex_limit),  "RRC log hex dump limit")
        ("log.gw_level",      bpo::value<string>(&args->log.gw_level),    "GW log level")
        ("log.gw_hex_limit",  bpo::value<int>(&args->log.gw_hex_limit),   "GW log hex dump limit")
        ("log.nas_level",     bpo::value<string>(&args->log.nas_level),   "NAS log level")
        ("log.nas_hex_limit", bpo::value<int>(&args->log.nas_hex_limit),  "NAS log hex dump limit")
        ("log.usim_level",    bpo::value<string>(&args->log.usim_level),  "USIM log level")
        ("log.usim_hex_limit",bpo::value<int>(&args->log.usim_hex_limit), "USIM log hex dump limit")

        ("log.all_level",     bpo::value<string>(&args->log.all_level)->default_value("info"),   "ALL log level")
        ("log.all_hex_limit", bpo::value<int>(&args->log.all_hex_limit)->default_value(32),  "ALL log hex dump limit")

        ("log.filename",      bpo::value<string>(&args->log.filename)->default_value("/tmp/ue.log"),"Log filename")

        ("usim.imsi",         bpo::value<string>(&args->usim.imsi),        "USIM IMSI")
        ("usim.imei",         bpo::value<string>(&args->usim.imei),        "USIM IMEI")
        ("usim.k",            bpo::value<string>(&args->usim.k),           "USIM K")
        
        
        /* Expert section */
        ("expert.prach_gain", bpo::value<float>(&args->expert.prach_gain)->default_value(-1.0),  "Disable PRACH power control")
        ("expert.ul_gain", bpo::value<float>(&args->expert.ul_gain)->default_value(-1.0),  "Disable UL power control")
        
        ("expert.ul_pwr_ctrl_offset",     bpo::value<float>(&args->expert.ul_pwr_ctrl_offset)->default_value(0),     "UL power control offset")
        ("expert.rx_gain_offset",         bpo::value<float>(&args->expert.rx_gain_offset)->default_value(-1),     "RX gain offset")
        
        ("expert.pdsch_max_its",         bpo::value<int>(&args->expert.pdsch_max_its)->default_value(-1), "Maximum number of turbo decoder iterations")

        ("expert.sync_track_th",         bpo::value<float>(&args->expert.sync_track_th)->default_value(-1), "Synchronization track phase threshold")
        ("expert.sync_track_avg_coef",   bpo::value<float>(&args->expert.sync_track_avg_coef)->default_value(-1), "Synchronization track phase averaging factor")
        ("expert.sync_find_th",         bpo::value<float>(&args->expert.sync_find_th)->default_value(1.6), "Synchronization find phase threshold")
        ("expert.sync_find_max_frames",   bpo::value<float>(&args->expert.sync_find_max_frames)->default_value(100), "Synchronization find phase timeout")
                
        ("expert.nof_phy_threads",   bpo::value<int>(&args->expert.nof_phy_threads)->default_value(2), "Number of PHY threads")
        
    ;
    
    // Positional options - config file location
    bpo::options_description position("Positional options");
    position.add_options()
    ("config_file", bpo::value< string >(&config_file), "UE configuration file")
    ;
    bpo::positional_options_description p;
    p.add("config_file", -1);

    // these options are allowed on the command line
    bpo::options_description cmdline_options;
    cmdline_options.add(common).add(position).add(general);

    // parse the command line and store result in vm
    bpo::variables_map vm;
    bpo::store(bpo::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm);
    bpo::notify(vm);

    // help option was given - print usage and exit
    if (vm.count("help")) {
        cout << "Usage: " << argv[0] << " [OPTIONS] config_file" << endl << endl;
        cout << common << endl << general << endl;
        exit(0);
    }

    // print version number and exit
    if (vm.count("version")) {
        cout << "Version " << VERSION << endl;
        exit(0);
    }

    // no config file given - print usage and exit
    if (!vm.count("config_file")) {
        cout << "Error: Configuration file not provided" << endl;
        cout << "Usage: " << argv[0] << " [OPTIONS] config_file" << endl << endl;
        exit(0);
    } else {
        cout << "Reading configuration file " << config_file << "..." << endl;
        ifstream conf(config_file.c_str(), ios::in);
        if(conf.fail()) {
          cout << "Failed to read configuration file " << config_file << " - exiting" << endl;
          exit(1);
        }
        bpo::store(bpo::parse_config_file(conf, common), vm);
        bpo::notify(vm);
    }

    // Apply all_level to any unset layers
    if (vm.count("log.all_level")) {
      if(!vm.count("log.phy_level")) {
        args->log.phy_level = args->log.all_level;
      }
      if(!vm.count("log.mac_level")) {
        args->log.mac_level = args->log.all_level;
      }
      if(!vm.count("log.rlc_level")) {
        args->log.rlc_level = args->log.all_level;
      }
      if(!vm.count("log.pdcp_level")) {
        args->log.pdcp_level = args->log.all_level;
      }
      if(!vm.count("log.rrc_level")) {
        args->log.rrc_level = args->log.all_level;
      }
      if(!vm.count("log.nas_level")) {
        args->log.nas_level = args->log.all_level;
      }
      if(!vm.count("log.gw_level")) {
        args->log.gw_level = args->log.all_level;
      }
      if(!vm.count("log.usim_level")) {
        args->log.usim_level = args->log.all_level;
      }
    }

    // Apply all_hex_limit to any unset layers
    if (vm.count("log.all_hex_limit")) {
      if(!vm.count("log.phy_hex_limit")) {
        args->log.phy_hex_limit = args->log.all_hex_limit;
      }
      if(!vm.count("log.mac_hex_limit")) {
        args->log.mac_hex_limit = args->log.all_hex_limit;
      }
      if(!vm.count("log.rlc_hex_limit")) {
        args->log.rlc_hex_limit = args->log.all_hex_limit;
      }
      if(!vm.count("log.pdcp_hex_limit")) {
        args->log.pdcp_hex_limit = args->log.all_hex_limit;
      }
      if(!vm.count("log.rrc_hex_limit")) {
        args->log.rrc_hex_limit = args->log.all_hex_limit;
      }
      if(!vm.count("log.nas_hex_limit")) {
        args->log.nas_hex_limit = args->log.all_hex_limit;
      }
      if(!vm.count("log.gw_hex_limit")) {
        args->log.gw_hex_limit = args->log.all_hex_limit;
      }
      if(!vm.count("log.usim_hex_limit")) {
        args->log.usim_hex_limit = args->log.all_hex_limit;
      }
    }
    
    /* Make sure the number of path taps equal the number of paths */
    if (args->ch_emu.enable) {
      if (args->ch_emu.nof_paths != args->ch_emu.path_tap.size()) {
        cout << "Error: Number path_tap elements must be equal to nof_paths" << endl; 
        exit(0);
      }
    }


}


int main(int argc, char *argv[]) {

  signal(SIGINT, sig_int_handler);
  srsue::all_args_t args;

  cout << "---  Software Radio Systems LTE UE  ---" << endl << endl;
  parse_args(&args, argc, argv);

  srsue::ue ue(&args);
  if(!ue.init()) {
    exit(1);
  }

  bool plot_started   = false; 
  bool ch_emu_started = false; 
  while(running) {
    if (ue.is_attached()) {
      if (!plot_started && args.gui.enable) {
        ue.start_plot();
        plot_started = true; 
      }
      
      if (!ch_emu_started && args.ch_emu.enable) {
        ue.start_channel_emulator(args.ch_emu.filename.c_str(), args.ch_emu.path_tap.data(),
                                  args.ch_emu.nof_paths, args.ch_emu.nof_coeffs, 
                                  args.ch_emu.nof_samples, args.ch_emu.nof_tti);
        ch_emu_started = true; 
      }
    }
    sleep(1);
  }

  ue.stop();
  cout << "---  exiting  ---" << endl;
  exit(0);
}

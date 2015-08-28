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

#include <stdint.h>
#include <string>
#include <stdarg.h>
#include <stdio.h>

#include "common/log_stdout.h"


using namespace std; 

namespace srslte {

const char* level_str[4] =  {"[ERROR ",
                             "[WARN  ",
                             "[INFO  ",
                             "[DEBUG "};

void log_stdout::printlog(level_t type, uint32_t tti, string msg, va_list args) {

  printlog(type, tti, string(), -1, msg, args);
}
 
void log_stdout::printlog(level_t type, uint32_t tti, string file, int line, string msg, va_list args) {

  printf("%s %s",level_str[type], get_service_name().c_str());
  if (file.length() > 0) {
    printf("/%-14s", file.substr(file.find_last_of("/")+1,file.find_last_of(".")-1-file.find_last_of("/")).c_str());
  }
  if (line >= 0) {
    printf(" %5d]: ", tti);
  }
  vprintf(msg.c_str(), args);
}

void log_stdout::error(string msg, ...)
{
  va_list args;
  va_start(args, msg);
  printlog(ERROR, tti, msg, args);
  va_end(args);
}

void log_stdout::info(string msg, ...)
{
  if (level >= LOG_LEVEL_INFO) {
    va_list args;
    va_start(args, msg);
    printlog(INFO, tti, msg, args);
    va_end(args);    
  }
}

void log_stdout::debug(string msg, ...)
{
  if (level >= LOG_LEVEL_DEBUG) {
    va_list args;
    va_start(args, msg);
    printlog(DEBUG, tti, msg, args);
    va_end(args);
  }
}

void log_stdout::warning(string msg, ...)
{
  va_list args;
  va_start(args, msg);
  printlog(WARNING, tti, msg, args);
  va_end(args);
}


void log_stdout::error_line(string file, int line, string msg, ...)
{
  va_list args;
  va_start(args, msg);
  printlog(ERROR, tti, file, line, msg, args);
  va_end(args);
}

void log_stdout::info_line(string file, int line, string msg, ...)
{
  if (level >= LOG_LEVEL_INFO) {
    va_list args;
    va_start(args, msg);
    printlog(INFO, tti, file, line, msg, args);
    va_end(args);
  }
}

void log_stdout::debug_line(string file, int line, string msg, ...)
{
  if (level >= LOG_LEVEL_DEBUG) {
    va_list args;
    va_start(args, msg);
    printlog(DEBUG, tti, file, line, msg, args);
    va_end(args);
  }
}

void log_stdout::warning_line(string file, int line, string msg, ...)
{
  va_list args;
  va_start(args, msg);
  printlog(WARNING, tti, file, line, msg, args);
  va_end(args);
}

}

  
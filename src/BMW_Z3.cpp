/*
 * This file is part of the stm32-vcu project.
 *
 * Copyright (C) 2021 Damien Maguire
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "BMW_Z3.h"
#include "digio.h"
#include "errormessage.h"
#include "my_math.h"
#include "utils.h"

static uint8_t counter_329 = 0;
static uint8_t ABSMsg = 0x11;

/////////////////////////////////////////////////////////////////////////////////////////////////////

void BMW_Z3::Task10Ms() {
  Msg316();
  Msg329();
  Msg545();
}

bool BMW_Z3::Ready() { return DigIo::t15_digi.Get(); }

// these messages go out on vehicle can and drive the BMW Z3 instrument
// cluster. Same DME1/DME2/DME4 message set as the E36/E39/E46 MS41/MS43 DME.

//////////////////////DME Messages/////////////////////////////////////////
void BMW_Z3::Msg316() // DME1 - engine speed
{
  uint16_t canRPM;

  if (clusterStart) {
    // Sweeps the rev counter needle to full scale on power-up, mimicking the
    // real DME's start-up self test.
    canRPM = 44800;
    clusterStart = false;
  } else {
    // Limit tachometer range from 750 - 7000 RPM so the cluster always
    // thinks the "engine" is alive and within its normal range.
    uint16_t speed_input = MAX(750, speed);
    speed_input = MIN(7000, speed_input);

    // RPM * 6.4, 16 bits, Intel LSB (LSB,MSB)
    canRPM = (speed_input * 64) / 10;
  }

  uint8_t bytes[8];
  // Byte 0 - Status - 0x01 Terminal 15, 0x04 Traction Control OK, 0x08 gearbox type
  bytes[0] = 0x0D;
  bytes[1] = 0x00;
  bytes[2] = canRPM & 0xFF; // RPM LSB
  bytes[3] = canRPM >> 8;   // RPM MSB
  bytes[4] = 0x00;
  bytes[5] = 0x00;
  bytes[6] = 0x00;
  bytes[7] = 0x00;

  can->Send(0x316, bytes, 8); // Send on CAN2
}

void BMW_Z3::Msg329() // DME2 - ASC/DSC status
{
  // Byte 0 - Bits 6-7 Multiplexer ID, Bits 0-5 Data, cycles 0x11/0x86/0xD9
  // Byte 1 - Coolant Temp on a real DME, motor temp used here as the analog
  // Byte 2 - Atmospheric pressure (mbar), no sensor so fixed
  // Byte 3 - bit 3 Engine running
  // Byte 5 - Throttle position (00-FE)
  // Byte 6 - bit 0 Brake pedal depressed

  int throttlePct = Param::GetInt(Param::potnom);
  throttlePct = MAX(0, throttlePct);
  throttlePct = MIN(100, throttlePct);
  uint8_t accelPot = (throttlePct * 254) / 100;

  int tmpm = Param::GetInt(Param::tmpm);
  tmpm = MAX(0, tmpm);
  tmpm = MIN(150, tmpm);
  uint8_t motorTempByte = utils::change(tmpm, 0, 150, 90, 254);

  uint8_t byte3 = 0x00;
  if (Param::GetInt(Param::opmode) == MOD_RUN)
    byte3 |= 0x08; // engine run bit

  uint8_t bytes[8];
  bytes[0] = ABSMsg; // needs to cycle 11,86,d9
  bytes[1] = motorTempByte;
  bytes[2] = 0xc5;
  bytes[3] = byte3;
  bytes[4] = 0x00;
  bytes[5] = accelPot;                                // throttle position
  bytes[6] = Param::GetBool(Param::din_brake) ? 1 : 0; // brake pedal
  bytes[7] = 0x00;

  counter_329++;
  if (counter_329 >= 22)
    counter_329 = 0;
  if (counter_329 == 0)
    ABSMsg = 0x11;
  if (counter_329 == 8)
    ABSMsg = 0x86;
  if (counter_329 == 15)
    ABSMsg = 0xd9;

  can->Send(0x329, bytes, 8); // Send on CAN2
}

void BMW_Z3::Msg545() // DME4 - check engine / EML
{
  // Byte 0 - Bit 1 Check Engine, Bit 4 EML Light
  bool hasError = Param::GetInt(Param::status) != STAT_NONE ||
                  Param::GetInt(Param::lasterr) != ERROR_NONE;

  uint8_t bytes[8];
  bytes[0] = hasError ? 0x10 : 0x00; // EML light
  bytes[1] = 0x00;
  bytes[2] = 0x00;
  bytes[3] = 0x00;
  bytes[4] = 0x7E;
  bytes[5] = 0x0A;
  bytes[6] = 0x00;
  bytes[7] = 0x12;

  can->Send(0x545, bytes, 8); // Send on CAN2
}

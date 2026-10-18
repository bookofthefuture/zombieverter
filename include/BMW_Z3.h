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

#ifndef BMW_Z3_h
#define BMW_Z3_h

/*  This library supports the Powertrain CAN messages for the BMW Z3 dash
    cluster. Same DME1/DME2/DME4 message set (0x316/0x329/0x545) as the E36
    era BMW dash, driving revcounter, ASC/DSC status and malf lights.
*/

#include "vehicle.h"
#include <stdint.h>

class BMW_Z3 : public Vehicle {

public:
  void Task10Ms();
  void SetRevCounter(int s) { speed = s; }
  void SetTemperatureGauge(float) {
  } // motor temp is read from Param::tmpm directly in Msg329()
  bool Ready();

private:
  void Msg316();
  void Msg329();
  void Msg545();

  uint16_t speed;
  bool clusterStart =
      true; // sweeps rev counter to max on power-up, like the real DME
};

#endif /* BMW_Z3_h */

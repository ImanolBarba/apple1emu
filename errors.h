/***************************************************************************
 *   errors.h  --  This file is part of apple1emu.                         *
 *                                                                         *
 *   Copyright (C) 2021 Imanol-Mikel Barba Sabariego                       *
 *                                                                         *
 *   apple1emu is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License as published     *
 *   by the Free Software Foundation, either version 3 of the License,     *
 *   or (at your option) any later version.                                *
 *                                                                         *
 *   apple1emu is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty           *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.               *
 *   See the GNU General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.   *
 *                                                                         *
 ***************************************************************************/

#ifndef ERRORS_H
#define ERRORS_H

enum error {
  SUCCESS = 0,
  FAILURE = -1,
  ERROR_DATA_TOO_LARGE = -2,
  ERROR_INVALID_MEMORY_SETUP = -3,
  ERROR_MEMORY_ALLOC = -4,
  ERROR_TOO_MANY_CHIPS_ON_CLOCK = -5,
  ERROR_TOO_MUCH_USER_MEMORY = -6,
  ERROR_READ_FILE = -7,
  ERROR_OPEN_FILE = -8,
  ERROR_WRITE_FILE = -9,
  ERROR_INVALID_MEMORY_RANGE = -10,
  ERROR_PTHREAD_CREATE = -11,
  ERROR_PTHREAD_JOIN = -12,
  ERROR_PTHREAD_SIGNAL = -13
};

#endif

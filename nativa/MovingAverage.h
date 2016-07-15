/*
 * MovingAverage
 * Copyright (C) 2016 Nicola Corna <nicola@corna.info>
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
 *
 */

#ifndef MOVINGAVERAGE_H
#define MOVINGAVERAGE_H

#include <stdint.h>

#include "Filter.h"

template <typename T, uint8_t filterSize>
class MovingAverage : public Filter<T>
{
protected:
	T values[filterSize];
	uint8_t index;

public:
	inline MovingAverage (T startValue = T()) :
	    index(0)
	{
		for (uint8_t i = 0; i < filterSize; i++)
		    values[i] = startValue;
	}

	inline T filter (T value)
	{
	  T sum = 0;
	
		values[index] = value;
		index = (index + 1) % filterSize;

		for(uint8_t i = 0; i < filterSize; i++)
		    sum += values[i];

		return sum / filterSize;
	}
};

#endif

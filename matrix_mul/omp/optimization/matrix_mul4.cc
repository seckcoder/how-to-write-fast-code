/*

    Copyright (C) 2011  Abhinav Jauhri (abhinav.jauhri@gmail.com), Carnegie Mellon University - Silicon Valley 

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <omp.h>
#include <stdlib.h>
#include <memory.h>
#include "matrix_mul.h"

typedef unsigned int uint;

namespace omp
{
  void
  matrix_multiplication(
      float *sq_matrix_1,
      float *sq_matrix_2,
      float *sq_matrix_result,
      unsigned int sq_dimension ) {
    memset(sq_matrix_result, 0, sq_dimension*sq_dimension*sizeof(float));
    uint block_size = 64;
    for (uint kk = 0; kk < sq_dimension; kk+=block_size) {
      for (uint jj = 0; jj < sq_dimension; jj+=block_size) {
#pragma omp parallel for collapse(3)
        for (uint i = 0; i < sq_dimension; i++) {
          for (uint k = kk; k < std::min(sq_dimension, kk+block_size); k++) {
            for (uint j = jj; j < std::min(sq_dimension, jj+block_size); j++) {
#pragma omp atomic
              sq_matrix_result[sq_dimension*i+j] += sq_matrix_1[sq_dimension*i+k] * sq_matrix_2[sq_dimension*k+j];
            }
          }
        }
      }
    }
  }
} //namespace omp

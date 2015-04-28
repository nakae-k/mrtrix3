/*
    Copyright 2008 Brain Research Institute, Melbourne, Australia

    Written by J-Donald Tournier, 27/06/08.

    This file is part of MRtrix.

    MRtrix is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    MRtrix is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with MRtrix.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "command.h"
#include "progressbar.h"
#include "image.h"
#include "algo/threaded_copy.h"
#include "math/matrix.h"
#include "math/least_squares.h"
#include "dwi/gradient.h"


using namespace MR;
using namespace App;
using namespace std;


void usage ()
{
  DESCRIPTION 
    + "convert mean dwi (trace-weighted) images to mean adc maps";

  ARGUMENTS 
    + Argument ("input", "the input image.").type_image_in ()
    + Argument ("output", "the output image.").type_image_out ();

  OPTIONS 
    + DWI::GradImportOptions();
}



typedef float value_type;



class DWI2ADC {
  public:
    DWI2ADC (Image<value_type>& dwi_image, Image<value_type>& adc_image, const Math::Matrix<value_type>& binv, size_t dwi_axis) : 
      dwi_image (dwi_image), 
      adc_image (adc_image), 
      dwi (dwi_image.size (dwi_axis)), 
      adc (2), 
      binv (binv), 
      dwi_axis (dwi_axis) { }

    void operator() (const Iterator& pos) {
      assign_pos_of (pos).to (dwi_image, adc_image);
      for (auto l = Loop (dwi_axis, dwi_axis+1) (dwi_image); l; ++l) {
        value_type val = dwi_image.value();
        dwi[dwi_image.index(dwi_axis)] = val ? std::log (val) : 1.0e-12;
      }

      Math::mult (adc, binv, dwi);

      adc_image.index (3) = 0;
      adc_image.value() = std::exp (adc[0]);
      adc_image.index(3) = 1;
      adc_image.value() = adc[1];
    }

  protected:
    Image<value_type> dwi_image, adc_image;
    Math::Vector<value_type> dwi, adc;
    const Math::Matrix<value_type>& binv;
    const size_t dwi_axis;
};




void run () {
  auto dwi = Header::open (argument[0]).get_image<value_type>();
  auto grad = DWI::get_valid_DW_scheme<value_type> (dwi.header());

  size_t dwi_axis = 3;
  while (dwi.size (dwi_axis) < 2)
    ++dwi_axis;
  INFO ("assuming DW images are stored along axis " + str (dwi_axis));

  Math::Matrix<value_type> b (grad.rows(), 2);
  for (size_t i = 0; i < b.rows(); ++i) {
    b(i,0) = 1.0;
    b(i,1) = -grad (i,3);
  }

  auto binv = Math::pinv (b);

  auto header = dwi.header();
  header.datatype() = DataType::Float32;
  header.set_ndim (4);
  header.size(3) = 2;

  auto adc = Header::create (argument[1], header).get_image<value_type>();

  ThreadedLoop ("computing ADC values...", dwi, 0, 3)
    .run (DWI2ADC (dwi, adc, binv, dwi_axis));
}



// ======================================================================== //
// Copyright 2009-2012 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#include "distribution1d.h"
#include <algorithm>
#include <iostream>
#include <bonez/maths/maths.hpp>
#include <bonez/sys/threads.hpp>

namespace BnZ {

Sample1f sampleContinuousDistribution1D(const float* pCDF, size_t size, float s1D) {
    // coarse sampling of the distribution
    auto ptr = std::upper_bound(pCDF, pCDF + size, s1D);
    int i = clamp(int(ptr - pCDF - 1), 0, int(size) - 1);
    // refine sampling linearly by assuming the distribution being a step function
    float p = pCDF[i + 1] - pCDF[i];
    float fraction = (s1D - pCDF[i]) / p;
    return Sample1f(i + fraction, p * size);
}

Sample1u sampleDiscreteDistribution1D(const float* pCDF, size_t size, float s1D) {
    if(pCDF[size] == 0.f) {
        return Sample1u(0u, 0.f);
    }

    auto ptr = std::upper_bound(pCDF, pCDF + size, s1D);
    int i = clamp(int(ptr - pCDF - 1), 0, int(size) - 1);
    return Sample1u(i, pCDF[i + 1] - pCDF[i]);
}

float pdfContinuousDistribution1D(const float* pCDF, size_t size, float x) {
    auto i = clamp(int(x), 0, int(size) - 1);
    return (pCDF[i + 1] - pCDF[i]) * size;
}

float pdfDiscreteDistribution1D(const float* pCDF, uint32_t i) {
    return (pCDF[i + 1] - pCDF[i]);
}

float cdfDiscreteDistribution1D(const float* pCDF, uint32_t i) {
    return pCDF[i];
}

}


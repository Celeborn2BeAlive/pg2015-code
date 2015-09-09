/*
 * Copyright (C) 2012, Tomas Davidovic (http://www.davidovic.cz)
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * (The above is MIT License: http://en.wikipedia.org/wiki/MIT_License)
 */

#pragma once

#include <vector>
#include <cmath>
#include <bonez/maths/maths.hpp>

namespace BnZ {

class HashGrid
{
public:
    void Reserve(int aNumCells) {
        mCellEnds.resize(aNumCells);
    }

    // Fill the hash grid with the provided particles
    // The functions:
    // - Vec3f getPosition(const tParticle&);
    // - bool isValid(const tParticle&);
    // must be defined. Only particles that are valid are put in the grid
    template<typename tParticle>
    void build(
        const tParticle* aParticles,
        uint32_t count,
        float aRadius)
    {
        mRadius      = aRadius;
        mRadiusSqr   = sqr(mRadius);
        mCellSize    = mRadius * 2.f;
        mInvCellSize = 1.f / mCellSize;

        mBBoxMin = Vec3f( 1e36f);
        mBBoxMax = Vec3f(-1e36f);
        // Build bounding box of the particiles
        for(size_t i=0; i<count; i++)
        {
            if(isValid(aParticles[i])) {
                const Vec3f &pos = getPosition(aParticles[i]);
                for(int j=0; j<3; j++)
                {
                    mBBoxMax[j] = std::max(mBBoxMax[j], pos[j]);
                    mBBoxMin[j] = std::min(mBBoxMin[j], pos[j]);
                }
            }
        }
        auto center = (mBBoxMin + mBBoxMax) / 2.f;
        // For numerical stability at the border of the scene:
        mBBoxMin = center + 1.1f * (mBBoxMin - center);
        mBBoxMax = center + 1.1f * (mBBoxMax - center);

        mIndices.resize(count);
        memset(&mCellEnds[0], 0, mCellEnds.size() * sizeof(int));

        // set mCellEnds[x] to number of particles within x
        for(size_t i=0; i<count; i++)
        {
            if(isValid(aParticles[i])) {
                const Vec3f &pos = getPosition(aParticles[i]);
                mCellEnds[GetCellIndex(pos)]++;
            }
        }

        // run exclusive prefix sum to really get the cell starts
        // mCellEnds[x] is now where the cell starts
        int sum = 0;
        for(size_t i=0; i<mCellEnds.size(); i++)
        {
            int temp = mCellEnds[i];
            mCellEnds[i] = sum;
            sum += temp;
        }

        for(size_t i=0; i<count; i++)
        {
            if(isValid(aParticles[i])) {
                const Vec3f &pos = getPosition(aParticles[i]);
                const int targetIdx = mCellEnds[GetCellIndex(pos)]++;
                mIndices[targetIdx] = int(i);
            }
        }

        // now mCellEnds[x] points to the index right after the last
        // element of cell x

        //// DEBUG
        //for(size_t i=0; i<aParticles.size(); i++)
        //{
        //    const Vec3f &pos  = aParticles[i].GetPosition();
        //    Vec2i range = GetCellRange(GetCellIndex(pos));
        //    bool found = false;
        //    for(;range.x < range.y; range.x++)
        //    {
        //        if(mIndices[range.x] == i)
        //            found = true;
        //    }
        //    if(!found)
        //        printf("Error at particle %d\n", i);
        //}
    }

    // Apply the function aFunc on each particle located in the ball of radius aRadius
    // around the queried position
    template<typename tParticle, typename tFunc>
    int process(
        const tParticle* aParticles,
        const Vec3f& queryPos,
        const tFunc& aFunc) const
    {
        const Vec3f distMin = queryPos - mBBoxMin;
        const Vec3f distMax = mBBoxMax - queryPos;
        for(int i=0; i<3; i++)
        {
            if(distMin[i] < 0.f || distMax[i] < 0.f) {
                return -1;
            }
        }

        const Vec3f cellPt = mInvCellSize * distMin;
        const Vec3f coordF(
            std::floor(cellPt.x),
            std::floor(cellPt.y),
            std::floor(cellPt.z));

        const int  px = int(coordF.x);
        const int  py = int(coordF.y);
        const int  pz = int(coordF.z);

        const Vec3f fractCoord = cellPt - coordF;

        const int  pxo = px + (fractCoord.x < 0.5f ? -1 : +1);
        const int  pyo = py + (fractCoord.y < 0.5f ? -1 : +1);
        const int  pzo = pz + (fractCoord.z < 0.5f ? -1 : +1);

        int found = 0;

        for(int j=0; j<8; j++)
        {
            Vec2i activeRange;
            switch(j)
            {
            case 0: activeRange = GetCellRange(GetCellIndex(Vec3i(px , py , pz ))); break;
            case 1: activeRange = GetCellRange(GetCellIndex(Vec3i(px , py , pzo))); break;
            case 2: activeRange = GetCellRange(GetCellIndex(Vec3i(px , pyo, pz ))); break;
            case 3: activeRange = GetCellRange(GetCellIndex(Vec3i(px , pyo, pzo))); break;
            case 4: activeRange = GetCellRange(GetCellIndex(Vec3i(pxo, py , pz ))); break;
            case 5: activeRange = GetCellRange(GetCellIndex(Vec3i(pxo, py , pzo))); break;
            case 6: activeRange = GetCellRange(GetCellIndex(Vec3i(pxo, pyo, pz ))); break;
            case 7: activeRange = GetCellRange(GetCellIndex(Vec3i(pxo, pyo, pzo))); break;
            }

            for(; activeRange.x < activeRange.y; activeRange.x++)
            {
                const int particleIndex   = mIndices[activeRange.x];
                const tParticle &particle = aParticles[particleIndex];

                const float distSqr =
                    lengthSquared(queryPos - getPosition(particle));

                if(distSqr <= mRadiusSqr) {
                    aFunc(particle);
                    ++found;
                }
            }
        }
        return found;
    }

private:

    Vec2i GetCellRange(int aCellIndex) const
    {
        if(aCellIndex == 0) return Vec2i(0, mCellEnds[0]);
        return Vec2i(mCellEnds[aCellIndex-1], mCellEnds[aCellIndex]);
    }

    int GetCellIndex(const Vec3i &aCoord) const
    {
        uint32_t x = uint32_t(aCoord.x);
        uint32_t y = uint32_t(aCoord.y);
        uint32_t z = uint32_t(aCoord.z);

        return int(((x * 73856093) ^ (y * 19349663) ^
            (z * 83492791)) % uint32_t(mCellEnds.size()));
    }

    int GetCellIndex(const Vec3f &aPoint) const
    {
        const Vec3f distMin = aPoint - mBBoxMin;

        const Vec3f coordF(
            std::floor(mInvCellSize * distMin.x),
            std::floor(mInvCellSize * distMin.y),
            std::floor(mInvCellSize * distMin.z));

        const Vec3i coordI  = Vec3i(int(coordF.x), int(coordF.y), int(coordF.z));

        return GetCellIndex(coordI);
    }

private:

    Vec3f mBBoxMin;
    Vec3f mBBoxMax;
    std::vector<int> mIndices;
    std::vector<int> mCellEnds;

    float mRadius;
    float mRadiusSqr;
    float mCellSize;
    float mInvCellSize;
};

}

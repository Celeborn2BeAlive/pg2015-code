#pragma once

#include <bonez/opengl/opengl.hpp>
#include <bonez/maths/glm.hpp>
#include <bonez/utils/Grid3D.hpp>
#include <bonez/scene/topology/CurvilinearSkeleton.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace BnZ {

struct CC3DFaceBits {
    enum Value {
        NOFACE = 0,
        POINT = 1 << 0,
        XEDGE = 1 << 1,
        YEDGE = 1 << 2,
        ZEDGE = 1 << 3,
        XYFACE = 1 << 4,
        YZFACE = 1 << 5,
        XZFACE = 1 << 6,
        CUBE = 1 << 7,
        ALL = POINT | XEDGE | YEDGE | ZEDGE | XYFACE | YZFACE | XZFACE | CUBE
    };
};

inline bool getFaceVectors(CC3DFaceBits::Value face, Vec3i& u, Vec3i& v) {
    switch (face) {
    case CC3DFaceBits::XYFACE:
        u = Vec3i(1, 0, 0);
        v = Vec3i(0, 1, 0);
        return true;
    case CC3DFaceBits::YZFACE:
        u = Vec3i(0, 1, 0);
        v = Vec3i(0, 0, 1);
        return true;
    case CC3DFaceBits::XZFACE:
        u = Vec3i(0, 0, 1);
        v = Vec3i(1, 0, 0);
        return true;
    default:
        break;
    }

    return false;
}

class CC3DFace {
    int8_t m_Bitfield;
public:
    CC3DFace(): CC3DFace(CC3DFaceBits::NOFACE) {}

    explicit CC3DFace(int8_t bits): m_Bitfield(bits) {}

    operator int8_t() const {
        return m_Bitfield;
    }

    void fill() {
        m_Bitfield = CC3DFaceBits::ALL;
    }

    void add(int8_t bits) {
        m_Bitfield |= bits;
    }

    void remove(int8_t bits) {
        m_Bitfield &= ~bits;
    }

    // Returns true if the face contains one element of the bitfield
    bool containsSome(int8_t bits) const {
        return m_Bitfield & bits;
    }

    // Returns true if the face contains each element of the bitfield
    bool containsAll(int8_t bits) const {
        return (m_Bitfield & bits) == bits;
    }

    bool exists() const {
        return m_Bitfield != int8_t(CC3DFaceBits::NOFACE);
    }
};

class CubicalComplex3D: Grid3D<CC3DFace> {
    using Base = Grid3D<CC3DFace>;
public:
    using value_type = Base::value_type;
    using reference = Base::reference;
    using const_reference = Base::const_reference;
    using difference_type = Base::difference_type;
    using size_type = Base::size_type;
    using iterator = Base::iterator;
    using const_iterator = Base::const_iterator;
    using Base::data;
    using Base::size;
    using Base::empty;
    using Base::begin;
    using Base::end;
    using Base::operator [];
    using Base::operator ();
    using Base::width;
    using Base::height;
    using Base::depth;
    using Base::contains;
    using Base::resolution;
    using Base::forEach;

    CubicalComplex3D(){}

    explicit CubicalComplex3D(uint32_t width, uint32_t height, uint32_t depth):
        Base(width + 1, height + 1, depth + 1, value_type())
    {
    }

    explicit CubicalComplex3D(const Vec3u& resolution): CubicalComplex3D(resolution.x, resolution.y, resolution.z) {
    }

    uint32_t recursiveDepthFirstTraversal(int x, int y, int z, std::vector<bool>& visited) const;

    uint32_t computeConnexityNumber() const;

    int computeEulerCharacteristic() const;

    static bool isInObject(int x, int y, int z, const CubicalComplex3D& cc) {
        return cc(x, y, z).containsSome(CC3DFaceBits::CUBE) || (cc(x, y, z).exists() && (x == int(cc.width() - 1)
                || y == int(cc.height() - 1) || z == int(cc.depth() - 1)));
    }

    static bool isInComplementary(int x, int y, int z, const CubicalComplex3D& cc) {
        return !isInObject(x, y, z, cc);
    }

    bool containsSurfacicPoint(int x, int y, int z) const {
        auto face = (*this)(x, y, z);
        return face.containsSome(CC3DFaceBits::POINT) && (face.containsSome(CC3DFaceBits::XYFACE | CC3DFaceBits::XZFACE | CC3DFaceBits::YZFACE) ||
                (x > 0 && (*this)(x - 1, y, z).containsSome(CC3DFaceBits::XYFACE | CC3DFaceBits::XZFACE)) ||
                (z > 0 && (*this)(x, y, z - 1).containsSome(CC3DFaceBits::YZFACE | CC3DFaceBits::XZFACE)) ||
                (y > 0 && (*this)(x, y - 1, z).containsSome(CC3DFaceBits::XYFACE | CC3DFaceBits::YZFACE)) ||
                (x > 0 && y > 0 && (*this)(x - 1, y - 1, z).containsSome(CC3DFaceBits::XYFACE)) ||
                (z > 0 && y > 0 && (*this)(x, y - 1, z - 1).containsSome(CC3DFaceBits::YZFACE)));
    }

    bool isEdgeXCurvilinear(int x, int y, int z) const {
        auto face = (*this)(x, y, z);
        return face.containsSome(CC3DFaceBits::XEDGE) && !face.containsSome(CC3DFaceBits::XYFACE | CC3DFaceBits::XZFACE)
                && (z == 0 || !(*this)(x, y, z - 1).containsSome(CC3DFaceBits::XZFACE))
                && (y == 0 || !(*this)(x, y - 1, z).containsSome(CC3DFaceBits::XYFACE));
    }

    bool isEdgeYCurvilinear(int x, int y, int z) const {
        auto face = (*this)(x, y, z);
        return face.containsSome(CC3DFaceBits::YEDGE) && !face.containsSome(CC3DFaceBits::XYFACE | CC3DFaceBits::YZFACE)
                && (x == 0 || !(*this)(x - 1, y, z).containsSome(CC3DFaceBits::XYFACE))
                && (z == 0 || !(*this)(x, y, z - 1).containsSome(CC3DFaceBits::YZFACE));
    }

    bool isEdgeZCurvilinear(int x, int y, int z) const {
        auto face = (*this)(x, y, z);
        return face.containsSome(CC3DFaceBits::ZEDGE) && !face.containsSome(CC3DFaceBits::XZFACE | CC3DFaceBits::YZFACE)
                && (x == 0 || !(*this)(x - 1, y, z).containsSome(CC3DFaceBits::XZFACE))
                && (y == 0 || !(*this)(x, y - 1, z).containsSome(CC3DFaceBits::YZFACE));
    }

    bool isIsolatedPoint(int x, int y, int z) const {
        auto face = (*this)(x, y, z);
        return face.containsSome(CC3DFaceBits::POINT) && !face.containsSome(CC3DFaceBits::XEDGE | CC3DFaceBits::YEDGE | CC3DFaceBits::ZEDGE)
                && (x == 0 || !(*this)(x - 1, y, z).containsSome(CC3DFaceBits::XEDGE))
                && (y == 0 || !(*this)(x, y - 1, z).containsSome(CC3DFaceBits::YEDGE))
                && (z == 0 || !(*this)(x, y, z - 1).containsSome(CC3DFaceBits::ZEDGE));
    }

    bool isFreeXNEG(int x, int y, int z) const;
    bool isFreeXNEG3D(int x, int y, int z) const;
    bool isFreeXNEG2D(int x, int y, int z) const;
    bool isFreeXNEG2DXY(int x, int y, int z) const;
    bool isFreeXNEG2DXZ(int x, int y, int z) const;
    bool isFreeXNEG1D(int x, int y, int z) const;

    bool isFreeXPOS(int x, int y, int z) const;
    bool isFreeXPOS3D(int x, int y, int z) const;
    bool isFreeXPOS2D(int x, int y, int z) const;
    bool isFreeXPOS2DXY(int x, int y, int z) const;
    bool isFreeXPOS2DXZ(int x, int y, int z) const;
    bool isFreeXPOS1D(int x, int y, int z) const;

    bool isFreeYNEG(int x, int y, int z) const;
    bool isFreeYNEG3D(int x, int y, int z) const;
    bool isFreeYNEG2D(int x, int y, int z) const;
    bool isFreeYNEG2DXY(int x, int y, int z) const;
    bool isFreeYNEG2DYZ(int x, int y, int z) const;
    bool isFreeYNEG1D(int x, int y, int z) const;

    bool isFreeYPOS(int x, int y, int z) const;
    bool isFreeYPOS3D(int x, int y, int z) const;
    bool isFreeYPOS2D(int x, int y, int z) const;
    bool isFreeYPOS2DXY(int x, int y, int z) const;
    bool isFreeYPOS2DYZ(int x, int y, int z) const;
    bool isFreeYPOS1D(int x, int y, int z) const;

    bool isFreeZNEG(int x, int y, int z) const;
    bool isFreeZNEG3D(int x, int y, int z) const;
    bool isFreeZNEG2D(int x, int y, int z) const;
    bool isFreeZNEG2DXZ(int x, int y, int z) const;
    bool isFreeZNEG2DYZ(int x, int y, int z) const;
    bool isFreeZNEG1D(int x, int y, int z) const;

    bool isFreeZPOS(int x, int y, int z) const;
    bool isFreeZPOS3D(int x, int y, int z) const;
    bool isFreeZPOS2D(int x, int y, int z) const;
    bool isFreeZPOS2DXZ(int x, int y, int z) const;
    bool isFreeZPOS2DYZ(int x, int y, int z) const;
    bool isFreeZPOS1D(int x, int y, int z) const;
};

CurvilinearSkeleton getCurvilinearSkeleton(const CubicalComplex3D& skeletonCC,
                                           const CubicalComplex3D& originalObjectCC,
                                           const Grid3D<uint32_t> distance26Map,
                                           const Grid3D<uint32_t> opening26Map,
                                           const Mat4f& gridToWorld);

CurvilinearSkeleton getSegmentedCurvilinearSkeleton(const CubicalComplex3D& skeletonCC,
                                                    const CubicalComplex3D& originalObjectCC,
                                                    const Grid3D<uint32_t> distance26Map,
                                                    const Grid3D<uint32_t> opening26Map,
                                                    const Mat4f& gridToWorld);

CubicalComplex3D getCubicalComplex(const VoxelGrid& voxelGrid, bool getComplementary = false);


CubicalComplex3D filterSurfacicParts(const CubicalComplex3D& skeletonCC);

CubicalComplex3D filterIsolatedPoints(const CubicalComplex3D& skeletonCC);

enum class CC3DDirection {
    X = 0, Y = 1, Z = 2
};

enum class CC3DOrientation {
    POSITIVE = 0, NEGATIVE = 1
};

enum class CC3DFaceElement {
    CUBE, XYFACE, YZFACE, XZFACE, XEDGE, YEDGE, ZEDGE
};

template<CC3DDirection Dir, CC3DOrientation Orient, CC3DFaceElement Face>
struct CC3DFreePairHelper;

// Positive X
template<>
struct CC3DFreePairHelper<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::CUBE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeXPOS3D(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::CUBE | CC3DFaceBits::YZFACE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::XZFACE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeXPOS2DXZ(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::XZFACE | CC3DFaceBits::ZEDGE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::XYFACE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeXPOS2DXY(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::XYFACE | CC3DFaceBits::YEDGE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::X, CC3DOrientation::POSITIVE, CC3DFaceElement::XEDGE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeXPOS1D(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::XEDGE | CC3DFaceBits::POINT);
    }
};

// Positive Y
template<>
struct CC3DFreePairHelper<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::CUBE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeYPOS3D(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::CUBE | CC3DFaceBits::XZFACE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::YZFACE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeYPOS2DYZ(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::YZFACE | CC3DFaceBits::ZEDGE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::XYFACE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeYPOS2DXY(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::XYFACE | CC3DFaceBits::XEDGE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::Y, CC3DOrientation::POSITIVE, CC3DFaceElement::YEDGE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeYPOS1D(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::YEDGE | CC3DFaceBits::POINT);
    }
};

// Positive Z
template<>
struct CC3DFreePairHelper<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::CUBE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeZPOS3D(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::CUBE | CC3DFaceBits::XYFACE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::YZFACE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeZPOS2DYZ(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::YZFACE | CC3DFaceBits::YEDGE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::XZFACE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeZPOS2DXZ(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::XZFACE | CC3DFaceBits::XEDGE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::Z, CC3DOrientation::POSITIVE, CC3DFaceElement::ZEDGE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeZPOS1D(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::ZEDGE | CC3DFaceBits::POINT);
    }
};

// Negative X
template<>
struct CC3DFreePairHelper<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::CUBE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeXNEG3D(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::YZFACE);
        f(Vec3i(v.x - 1, v.y, v.z), CC3DFaceBits::CUBE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::XZFACE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeXNEG2DXZ(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::ZEDGE);
        f(Vec3i(v.x - 1, v.y, v.z), CC3DFaceBits::XZFACE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::XYFACE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeXNEG2DXY(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::YEDGE);
        f(Vec3i(v.x - 1, v.y, v.z), CC3DFaceBits::XYFACE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::X, CC3DOrientation::NEGATIVE, CC3DFaceElement::XEDGE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeXNEG1D(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::POINT);
        f(Vec3i(v.x - 1, v.y, v.z), CC3DFaceBits::XEDGE);
    }
};

// Negative Y
template<>
struct CC3DFreePairHelper<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::CUBE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeYNEG3D(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::XZFACE);
        f(Vec3i(v.x, v.y - 1, v.z), CC3DFaceBits::CUBE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::YZFACE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeYNEG2DYZ(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::ZEDGE);
        f(Vec3i(v.x, v.y - 1, v.z), CC3DFaceBits::YZFACE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::XYFACE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeYNEG2DXY(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::XEDGE);
        f(Vec3i(v.x, v.y - 1, v.z), CC3DFaceBits::XYFACE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::Y, CC3DOrientation::NEGATIVE, CC3DFaceElement::YEDGE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeYNEG1D(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::POINT);
        f(Vec3i(v.x, v.y - 1, v.z), CC3DFaceBits::YEDGE);
    }
};

// Negative Z
template<>
struct CC3DFreePairHelper<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::CUBE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeZNEG3D(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::XYFACE);
        f(Vec3i(v.x, v.y, v.z - 1), CC3DFaceBits::CUBE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::YZFACE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeZNEG2DYZ(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::YEDGE);
        f(Vec3i(v.x, v.y, v.z - 1), CC3DFaceBits::YZFACE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::XZFACE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeZNEG2DXZ(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::XEDGE);
        f(Vec3i(v.x, v.y, v.z - 1), CC3DFaceBits::XZFACE);
    }
};

template<>
struct CC3DFreePairHelper<CC3DDirection::Z, CC3DOrientation::NEGATIVE, CC3DFaceElement::ZEDGE> {
    static bool isFree(const CubicalComplex3D& cc, const Vec3i& v) {
        return cc.isFreeZNEG1D(v.x, v.y, v.z);
    }

    template<typename Functor>
    static void forEach(const Vec3i& v, Functor f) {
        f(v, CC3DFaceBits::POINT);
        f(Vec3i(v.x, v.y, v.z - 1), CC3DFaceBits::ZEDGE);
    }
};

}

#pragma once

#include "maths.hpp"

namespace BnZ {

template<typename T>
struct BBox
{
    static const auto dim = VecTraits<T>::dim;
    T lower, upper;

    BBox           ( ): lower(std::numeric_limits<T>::max()), upper(std::numeric_limits<T>::lowest()) { }
    BBox           ( const BBox& other ): lower(other.lower), upper(other.upper) { }
    BBox& operator=( const BBox& other ) { lower = other.lower; upper = other.upper; return *this; }

    BBox ( const T& v                 ) : lower(v),   upper(v) {}
    BBox ( const T& lower, const T& upper ) : lower(lower), upper(upper) {}

    void grow(const BBox& other) { lower = min(lower,other.lower); upper = max(upper,other.upper); }
    void grow(const T   & other) { lower = min(lower,other    ); upper = max(upper,other    ); }

    bool empty() const { for (auto i = 0u; i < dim; i++) if (lower[i] > upper[i]) return true; return false; }

    T size() const { return upper - lower; }
};

/*! tests if box is empty */
template<typename T> inline bool isEmpty(const BBox<T>& box) { return box.empty(); }

/*! computes the center of the box */
template<typename T> inline const T center (const BBox<T>& box) { return T(.5f)*(box.lower + box.upper); }
template<typename T> inline const T center2(const BBox<T>& box) { return box.lower + box.upper; }

/*! computes the size of the box */
template<typename T> inline const T size(const BBox<T>& box) { return box.size(); }

///*! computes the volume of a bounding box */
//template<typename T> inline const T volume( const BBox<T>& b ) { return reduce_mul(size(b)); }

///*! computes the surface area of a bounding box */
//template<typename T> inline const T     area( const BBox<Vec2<T> >& b ) { const auto d = size(b); return d.x*d.y; }
//template<typename T> inline const T     area( const BBox<Vec3<T> >& b ) { const auto d = size(b); return T(2)*(d.x*(d.y+d.z)+d.y*d.z); }
//template<typename T> inline const T halfArea( const BBox<Vec3<T> >& b ) { const auto d = size(b); return d.x*(d.y+d.z)+d.y*d.z; }

/*! merges bounding boxes and points */
template<typename T> inline const BBox<T> merge( const BBox<T>& a, const       T& b ) { return BBox<T>(min(a.lower, b    ), max(a.upper, b    )); }
template<typename T> inline const BBox<T> merge( const       T& a, const BBox<T>& b ) { return BBox<T>(min(a    , b.lower), max(a    , b.upper)); }
template<typename T> inline const BBox<T> merge( const BBox<T>& a, const BBox<T>& b ) { return BBox<T>(min(a.lower, b.lower), max(a.upper, b.upper)); }
template<typename T> inline const BBox<T> merge( const BBox<T>& a, const BBox<T>& b, const BBox<T>& c ) { return merge(a,merge(b,c)); }
template<typename T> inline const BBox<T>& operator+=( BBox<T>& a, const BBox<T>& b ) { return a = merge(a,b); }
template<typename T> inline const BBox<T>& operator+=( BBox<T>& a, const       T& b ) { return a = merge(a,b); }

/*! Merges four boxes. */
template<typename T> inline BBox<T> merge(const BBox<T>& a, const BBox<T>& b, const BBox<T>& c, const BBox<T>& d) {
  return merge(merge(a,b),merge(c,d));
}

/*! Merges eight boxes. */
template<typename T> inline BBox<T> merge(const BBox<T>& a, const BBox<T>& b, const BBox<T>& c, const BBox<T>& d,
                                                 const BBox<T>& e, const BBox<T>& f, const BBox<T>& g, const BBox<T>& h) {
  return merge(merge(a,b,c,d),merge(e,f,g,h));
}

/*! Comparison Operators */
template<typename T> inline bool operator==( const BBox<T>& a, const BBox<T>& b ) { return a.lower == b.lower && a.upper == b.upper; }
template<typename T> inline bool operator!=( const BBox<T>& a, const BBox<T>& b ) { return a.lower != b.lower || a.upper != b.upper; }

/*! scaling */
template<typename T> inline BBox<T> operator *( const float& a, const BBox<T>& b ) { return BBox<T>(a*b.lower,a*b.upper); }

/*! intersect bounding boxes */
template<typename T> inline const BBox<T> intersect( const BBox<T>& a, const BBox<T>& b ) { return BBox<T>(max(a.lower, b.lower), min(a.upper, b.upper)); }
template<typename T> inline const BBox<T> intersect( const BBox<T>& a, const BBox<T>& b, const BBox<T>& c ) { return intersect(a,intersect(b,c)); }

/*! tests if bounding boxes (and points) are disjoint (empty intersection) */
template<typename T> inline bool disjoint( const BBox<T>& a, const BBox<T>& b )
{ const T d = min(a.upper, b.upper) - max(a.lower, b.lower); for ( size_t i = 0 ; i < BBox<T>::dim ; i++ ) if ( d[i] < typename VecTraits<T>::Scalar(0) ) return true; return false; }
template<typename T> inline bool disjoint( const BBox<T>& a, const  T& b )
{ const T d = min(a.upper, b)       - max(a.lower, b);       for ( size_t i = 0 ; i < BBox<T>::dim ; i++ ) if ( d[i] < typename VecTraits<T>::Scalar(0) ) return true; return false; }
template<typename T> inline bool disjoint( const  T& a, const BBox<T>& b )
{ const T d = min(a, b.upper)       - max(a, b.lower);       for ( size_t i = 0 ; i < BBox<T>::dim ; i++ ) if ( d[i] < typename VecTraits<T>::Scalar(0) ) return true; return false; }

/*! tests if bounding boxes (and points) are conjoint (non-empty intersection) */
template<typename T> inline bool conjoint( const BBox<T>& a, const BBox<T>& b )
{ const T d = min(a.upper, b.upper) - max(a.lower, b.lower); for ( size_t i = 0 ; i < BBox<T>::dim ; i++ ) if ( d[i] < typename VecTraits<T>::Scalar(0) ) return false; return true; }
template<typename T> inline bool conjoint( const BBox<T>& a, const  T& b )
{ const T d = min(a.upper, b)       - max(a.lower, b);       for ( size_t i = 0 ; i < BBox<T>::dim ; i++ ) if ( d[i] < typename VecTraits<T>::Scalar(0) ) return false; return true; }
template<typename T> inline bool conjoint( const  T& a, const BBox<T>& b )
{ const T d = min(a, b.upper)       - max(a, b.lower);       for ( size_t i = 0 ; i < BBox<T>::dim ; i++ ) if ( d[i] < typename VecTraits<T>::Scalar(0) ) return false; return true; }

/*! subset relation */
template<typename T> inline bool subset( const BBox<T>& a, const BBox<T>& b )
{
  for ( size_t i = 0 ; i < BBox<T>::dim ; i++ ) if ( a.lower[i]*1.00001f < b.lower[i] ) return false;
  for ( size_t i = 0 ; i < BBox<T>::dim ; i++ ) if ( a.upper[i] > b.upper[i]*1.00001f ) return false;
  return true;
}

/*! output operator */
template<typename T> inline std::ostream& operator<<(std::ostream& cout, const BBox<T>& box) {
  return cout << "[" << box.lower << "; " << box.upper << "]";
}

template<typename T>
inline void boundingSphere(const BBox<T>& bbox, T& c,
                           typename VecTraits<T>::Scalar& radius) {
    c = center(bbox);
    radius = length(size(bbox)) * 0.5f;
}

/*! default template instantiations */
typedef BBox<Vec2f> BBox2f;
typedef BBox<Vec3f> BBox3f;

typedef BBox<Vec2i> BBox2i;
typedef BBox<Vec3i> BBox3i;

}

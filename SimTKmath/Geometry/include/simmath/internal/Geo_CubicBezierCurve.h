#ifndef SimTK_SIMMATH_GEO_CUBIC_BEZIER_CURVE_H_
#define SimTK_SIMMATH_GEO_CUBIC_BEZIER_CURVE_H_

/* -------------------------------------------------------------------------- *
 *                        SimTK Simbody: SimTKmath                            *
 * -------------------------------------------------------------------------- *
 * This is part of the SimTK biosimulation toolkit originating from           *
 * Simbios, the NIH National Center for Physics-Based Simulation of           *
 * Biological Structures at Stanford, funded under the NIH Roadmap for        *
 * Medical Research, grant U54 GM072970. See https://simtk.org.               *
 *                                                                            *
 * Portions copyright (c) 2011 Stanford University and the Authors.           *
 * Authors: Michael Sherman                                                   *
 * Contributors:                                                              *
 *                                                                            *
 * Permission is hereby granted, free of charge, to any person obtaining a    *
 * copy of this software and associated documentation files (the "Software"), *
 * to deal in the Software without restriction, including without limitation  *
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,   *
 * and/or sell copies of the Software, and to permit persons to whom the      *
 * Software is furnished to do so, subject to the following conditions:       *
 *                                                                            *
 * The above copyright notice and this permission notice shall be included in *
 * all copies or substantial portions of the Software.                        *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *
 * THE AUTHORS, CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,    *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR      *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE  *
 * USE OR OTHER DEALINGS IN THE SOFTWARE.                                     *
 * -------------------------------------------------------------------------- */

/** @file
Provides primitive operations for a single bicubic Bezier curve using either
single or double precision. **/

#include "SimTKcommon.h"
#include "simmath/internal/common.h"
#include "simmath/internal/Geo.h"
#include "simmath/internal/Geo_Point.h"
#include "simmath/internal/Geo_Sphere.h"
#include "simmath/internal/Geo_Box.h"

#include <cassert>
#include <cmath>
#include <algorithm>

namespace SimTK {


//==============================================================================
//                          GEO CUBIC BEZIER CURVE
//==============================================================================
/** This is a primitive useful for computations involving a single cubic Bezier
curve segment. Objects of this class contain the Bezier control points, but 
these can easily be converted to algebraic or Hermite coefficients. A useful 
feature of the Bezier control points representation is that the curve (not 
necessarily planar) lies within the convex hull of the four control points. We 
can use that fact to create a bounding sphere or oriented bounding box around 
the curve. We can check whether the control points are already convex to ensure
that the contained curve is well behaved, and subdivide if not.

Note that a cubic Bezier spline (made up of multiple segments) would not 
necessarily be composed of these because a spline can be contructed more 
compactly with shared end points. However, the primitive and inline methods 
here can be used for fast curve segment computations.

<h3>Theory</h3>
The primary reference for this implementation is the book "Geometric Modeling, 
3rd ed." by Michael E. Mortenson, Industrial Press 2006, chapter 4. We follow
Mortenson's notation here (with some name changes) and equation numbers are 
from the text. See CubicHermiteCurve_ comments for an introduction; here we
add the Bezier description to the algebraic and Hermite (geometric) forms 
described there.

The curve is parameterized by a scalar u in [0..1], such that points on the
curve, and their derivatives with respect to u are given by <pre>
    P(u)    =    B0(u) b0 +    B1(u) b1 +    B2(u) b2 +    B3(u) b3        (4.2)
    Pu(u)   =   B0u(u) b0 +   B1u(u) b1 +   B2u(u) b2 +   B3u(u) b3
    Puu(u)  =  B0uu(u) b0 +  B1uu(u) b1 +  B2uu(u) b2 +  B3uu(u) b3
    Puuu(u) = B0uuu(u) b0 + B1uuu(u) b1 + B2uuu(u) b2 + B3uuu(u) b3
</pre> where the Bi's are the (scalar) Bernstein polynomials given by 
<pre>
    B0(u) =      (1-u)^3 =  -u^3 + 3u^2 - 3u + 1                           (4.5)
    B1(u) = 3 u  (1-u)^2 =  3u^3 - 6u^2 + 3u
    B2(u) = 3 u^2(1-u)   = -3u^3 + 3u^2
    B3(u) =   u^3        =   u^3

    B0u(u) = -3u^2 +  6u - 3  B0uu(u) =  -6u +  6  B0uuu(u) =  -6       
    B1u(u) =  9u^2 - 12u + 3  B1uu(u) =  18u - 12  B1uuu(u) =  18   
    B2u(u) = -9u^2 +  6u      B2uu(u) = -18u +  6  B2uuu(u) = -18
    B3u(u) =  3u^2            B3uu(u) =   6u       B3uuu(u) =   6
</pre>
In matrix notation, let Fb=[B0 B1 B2 B3], and U=[u^3 u^2 u 1]. Then 
    Fb = U Mb
where Mb, the Bezier basis transformation matrix, and its inverse are:
<pre>
         [ -1  3 -3  1 ]             [ 0  0   0  1 ]
    Mb = [  3 -6  3  0 ]   inv(Mb) = [ 0  0  1/3 1 ]
         [ -3  3  0  0 ]             [ 0 1/3 2/3 1 ] 
         [  1  0  0  0 ]             [ 1  1   1  1 ]
</pre>
Now we can write the algebraic, Hermite, and Bezier forms in matrix notation.
Let A=~[a3 a2 a1 a0], H=~[h0 h1 hu0 hu1], B=~[b0 b1 b2 b3]. We have <pre>
    P(u) = U A = U Mh H = U Mb B                                           (4.7)
       A = Mh H = Mb B
       H = inv(Mh) A = inv(Mh) Mb B                                        (4.8)
       B = inv(Mb) A = inv(Mb) Mh H                                        (4.9)
</pre>
where these equations show how to convert among the algebraic, Hermite,
and Bezier forms. Note that while U, Fb, Mh, and Mb are ordinary matrices, A, H,
and B are hypermatrices since their elements are 3-vectors. Multiplying out
the matrix products gives: <pre>
               [  1  0  0  0 ]                 [ 1  0   0   0  ]
    Mh^-1 Mb = [  0  0  0  1 ]      Mb^-1 Mh = [ 1  0  1/3  0  ]
               [ -3  3  0  0 ]                 [ 0  1   0 -1/3 ] 
               [  0  0 -3  3 ]                 [ 0  1   0   0  ]
</pre>
Because of the sparsity of the matrices and the many common subexpressions 
above, it saves a considerable amount of computation to work out the necessary
products by hand, and this implementation does that. For example, to find the
Bezier control points B given the Hermite coefficients H, or vice versa, the 
matrix-vector multiply would take 3x28=84 flops, while the hand-worked versions
are: <pre>
       [h0 ]     -1       [    b0   ]         [b0]     -1       [   h0     ]
   H = [h1 ] = Mh  Mb B = [    b3   ]     B = [b1] = Mb  Mh H = [h0 + hu0/3]
       [hu0]              [3 (b1-b0)]         [b2]              [h1 - hu1/3]               
       [hu1]              [3 (b3-b2)]         [b3]              [   h1     ] 
</pre> 
which instead take 3x4=12 flops, 7X faster. Conversion between Bezier
and algebraic is a little more expensive: <pre>
              [ b3-b0 + 3 (b1-b2) ]                  [         a0          ]
   A = Mb B = [ 3 (b0+b2) - 6 b1  ]    B = Mb^-1 A = [     a1/3 + a0       ]
              [    3 (b1-b0)      ]                  [  (a2 + 2 a1)/3 + a0 ]
              [        b0         ]                  [  a3 + a2 + a1 + a0  ]
</pre>
which take about 3x10=30 flops, still almost 3X faster than a matrix-vector 
multiply.

@see CubicHermiteCurve_, BicubicBezierPatch_, BicubicHermitePatch_
**/
template <class P>
class Geo::CubicBezierCurve_ {
typedef P               RealP;
typedef Vec<3,RealP>    Vec3P;

public:
/** Construct an uninitialized curve; control points will be garbage. **/
CubicBezierCurve_() {}
/** Construct a cubic Bezier curve using the given control points 
B=[b0 b1 b2 b3]. **/
explicit CubicBezierCurve_(const Vec<4,Vec3P>& controlPoints) 
: B(controlPoints) {} 
/** Return a reference to the Bezier control points B=[b0 b1 b2 b3] that are
stored in this object. **/
const Vec<4,Vec3P>& getControlPoints() const {return B;}
/** Calculate the algebraic coefficients A=[a3 a2 a1 a0] from the stored
Bezier control points. Cost is 30 flops. **/
Vec<4,Vec3P> calcAlgebraicCoefficients() const {return calcAFromB(B);}
/** Calculate the Hermite (geometric) coefficients H=[h0 h1 hu0 hu1] from the 
stored Bezier control points. Cost is 12 flops. **/
Vec<4,Vec3P> calcHermiteCoefficients() const {return calcHFromB(B);}
/** Evaluate a point on this curve given a value for parameter u in [0,1].
Values outside this range are permitted but do not lie on the curve segment. 
Cost is 20 flops. **/
Vec3P evalP(RealP u) const {return evalPUsingB(B,u);}
/** Evaluate the tangent Pu=dP/du on this curve given a value for parameter u 
in [0,1]. Values outside this range are permitted but do not lie on the curve 
segment. Cost is 15 flops. **/
Vec3P evalPu(RealP u) const {return evalPuUsingB(B,u);}
/** Evaluate the second derivative Puu=d2P/du2 on this curve given a value for 
parameter u in [0,1]. Values outside this range are permitted but do not lie on
the curve segment. Cost is 10 flops. **/
Vec3P evalPuu(RealP u) const {return evalPuuUsingB(B,u);}
/** Evaluate the third derivative Puuu=d3P/du3 on this curve. Parameter u is
ignored here since the 3rd derivative of a cubic curve is a constant. Cost is 
3 flops. **/
Vec3P evalPuuu(RealP u) const {return evalPuuuUsingB(B,u);}

/** Return a sphere that surrounds the entire curve segment in the u=[0..1]
range. We use the fact that the curve is enclosed within the convex hull of
its control points and generate the minimum bounding sphere that includes all
four control points. **/
Geo::Sphere_<P> calcBoundingSphere() const 
{   return Geo::Point_<P>::calcBoundingSphere(B[0],B[1],B[2],B[3]); }

/** Return an axis-aligned bounding box (AABB) that surrounds the entire curve 
segment in the u=[0..1] range. We use the fact that the curve is enclosed 
within the convex hull of its control points and generate the minimum 
axis-aligned box that includes all four control points. **/
Geo::AlignedBox_<P> calcAxisAlignedBoundingBox() const 
{   const ArrayViewConst_<Vec3P> points(&B[0], &B[0]+4); // no copy or heap use
    return Geo::Point_<P>::calcAxisAlignedBoundingBox(points); }

/** Return an oriented bounding box (OBB) that surrounds the entire curve 
segment in the u=[0..1] range. We use the fact that the curve is enclosed 
within the convex hull of its control points and generate the minimum 
axis-aligned box that includes all four control points. **/
Geo::OrientedBox_<P> calcOrientedBoundingBox() const 
{   const ArrayViewConst_<Vec3P> points(&B[0], &B[0]+4); // no copy or heap use
    return Geo::Point_<P>::calcOrientedBoundingBox(points); }

/**@name                      Utility methods
These static methods provide operations useful for working with cubic Bezier
curves. See the CubicHermiteCurve_ class for related operations. **/
/**@{**/
/** Calculate the Bernstein basis functions Fb=[B0..B3] for a given value of 
the parameter u. This is an optimized calculation of U*Mb, taking 9 flops. **/
static Row<4,P> calcFb(RealP u) {
    const RealP u2 = u*u, u3 = u*u2;                // powers of u
    const RealP u1 = 1-u, u12=u1*u1, u13=u1*u12;    // powers of 1-u
    return Row<4,P>(u13, 3*u*u12, 3*u2*u1, u3); 
}

/** Calculate first derivatives Fbu=[B0u..B3u] of the Bernstein basis functions
for a given value of the parameter u. Cost is 10 flops. **/
static Row<4,P> calcFbu(RealP u) {
    const RealP u6=6*u, u2 = u*u, u23 = 3*u2, u29 = 9*u2;
    return Row<4,P>(u6-u23-3, u29-12*u+3, u6-u29); 
}

/** Calculate second derivatives Fbuu=[B0uu..B3uu] of the Bernstein basis
functions for a given value of the parameter u. Cost is 5 flops. **/
static Row<4,P> calcFbuu(RealP u) {
    const RealP u6  = 6*u, u18 = 18*u;
    return Row<4,P>(6-u6, u18-12, 6-u18, u6); 
}

/** Calculate third derivatives Fbuuu=[B0uuu..B3uuu] of the Bernstein basis 
functions for a given value of the parameter u. For a cubic curve this is
just a constant. Cost is 0 flops. **/
static Row<4,P> calcFbuuu(RealP u) {
    return Row<4,P>(-6, 18, -18, 6); 
}

/** Given the Bezier control points B=~[b0 b1 b2 b3], return the algebraic
coefficients A=~[a3 a2 a1 a0]. All coefficients are 3-vectors. Cost is 30
flops. **/
static Vec<4,Vec3P> calcAFromB(const Vec<4,Vec3P>& B) {
    const Vec3P& b0=B[0]; const Vec3P& b1=B[1];    // aliases for beauty
    const Vec3P& b2=B[2]; const Vec3P& b3=B[3];
    return Vec<4,Vec3P>(b3-b0+3*(b1-b2), 3*(b0+b2)-6*b1, 3*(b1-b0), b0);
}

/** Given the algebraic coefficients A=~[a3 a2 a1 a0], return the Bezier 
control points B=~[b0 b1 b2 b3]. All coefficients are 3-vectors. Cost is 27
flops. **/
static Vec<4,Vec3P> calcBFromA(const Vec<4,Vec3P>& A) {
    const Vec3P& a3=A[0]; const Vec3P& a2=A[1];    // aliases for beauty
    const Vec3P& a1=A[2]; const Vec3P& a0=A[3];
    return Vec<4,Vec3P>(a0, a1/3 + a0, (a2+2*a1)/3 + a0, a3+a2+a1+a0);
}

/** Given the Bezier control points B=~[b0 b1 b2 b3], return the Hermite
coefficients H=~[h0 h1 hu0 hu1]. All coefficients are 3-vectors. Cost is 12
flops. **/
static Vec<4,Vec3P> calcHFromB(const Vec<4,Vec3P>& B) {
    const Vec3P& b0=B[0]; const Vec3P& b1=B[1];    // aliases for beauty
    const Vec3P& b2=B[2]; const Vec3P& b3=B[3];
    return Vec<4,Vec3P>(b0, b3, 3*(b1-b0), 3*(b3-b2));
}

/** Given the Hermite coefficients H=~[h0 h1 hu0 hu1], return the Bezier
control points B=~[b0 b1 b2 b3]. All coefficients are 3-vectors. Cost is 12
flops. **/
static Vec<4,Vec3P> calcBFromH(const Vec<4,Vec3P>& H) {
    const Vec3P& h0= H[0]; const Vec3P& h1= H[1];    // aliases for beauty
    const Vec3P& hu0=H[2]; const Vec3P& hu1=H[3];
    return Vec<4,Vec3P>(h0, h0 + hu0/3, h1 - hu1/3, h1);
}

/** Given Bezier control points B and a value for the curve parameter u, return
the point P(u) at that location. Cost is 30 flops. Note that if you need to
do this for the same curve more than twice, it is cheaper to convert to
algebraic form using calcAFromB() (30 flops) and then evaluate using A 
(20 flops). **/
static Vec3P evalPUsingB(const Vec<4,Vec3P>& B, RealP u) { 
    return calcFb(u)*B; // 9 + 3*7 = 30 flops
}
/** Given Bezier control points B and a value for the curve parameter u, return
the first derivative Pu(u)=dP/du at that location. Cost is 31 flops. Note that
if you need to do this for the same curve more than once, it is cheaper to 
convert to algebraic form using calcAFromB() (30 flops) and then evaluate using
A (15 flops). **/
static Vec3P evalPuUsingB(const Vec<4,Vec3P>& B, RealP u) { 
    return calcFbu(u)*B; // 10 + 3*7 = 31 flops
}
/** Given Bezier control points B and a value for the curve parameter u, return
the second derivative Puu(u)=d2P/du2 at that location. Cost is 26 flops. Note 
that if you need to do this for the same curve more than once, it is cheaper to 
convert to algebraic form using calcAFromB() (30 flops) and then evaluate using
A (10 flops). **/
static Vec3P evalPuuUsingB(const Vec<4,Vec3P>& B, RealP u) { 
    return calcFbuu(u)*B; // 5 + 3*7 = 26 flops
}
/** Given Bezier control points B and a value for the curve parameter u, return
the third derivative Puuu(u)=d3P/du3 at that location. Cost is 21 flops. Note 
that if you need to do this for the same curve more than once, it is cheaper to 
convert to algebraic form using calcAFromB() (30 flops) and then evaluate using
A (3 flops). **/
static Vec3P evalPuuuUsingB(const Vec<4,Vec3P>& B, RealP u) { 
    return calcFbuuu(u)*B; // 0 + 3*7 = 21 flops
}
/** Obtain the Bezier basis matrix Mb explicitly. This is mostly useful for
testing since specialized routines can save a lot of CPU time over working
directly in matrix form. This is a constant matrix so there is no computation
cost. The matrix is symmetric although we return a full 4x4 here. **/
static Mat<4,4,P> getMb() {
    return Mat<4,4,P>( -1,  3, -3,  1,
                        3, -6,  3,  0,
                       -3,  3,  0,  0,
                        1,  0,  0,  0);
}

/** Form the product of the Bezier basis matrix Mb and a 4-vector, exploiting
the structure of Mb. Since Mb is symmetric you can also use this for 
multiplication by a row from the left, i.e. ~b*Mb=~(~Mb*b)=~(Mb*b).
Cost is 10 flops. **/
static Vec<4,P> multiplyByMb(const Vec<4,P>& b) {
    const RealP b0=b[0], b1=b[1], b2=b[2], b3=b[3];
    return Vec<4,P>(3*(b1-b2)+b3-b0, 3*(b0+b2)-6*b1, 3*(b1-b0), b0);
}

/** Obtain the inverse inv(Mb) of the Bezier basis matrix explicitly. This is
mostly useful for testing since specialized routines can save a lot of CPU time
over working directly in matrix form. This is a constant matrix so there is no
computation cost. The matrix is symmetric although we return a full 4x4 
here. **/
static Mat<4,4,P> getMbInv() {
    return Mat<4,4,P>( 0,    0,       0,   1,
                       0,    0,   P(1)/3,  1,
                       0, P(1)/3, P(2)/3,  1,
                       1,    1,       1,   1 );
}

/** Form the product of the inverse inv(Mb) of the Bezier basis matrix Mb and a 
4-vector, exploiting the structure of inv(Mb). Since inv(Mb) is symmetric you 
can also use this for multiplication by a row from the left, i.e. 
~b*Mb^-1=~(Mb^-T*b)=~(Mb^-1*b). Cost is 9 flops. **/
static Vec<4,P> multiplyByMbInv(const Vec<4,P>& b) {
    const RealP b0=b[0], b1=b[1], b2=b[2], b3=b[3];
    return Vec<4,P>(b3, b2/3+b3, (b1+2*b2)/3+b3, b0+b1+b2+b3);
}

/** Obtain the product Mh^-1*Mb explicitly; this is the matrix used for 
conversion from Bezier to Hermite bases since H=Mh^-1 Mb B and is the inverse
of the matrix Mb^-1*Mh. This is mostly useful for testing since specialized 
routines can save a lot of CPU time over working directly in matrix form. 
There is a very efficient method for forming matrix-vector products with this
matrix. This is a constant matrix so there is no computation cost. 
@see multiplyByMhInvMb(), getMbInvMh() **/
static Mat<4,4,P> getMhInvMb() {
    return Mat<4,4,P>(  1,  0,  0,  0,
                        0,  0,  0,  1,
                       -3,  3,  0,  0,
                        0,  0, -3,  3 );
}
/** Given a vector v, form the product inv(Mh)*Mb*v, exploiting the structure
of the constant matrix inv(Mh)*Mb (not symmetric). Cost is 4 flops. **/
static Vec<4,P> multiplyByMhInvMb(const Vec<4,P>& v) {
    const RealP v0=v[0], v1=v[1], v2=v[2], v3=v[3];
    return Vec<4,P>(v0, v3, 3*(v1-v0), 3*(v3-v2));
}

/** Obtain the product Mb^-1*Mh explicitly; this is the matrix used for 
conversion from Hermite to Bezier bases since B=Mb^-1 Mh H and is the inverse
of the matrix Mh^-1*Mb. This matrix is not symmetric. This method is mostly 
useful for testing since specialized routines can save a lot of CPU time over 
working directly in matrix form. There is a very efficient method for forming 
matrix-vector products with this matrix. This is a constant matrix so there is 
no computation cost here. 
@see multiplyByMhInvMb(), getMhInvMb() **/
static Mat<4,4,P> getMbInvMh() {
    return Mat<4,4,P>(  1,  0,    0,      0,
                        1,  0, P(1)/3,    0,
                        0,  1,    0,   P(-1)/3,
                        0,  1,    0,      0 );
}
/** Given a vector v, form the product inv(Mb)*Mh*v, exploiting the structure
of the constant matrix inv(Mb)*Mh (not symmetric). Cost is 4 flops. **/
static Vec<4,P> multiplyByMbInvMh(const Vec<4,P>& v) {
    const RealP v0=v[0], v1=v[1], v2=v[2], v3=v[3];
    return Vec<4,P>(v0, v0+v2/3, v1-v3/3, v1);
}
/**@}**/

private:
Vec<4,Vec3P> B;
};



} // namespace SimTK

#endif // SimTK_SIMMATH_GEO_CUBIC_BEZIER_CURVE_H_

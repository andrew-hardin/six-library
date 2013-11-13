/* =========================================================================
 * This file is part of scene-c++
 * =========================================================================
 *
 * (C) Copyright 2004 - 2013, General Dynamics - Advanced Information Systems
 *
 * scene-c++ is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; If not,
 * see <http://www.gnu.org/licenses/>.
 *
 */
#ifndef __SCENE_PROJECTION_MODEL_H__
#define __SCENE_PROJECTION_MODEL_H__

#include <scene/Types.h>
#include <scene/GridECEFTransform.h>
#include <import/math/poly.h>

namespace scene
{

const double DELTA_GP_MAX = 0.001;

class ProjectionModel
{
protected:
    Vector3 mSlantPlaneNormal;
    Vector3 mImagePlaneRowVector;
    Vector3 mImagePlaneColVector;
    Vector3 mImagePlaneNormal;
    Vector3 mSCP;
    double mScaleFactor;
    math::poly::OneD<Vector3> mARPPoly;
    math::poly::OneD<Vector3> mARPVelPoly;
    math::poly::TwoD<double> mTimeCOAPoly;
    int mLookDir;
public:

    enum { MAX_ITER = 50 };

    ProjectionModel(const Vector3& slantPlaneNormal,
                    const Vector3& imagePlaneRowVector,
                    const Vector3& imagePlaneColVector,
                    const Vector3& scp,
                    const math::poly::OneD<Vector3>& arpPoly,
                    const math::poly::TwoD<double>& timeCOAPoly,
                    int lookDir);

    virtual ~ProjectionModel() {}

    /*!
     *  Utility function that evaluates the TimeCOAPoly at the
     *  given pixel.
     */
    inline double computeImageTime(const types::RowCol<double> pixel) const
    {
        return mTimeCOAPoly(pixel.row, pixel.col);
    }

    /*!
     *  Utility function that evaluates the ARPPoly at the
     *  given time.
     */
    inline Vector3 computeARPPosition(const double time) const
    {
        return mARPPoly(time);
    }

    /*!
     *  Utility function that evaluates the ARPVelPoly at the
     *  given time.
     */
    inline Vector3 computeARPVelocity(const double time) const
    {
        return mARPVelPoly(time);
    }

    /*!
     *  Implements the transform from a plane point to row column
     *  by subtracting of the SCP projecting into row and column
     *  contributions.
     */
    inline types::RowCol<double>
        computeImageCoordinates(const Vector3& imagePlanePoint) const
    {
        // Delta IPP = xrow * uRow + ycol * uCol
        Vector3 delta(imagePlanePoint - mSCP);
            
        // What is the x contribution?
        return types::RowCol<double>(delta.dot(mImagePlaneRowVector),
                                    delta.dot(mImagePlaneColVector) );

    }

        
    /*!
     *  Virtual method to compute the R/Rdot contour from an
     *  image grid point.  Described in Chapter 4 of
     *  SICD Image Projections.  This routine is specific to IFP
     *  algorithm and to grid type, and so is provided in sub-classes
     *
     */
    virtual void computeContour(const Vector3& arpCOA,
                                const Vector3& velCOA,
                                double timeCOA,
                                const types::RowCol<double>& imageGridPoint,
                                double* r,
                                double* rDot) const = 0;
        
    /*!
     *  Calculations for section 5.2 in SICD Image Projections:
     *  R/Rdot Contour Ground Plane Intersection
     *
     */
    Vector3
        contourToGroundPlane(double rCOA, double rDotCOA,
                             const Vector3& arpCOA,
                             const Vector3& velCOA,
                             double timeCOA,
                             const Vector3& groundPlaneNormal,
                             const Vector3& groundRefPoint) const;

    /*!
     *  Implements chapter 6 Precise Scene to Image Grid
     *  Projection from SICD Image Projections, 6.1
     *  Scene to Image: Single Scene Point
     *
     *  This method is iterative.  Using a scenePoint, it iteratively
     *  projects this point into an image plane point, and
     *  converting that to a rg/az coord.  These rg/az coords
     *  are used in a IFP algorithm/grid-type specific way to
     *  find the range/range-rate contour (called R/Rdot in the doc).
     *  Finally this is used to perform a projection to the ground plane
     *  and compare with the reference until convergence is reached.
     *
     *  Note that in the document, Scene and Ground are used
     *  interchangeably,
     *  and sometimes even Image and Slant are as well.
     *
     *  \param scenePoint A scene (ground) point in 3-space
     *  \param oTimeCOA [output] An optional ptr to the timeCOA,
     *  which, if NULL is not set.
     *  \return An continuous surface image point
     *
     */
    types::RowCol<double> sceneToImage(const Vector3& scenePoint,
                                      double* oTimeCOA) const;


    /*!
     *  Implements (Slant plane) Image to Scene (Ground plane)
     *  projection using computerContour and contourToGroundPlane
     *
     *  \param imageGridPoint A point in the image surface (continuous)
     *  \param A ground plane reference point
     *  \param groundPlaneNormal A parameter which could just be the
     *  normalized ground plane reference point
     *  \param oTimeCOA [output] optional evaluation of TimeCOAPoly at
     *                  imageGridPoint.row, imageGridPoint.col
     *  \return A scene (ground) point in 3 space
     */
    Vector3 imageToScene(const types::RowCol<double>& imageGridPoint,
                                const Vector3& groundRefPoint,
                                const Vector3& groundPlaneNormal,
                                double *oTimeCOA) const;

    /*!
     * Implements chapter 9 Precise R/Rdot To Constant HAE Surface Projection
     * from SICD Image Projections, 9.1 Constant Height Surface & Surface
     * Normal
     *
     * This method is iterative.  It computes the R/Rdot projection to one
     * or more ground planes that are tangent to the constant height surface.
     * Each ground plane projection point computed is slightly above the
     * constant HAE surface.  The final surface position is computed by
     * projecting from the final ground plane projection point down to the
     * HAE surface
     *
     *  \param imageGridPoint A point (meters) in the image surface
     *  (continuous)
     *  \param height Surface height (meters) above the WGS-84 reference
     *  ellipsoid
     *  \param heightThreshold Height threshold (meters) for convergence of
     *  iterative projection sequence.  Must be positive.  Default value will
     *  yield highly accurate projection results, including for very short
     *  range and step grazing angle geometries.  Reducing this threshold will
     *  have negligible improvements in projection accuracy.
     *  \maxNumIters Maximum number of iterations to perform.  Even for very
     *  large images, the maximum iteration count required is expected to be
     *  2 for the default heightThreshold.
     *
     *  \return A scene (ground) point in 3 space at the desired height
     *
     */
    Vector3 imageToScene(const types::RowCol<double>& imageGridPoint,
                         double height,
                         double heightThreshold = 1.0,
                         size_t maxNumIters = 3) const;

    /*!
     * Samples a 10x10 grid of points that spans outExtent using
     * imageToScene().  From these samples, fits projection and time COA
     * polynomials of the specified order.  Optionally computes mean residual
     * errors in these polynomials (mean of the squares of the differences).
     *
     * inPixelStart and inSceneCenter are in the slant plane.
     *
     * interimSceneCenter and interimSampleSpacing are also in the slant
     * plane.  These represent the image at the point when the output to slant
     * polynomials will be applied.  They're intended to allow for any
     * image upsampling, etc. that may be done while still in the slant plane.
     * If no such upsampling is done, set interimSceneCenter = inSceneCenter.
     *
     * outSceneCenter, outSampleSpacing, and outExtent are in the output
     * plane.
     *
     * \param gridTransform Transform that knows how to convert from
     * output row/col pixel space to ECEF space
     * \param inPixelStart Input space start pixel (i.e. if this is non-zero,
     * it indicates an AOI)
     * \param inSceneCenter Input space scene center pixel in row/col
     * \param interimSceneCenter Scene center pixel in row/col of the image
     * when outputToSlantRow/Col polynomials are applied
     * \param interimSampleSpacing Sample spacing of the image when
     * outputToSlantRow/Col polynomials are applied
     * \param outSceneCenter Output space scene center pixel in row/col
     * \param outSampleSpacing Output space sample spacing
     * \param outExtent Output extent in row/col
     * \param polyOrder Polynomial order to use when fitting the polynomials
     * \param outputToSlantRow [output] Output to slant row polynomial
     * \param outputToSlantCol [output] Output to slant col polynomial
     * \param timeCOAPoly [output] Time center of aperture polynomial
     * (in meters from outSceneCenter)
     * \param meanResidualErrorRow [output] Optional.  Mean residual error in
     * outputToSlantRow
     * \param meanResidualErrorCol [output] Optional.  Mean residual error in
     * outputToSlantCol
     * \param meanResidualErrorTCOA [output] Optional.  Mean residual error in
     * timeCOAPoly
     */
    void computeProjectionPolynomials(
        const GridECEFTransform& gridTransform,
        const types::RowCol<size_t>& inPixelStart,
        const types::RowCol<double>& inSceneCenter,
        const types::RowCol<double>& interimSceneCenter,
        const types::RowCol<double>& interimSampleSpacing,
        const types::RowCol<double>& outSceneCenter,
        const types::RowCol<double>& outSampleSpacing,
        const types::RowCol<size_t>& outExtent,
        size_t polyOrder,
        math::poly::TwoD<double>& outputToSlantRow,
        math::poly::TwoD<double>& outputToSlantCol,
        math::poly::TwoD<double>& timeCOAPoly,
        double* meanResidualErrorRow = NULL,
        double* meanResidualErrorCol = NULL,
        double* meanResidualErrorTCOA = NULL) const;

    /*
     * Same process as above but only computes a pixel-based (from the
     * upper-left corner of the output grid) TLP.
     *
     * TODO: Break these methods into a class that does the grid sampling once
     * and can then provide you fitted polynomials in different units.
     *
     * \param gridTransform Transform that knows how to convert from
     * output row/col pixel space to ECEF space
     * \param outPixelStart Output space start pixel (i.e. if this is
     * non-zero, it indicates the SCP used with gridTransform is with respect
     * to a global space.  This basically translates to a multi-segment SICD.)
     * \param outExtent Output extent in row/col
     * \param polyOrder Polynomial order to use when fitting the polynomials
     * \param timeCOAPoly [output] Time center of aperture polynomial
     * (in pixels from the upper left corner of this extent, not the global
     * extent).
     */
    void computePixelBasedTimeCOAPolynomial(
        const GridECEFTransform& gridTransform,
        const types::RowCol<double>& outPixelStart,
        const types::RowCol<size_t>& outExtent,
        size_t polyOrder,
        math::poly::TwoD<double>& timeCOAPoly) const;
};

class RangeAzimProjectionModel : public ProjectionModel
{
protected:
    math::poly::OneD<double> mPolarAnglePoly;
    math::poly::OneD<double> mPolarAnglePolyPrime;
    math::poly::OneD<double> mKSFPoly;
    math::poly::OneD<double> mKSFPolyPrime;

public:
    RangeAzimProjectionModel(const math::poly::OneD<double>& polarAnglePoly,
                             const math::poly::OneD<double>& ksfPoly,
                             const Vector3& slantPlaneNormal,
                             const Vector3& imagePlaneRowVector,
                             const Vector3& imagePlaneColVector,
                             const Vector3& scp,
                             const math::poly::OneD<Vector3>& arpPoly,
                             const math::poly::TwoD<double>& timeCOAPoly,
                             int lookDir);

    virtual ~RangeAzimProjectionModel() {}

    virtual void computeContour(const Vector3& arpCOA,
                                const Vector3& velCOA,
                                double timeCOA,
                                const types::RowCol<double>& imageGridPoint,
                                double* r,
                                double* rDot) const;

};

class RangeZeroProjectionModel : public ProjectionModel
{
protected:
    math::poly::OneD<double> mTimeCAPoly;
    math::poly::TwoD<double> mDSRFPoly;
    double mRangeCA;
    
public:
    RangeZeroProjectionModel(const math::poly::OneD<double>& timeCAPoly,
                             const math::poly::TwoD<double>& dsrfPoly,
                             double rangeCA,
                             const Vector3& slantPlaneNormal,
                             const Vector3& imagePlaneRowVector,
                             const Vector3& imagePlaneColVector,
                             const Vector3& scp,
                             const math::poly::OneD<Vector3>& arpPoly,
                             const math::poly::TwoD<double>& timeCOAPoly,
                             int lookDir);

    virtual ~RangeZeroProjectionModel() {}

    virtual void computeContour(const Vector3& arpCOA,
                                const Vector3& velCOA,
                                double timeCOA,
                                const types::RowCol<double>& imageGridPoint,
                                double* r,
                                double* rDot) const;

};


class PlaneProjectionModel : public ProjectionModel
{
public:
    PlaneProjectionModel(const Vector3& slantPlaneNormal,
                         const Vector3& imagePlaneRowVector,
                         const Vector3& imagePlaneColVector,
                         const Vector3& scp,
                         const math::poly::OneD<Vector3>& arpPoly,
                         const math::poly::TwoD<double>& timeCOAPoly,
                         int lookDir);

    virtual ~PlaneProjectionModel() {}

    virtual void computeContour(const Vector3& arpCOA,
                                const Vector3& velCOA,
                                double timeCOA,
                                const types::RowCol<double>& imageGridPoint,
                                double* r,
                                double* rDot) const;

};

typedef PlaneProjectionModel XRGYCRProjectionModel;
typedef PlaneProjectionModel XCTYATProjectionModel;
}

#endif

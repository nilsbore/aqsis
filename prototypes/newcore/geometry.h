// Copyright (C) 2001, Paul C. Gregory and the other authors and contributors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the software's owners nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// (This is the New BSD license)

#ifndef AQSIS_GEOMETRY_H_INCLUDED
#define AQSIS_GEOMETRY_H_INCLUDED

#include "refcount.h"
#include "util.h"

namespace Aqsis {

class Grid;
class GridStorageBuilder;
class Geometry;
class Options;
class Attributes;
class TessControl;

typedef boost::intrusive_ptr<Geometry> GeometryPtr;
typedef boost::intrusive_ptr<Grid> GridPtr;

/// Tessellation context for geometric split/dice
///
/// The split/dice part of an reyes pipeline is an exercise in generating a
/// fine, uniform, adaptive tesselation of the geometry to be rendered.  The
/// uniformity of the tesselation is usually measured with respect to raster
/// space.
///
/// During tessellation, geometry needs to know how finely to split itself up,
/// and the results have to be placed somewhere.  TessellationContext fulfils
/// these needs.
class TessellationContext
{
    public:
        /// Invoke the tessellator object with the current geometry being
        /// processed as the argument.
        virtual void invokeTessellator(TessControl& tessControl) = 0;

        /// Push some geometry into the render pipeline.
        /// The results from splitting operations go here.
        virtual void push(const GeometryPtr& geom) = 0;

        /// Push a grid into the render pipeline.
        /// The results from geometry dicing go here.
        virtual void push(const GridPtr& grid) = 0;

        /// Return the renderer option state
        virtual const Options& options() = 0;
        /// Return the current surface attributes state
        virtual const Attributes& attributes() = 0;

        /// Get a builder for storage which will hold dicing results.
        virtual GridStorageBuilder& gridStorageBuilder() = 0;

        virtual ~TessellationContext() {};
};


/// Abstract piece of geometry to be rendered
///
/// TODO: Need really good docs on how this works
class Geometry : public RefCounted
{
    public:
        // Return the number of values required to represent a primvar of each
        // storage class for the surface.
//        virtual StorageCount storageCount() const = 0;

        /// Return a bounding box for the geometry.
        ///
        /// This will only be called once by the renderer for each piece of
        /// geometry - there's no need to cache the results internally or
        /// anything.
        ///
        virtual Box3f bound() const = 0;

        /// Tessellation driver function; the "split/dice decision"
        ///
        /// In reyes language, this function determines whether the surface
        /// should be split or diced.  It should then submit the resulting
        /// decision object to the tessellation context via the
        /// invokeTessellator() function.  (Avoiding doing the actual
        /// splitting and dicing directly inside this function allows the
        /// renderer to do the hard work when keeping track of deformation
        /// motion blur.)
        ///
        /// \param tessCoords - Transformation from object coordinates into
        ///   "tessellation coordinates".  Geometry classes should try to
        ///   ensure that the micropolygons (or other geometric pieces)
        ///   resulting from tessellation have area not greater than 1.0 in
        ///   tessellation coordinates.  When rasterizing, tessCoords will be a
        ///   projection into raster coordinates, but note that this is not
        ///   always the case.
        ///
        /// TODO: Maybe we should break with tradition, and try to put a bound
        /// on the *linear* dimensions of the diced pieces, rather than the
        /// area?  Needs thought.
        ///
        /// \param forceSplit - If this is greater than zero, it indicates
        ///   that the geometry cannot be diced into micropolygons because it
        ///   has a bound which spans the perspective divide defined by
        ///   tessCoords.  Instead the geometry should be split.  To ensure
        ///   that splitting makes progress, forceSplit contains the number of
        ///   forced splittings that the surface has already undergone.  The
        ///   geometry should use this information to alternate the splitting
        ///   in complimentary directions, because measuring lengths with
        ///   tessCoords is unreliable in this case.
        ///
        /// \param tessCtx - context in which to tessellate, holding the
        ///   options and attributes, etc.  tessellate should generate output
        ///   by calling tessCtx.invokeTessellator().
        ///
        virtual void tessellate(const M44f& tessCoords, int forceSplit,
                                TessellationContext& tessCtx) const = 0;

        virtual ~Geometry() {}
};


/// Control geometric tessellation (eg, know whether to split or dice)
///
/// Each Geometry class should have an associated TessControl class which can
/// hold the tessellation decision determined by Geometry::tessellate().
/// Normally the "tessellation decision" will be a split/dice decision, along
/// with associated split direction or dice resolution.
class TessControl
{
    public:
        /// Tessellate the geometry & push the results back to the context.
        virtual void tessellate(const Geometry& geom,
                                TessellationContext& tessContext) const = 0;

        virtual ~TessControl() {};
};


//------------------------------------------------------------------------------
// Tessellator control objects for the usual reyes-style split/dice
// tessellation of 2D surfaces:

/// Tessellator control for dicing 2D surfaces.
template<typename GeomT>
class SurfaceDicer : public TessControl
{
    private:
        int m_nu;
        int m_nv;
    public:
        SurfaceDicer(int nu, int nv) : m_nu(nu), m_nv(nv) {}

        virtual void tessellate(const Geometry& geom,
                                TessellationContext& tessContext) const
        {
            const GeomT& g = static_cast<const GeomT&>(geom);
            g.dice(m_nu, m_nv, tessContext);
        }
};


/// Tessellator control for splitting 2D surfaces.
template<typename GeomT>
class SurfaceSplitter : public TessControl
{
    private:
        bool m_splitDirectionU;
    public:
        SurfaceSplitter(bool splitDirectionU)
            : m_splitDirectionU(splitDirectionU) {}

        virtual void tessellate(const Geometry& geom,
                                TessellationContext& tessContext) const
        {
            const GeomT& g = static_cast<const GeomT&>(geom);
            g.split(m_splitDirectionU, tessContext);
        }
};

} // namespace Aqsis

#endif // AQSIS_GEOMETRY_H_INCLUDED

// Aqsis
// Copyright 1997 - 2001, Paul C. Gregory
//
// Contact: pgregory@aqsis.com
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


/** \file
		\brief Implements the CqSurfaceNurbs classes for handling Renderman NURBS primitives.
		\author Paul C. Gregory (pgregory@aqsis.com)
*/

#include	<math.h>
#include	<strstream>

#include	<stdio.h>

#include	"aqsis.h"
#include	"nurbs.h"
#include	"micropolygon.h"
#include	"renderer.h"
#include	"vector3d.h"
#include	"imagebuffer.h"
#include	"bilinear.h"
#include	"attributes.h"

START_NAMESPACE( Aqsis )


//---------------------------------------------------------------------
/** Constructor.
 */

CqSurfaceNURBS::CqSurfaceNURBS() : CqSurface(), m_cuVerts(0), m_cvVerts(0), m_uOrder(0), m_vOrder(0), m_umin(0.0f), m_umax(1.0f), m_vmin(0.0f), m_vmax(1.0f), m_fPatchMesh( TqFalse )
{
	TrimLoops() = static_cast<const CqAttributes*>(pAttributes()) ->TrimLoops();
}

//---------------------------------------------------------------------
/** Copy constructor.
 */

CqSurfaceNURBS::CqSurfaceNURBS( const CqSurfaceNURBS& From )
{
	*this = From;
}


//---------------------------------------------------------------------
/** Assignment operator.
 */

void CqSurfaceNURBS::operator=( const CqSurfaceNURBS& From )
{
	// Use the CqSurface assignment operator.
	CqSurface::operator=( From );

	// Initialise the NURBS surface.
	Init( From.m_uOrder, From.m_vOrder, From.m_cuVerts, From.m_cvVerts );

	m_umin = From.m_umin;
	m_umax = From.m_umax;
	m_vmin = From.m_vmin;
	m_vmax = From.m_vmax;

	m_fPatchMesh = From.m_fPatchMesh;

	// Copy the knot vectors.
	TqInt i;
	for ( i = From.m_auKnots.size() - 1; i >= 0; i-- )
		m_auKnots[ i ] = From.m_auKnots[ i ];
	for ( i = From.m_avKnots.size() - 1; i >= 0; i-- )
		m_avKnots[ i ] = From.m_avKnots[ i ];

	TrimLoops() = From.TrimLoops();
}


//---------------------------------------------------------------------
/** Comparison operator.
 */

TqInt CqSurfaceNURBS::operator==( const CqSurfaceNURBS& from )
{
	if ( from.m_cuVerts != m_cuVerts || from.m_cvVerts != m_cvVerts )
		return ( 0 );

	if ( from.m_uOrder != m_uOrder || from.m_vOrder != m_vOrder )
		return ( 0 );

	TqInt i;
	for ( i = P().Size() - 1; i >= 0; i-- )
	{
		if ( P() [ i ] != from.P() [ i ] )
			return ( 0 );
	}

	for ( i = m_auKnots.size() - 1; i >= 0; i-- )
	{
		if ( m_auKnots[ i ] != from.m_auKnots[ i ] )
			return ( 0 );
	}

	for ( i = m_avKnots.size() - 1; i >= 0; i-- )
	{
		if ( m_avKnots[ i ] != from.m_avKnots[ i ] )
			return ( 0 );
	}
	return ( 1 );
}


//---------------------------------------------------------------------
/** Find the span in the U knot vector containing the specified parameter value.
 */

TqUint CqSurfaceNURBS::FindSpanU( TqFloat u ) const
{
	if ( u >= m_auKnots[ m_cuVerts ] )
		return ( m_cuVerts -1 );
	if ( u <= m_auKnots[ uDegree() ] )
		return ( uDegree() );

	TqUint low = 0;
	TqUint high = m_cuVerts + 1;
	TqUint mid = ( low + high ) / 2;

	while ( u < m_auKnots[ mid ] || u >= m_auKnots[ mid + 1 ] )
	{
		if ( u < m_auKnots[ mid ] )
			high = mid;
		else
			low = mid;
		mid = ( low + high ) / 2;
	}
	return ( mid );
}


//---------------------------------------------------------------------
/** Find the span in the V knot vector containing the specified parameter value.
 */

TqUint CqSurfaceNURBS::FindSpanV( TqFloat v ) const
{
	if ( v >= m_avKnots[ m_cvVerts ] )
		return ( m_cvVerts -1 );
	if ( v <= m_avKnots[ vDegree() ] )
		return ( vDegree() );

	TqUint low = 0;
	TqUint high = m_cvVerts + 1;
	TqUint mid = ( low + high ) / 2;

	while ( v < m_avKnots[ mid ] || v >= m_avKnots[ mid + 1 ] )
	{
		if ( v < m_avKnots[ mid ] )
			high = mid;
		else
			low = mid;
		mid = ( low + high ) / 2;
	}
	return ( mid );
}


//---------------------------------------------------------------------
/** Return the basis functions for the specified parameter value.
 */

void CqSurfaceNURBS::BasisFunctions( TqFloat u, TqUint i, std::vector<TqFloat>& U, TqInt k, std::vector<TqFloat>& N )
{
	register TqInt j, r;
	register TqFloat saved, temp;
	std::vector<TqFloat> left(k), right(k);

	N[ 0 ] = 1.0f;
	for( j=1; j<=k-1; j++ )
	{
		left[ j ] = u - U[ i + 1 - j ];
		right[ j ] = U[ i + j ] - u;
		saved = 0.0f;
		for( r = 0; r < j; r++ )
		{
			temp = N[ r ] / ( right[ r + 1 ] + left[ j - r ] );
			N[ r ] = saved + right[ r + 1 ] * temp;
			saved = left[ j - r ] * temp;
		}
		N[ j ] = saved;
	}
}



//---------------------------------------------------------------------
/** Return the basis functions for the specified parameter value.
 */

void CqSurfaceNURBS::DersBasisFunctions( TqFloat u, TqUint i, std::vector<TqFloat>& U, TqInt k, TqInt n, std::vector<std::vector<TqFloat> >& ders )
{
	register TqInt j, r;
	register TqFloat saved, temp;
	std::vector<TqFloat> left(k), right(k);
	std::vector<std::vector<TqFloat> > ndu(k), a(2);
	for( j = 0; j < k; j++ )	ndu[ j ].resize(k);
	ders.resize( n+1 );
	for( j = 0; j < n+1; j++ )	ders[ j ].resize(k);
	a[ 0 ].resize(k);
	a[ 1 ].resize(k);

	TqUint p = k-1;

	ndu[ 0 ][ 0 ] = 1.0f;
	for( j = 1; j <= p; j++ )
	{
		left[ j ] = u - U[ i + 1 - j ];
		right[ j ] = U[ i + j ] - u;
		saved = 0.0f;
		for( r = 0; r < j; r++ )
		{
			ndu[ j ][ r ] = right[ r + 1 ] + left[ j - r ];
			temp = ndu[ r ][ j -1 ] / ndu[ j ][ r ];
			
			ndu[ r ][ j ] = saved + right[ r + 1 ] * temp;
			saved = left[ j - r ] * temp;
		}
		ndu[ j ][ j ] = saved;
	}

	// Load the basis functions
	for( j = 0; j <= p; j++ )
		ders[ 0 ][ j ] = ndu[ j ][ p ];

	// Compute the derivatives.
	for( r = 0; r <= p; r ++)
	{
		// Alternate rows in array a.
		TqInt s1 = 0;
		TqInt s2 = 1;
		a[ 0 ][ 0 ] = 1.0f;
		TqInt j1, j2;

		// Loop to compute the kth derivative
		for( k = 1; k <= n; k++ )
		{
			TqFloat d = 0.0f;
			TqInt rk = r - k;
			TqInt pk = p - k;
			if( r >= k )
			{
				a[ s2 ][ 0 ] = a[ s1 ][ 0 ] / ndu[ pk+1 ][ rk ];
				d = a[ s2 ][ 0 ] * ndu[ rk ][ pk ];
			}
			if( rk >= -1 )
				j1 = 1;
			else
				j1 = -rk;

			if( r-1 <= pk )
				j2 = k-1;
			else
				j2 = p-r;

			for( j = j1; j <= j2; j++)
			{
				a[ s2 ][ j ] = (a[ s1 ][ j ] - a[ s1 ][ j-1 ]) / ndu[ pk+1 ][ rk+j ];
				d += a[ s2 ][ j ] * ndu[ rk+j ][ pk ];
			}
			if( r <= pk )
			{
				a[ s2 ][ k ] = -a[ s1 ][ k-1 ] / ndu[ pk+1 ][ r ];
				d += a[ s2 ][ k ] * ndu[ r ][ pk ];
			}
			ders[ k ][ r ] = d;
			
			// Switch rows.
			j = s1;
			s1 = s2;
			s2 = j;
		}
	}
	// Multiply through the correct factors.
	r = p;
	for( k = 1; k <= n; k++ )
	{
		for( j = 0; j <= p; j++ )
			ders[ k ][ j ] *= r;
		r *= ( p-k );
	}
}


//---------------------------------------------------------------------
/** Evaluate the nurbs surface at parameter values u,v.
 */

CqVector4D	CqSurfaceNURBS::Evaluate( TqFloat u, TqFloat v )
{
	std::vector<TqFloat> Nu( m_uOrder );
	std::vector<TqFloat> Nv( m_vOrder );

	CqVector4D r( 0, 0, 0, 0 );

	/* Evaluate non-uniform basis functions (and derivatives) */

	TqUint uspan = FindSpanU( u );
	BasisFunctions( u, uspan, m_auKnots, m_uOrder, Nu );
	TqUint vspan = FindSpanV( v );
	BasisFunctions( v, vspan, m_avKnots, m_vOrder, Nv );
	TqUint uind = uspan - uDegree();

	CqVector4D S(0.0f, 0.0f, 0.0f, 1.0f);
	TqUint l, k;
	for( l = 0; l <= vDegree(); l++)
	{
		CqVector4D temp(0.0f, 0.0f, 0.0f, 1.0f);
		TqUint vind = vspan - vDegree() + l;
		for( k = 0; k <= uDegree(); k++ )
			temp = temp + Nu[ k ] * CP( uind + k, vind );
		S = S + Nv[ l ] * temp;
	}
	return(S);
}


//---------------------------------------------------------------------
/** Evaluate the nurbs surface at parameter values u,v.
 */

CqVector4D	CqSurfaceNURBS::EvaluateNormal( TqFloat u, TqFloat v )
{
	CqVector4D N;
	TqInt d = 1;	// Default to 1st order derivatives.
	TqInt k, l, s, r;
	TqInt p = uDegree();
	TqInt q = vDegree();

	std::vector<std::vector<CqVector4D> > SKL( d+1 );
	for( k = 0; k <= d; k++ )	SKL[ k ].resize( d+1 );
	std::vector<std::vector<TqFloat> > Nu, Nv;
	std::vector<CqVector4D> temp( q+1 );

	TqInt du = MIN(d, p);
	for( k = p+1; k <= d; k++ )
		for( l = 0; l <= d - k; l++ )
			SKL[ k ][ l ] = CqVector4D( 0.0f, 0.0f, 0.0f, 1.0f );

	TqInt dv = MIN(d, q);
	for( l = q+1; l <= d; l++ )
		for( k = 0; k <= d - l; k++ )
			SKL[ k ][ l ] = CqVector4D( 0.0f, 0.0f, 0.0f, 1.0f );

	TqUint uspan = FindSpanU( u );
	DersBasisFunctions( u, uspan, m_auKnots, m_uOrder, du, Nu );
	TqUint vspan = FindSpanV( v );
	DersBasisFunctions( v, vspan, m_avKnots, m_vOrder, dv, Nv );

	for( k = 0; k <= du; k++ )
	{
		for( s = 0; s <= q; s++ )
		{
			temp[ s ] = CqVector4D( 0.0f, 0.0f, 0.0f, 1.0f );
			for( r = 0; r <= p; r++ )
				temp[ s ] = temp[ s ] + Nu[ k ][ r ] * CP( uspan-p+r, vspan-q+s );
		}
		TqInt dd = MIN( d-k, dv );
		for( l = 0; l <= dd; l++ )
		{
			SKL[ k ][ l ] = CqVector4D( 0.0f, 0.0f, 0.0f, 1.0f );
			for( s = 0; s <= q; s++ )
				SKL[ k ][ l ] = SKL[ k ][ l ] + Nv[ l ][ s ] * temp[ s ];
		}
	}
	N = SKL[0][1]%SKL[1][0];
	N.Unit();
	return( N );	
}


//---------------------------------------------------------------------
/** Insert the specified knot into the U knot vector, and refine the control points accordingly.
 * \return The number of new knots created.
 */

TqUint CqSurfaceNURBS::InsertKnotU( TqFloat u, TqInt r )
{
	// Work on a copy.
	CqSurfaceNURBS nS( *this );
	nS.P() = P();

	TqInt k = m_auKnots.size() - 1, s = 0;
	TqInt i, j;
	TqInt p = uDegree();

	if ( u < m_auKnots[ uDegree() ] || u > m_auKnots[ m_cuVerts ] )
	{
		return ( 0 );
	}

	for ( i = 0; i < static_cast<TqInt>( m_auKnots.size() ); i++ )
	{
		if ( m_auKnots[ i ] > u )
		{
			k = i - 1;
			break;
		}
	}

	if ( u <= m_auKnots[ k ] )
	{
		s = 1;
		for ( i = k; i > 0; i-- )
		{
			if ( m_auKnots[ i ] <= m_auKnots[ i - 1 ] )
				s++;
			else
				break;
		}
	}
	else
		s = 0;

	if ( ( r + s ) > p + 1 )
		r = p + 1 - s;

	if ( r <= 0 )
		return ( 0 );

	nS.Init( m_uOrder, m_vOrder, m_cuVerts + r, m_cvVerts );

	// Load new knot vector
	for ( i = 0;i <= k;i++ ) nS.m_auKnots[ i ] = m_auKnots[ i ];
	for ( i = 1;i <= r;i++ ) nS.m_auKnots[ k + i ] = u;
	for ( i = k + 1;i < static_cast<TqInt>( m_auKnots.size() ); i++ )
		nS.m_auKnots[ i + r ] = m_auKnots[ i ];

	// Save unaltered control points
	std::vector<CqVector4D> R( p + 1 );

	// Insert control points as required on each row.
	TqUint row;
	for ( row = 0; row < m_cvVerts; row++ )
	{
		for ( i = 0; i <= k - p; i++ ) nS.CP( i, row ) = CP( i, row );
		for ( i = k - s; i < static_cast<TqInt>( m_cuVerts ); i++ )
			nS.CP( i + r, row ) = CP( i, row );
		for ( i = 0; i <= p - s; i++ ) R[ i ] = CP( k - p + i, row );

		// Insert the knot r times
		TqUint L = 0 ;
		TqFloat alpha;
		for ( j = 1; j <= r; j++ )
		{
			L = k - p + j;
			for ( i = 0;i <= p - j - s;i++ )
			{
				alpha = ( u - m_auKnots[ L + i ] ) / ( m_auKnots[ i + k + 1 ] - m_auKnots[ L + i ] );
				R[ i ] = CqVector4D( alpha * R[ i + 1 ].x() + ( 1.0 - alpha ) * R[ i ].x(),
				                     alpha * R[ i + 1 ].y() + ( 1.0 - alpha ) * R[ i ].y(),
				                     alpha * R[ i + 1 ].z() + ( 1.0 - alpha ) * R[ i ].z(),
				                     alpha * R[ i + 1 ].h() + ( 1.0 - alpha ) * R[ i ].h() );
				//				R[i]=alpha*R[i+1]+(1.0-alpha)*R[i];
			}
			nS.CP( L, row ) = R[ 0 ];
			if ( p - j - s > 0 )
				nS.CP( k + r - j - s, row ) = R[ p - j - s ];
		}

		// Load remaining control points
		for ( i = L + 1; i < k - s; i++ )
		{
			nS.CP( i, row ) = R[ i - L ];
		}
	}
	*this = nS;
	P() = nS.P();

	return ( r );
}


//---------------------------------------------------------------------
/** Insert the specified knot into the V knot vector, and refine the control points accordingly.
 * \return The number of new knots created.
 */

TqUint CqSurfaceNURBS::InsertKnotV( TqFloat v, TqInt r )
{
	// Work on a copy.
	CqSurfaceNURBS nS( *this );
	nS.P() = P();

	// Compute k and s      v = [ v_k , v_k+1)  with v_k having multiplicity s
	TqInt k = m_avKnots.size() - 1, s = 0;
	TqInt i, j;
	TqInt p = vDegree();

	if ( v < m_avKnots[ vDegree() ] || v > m_avKnots[ m_cvVerts ] )
	{
		return ( 0 );
	}

	for ( i = 0; i < static_cast<TqInt>( m_avKnots.size() ); i++ )
	{
		if ( m_avKnots[ i ] > v )
		{
			k = i - 1;
			break;
		}
	}

	if ( v <= m_avKnots[ k ] )
	{
		s = 1;
		for ( i = k; i > 0; i-- )
		{
			if ( m_avKnots[ i ] <= m_avKnots[ i - 1 ] )
				s++;
			else
				break;
		}
	}
	else
		s = 0;

	if ( ( r + s ) > p + 1 )
		r = p + 1 - s;

	if ( r <= 0 )
		return ( 0 );

	nS.Init( m_uOrder, m_vOrder, m_cuVerts, m_cvVerts + r );

	// Load new knot vector
	for ( i = 0;i <= k;i++ ) nS.m_avKnots[ i ] = m_avKnots[ i ];
	for ( i = 1;i <= r;i++ ) nS.m_avKnots[ k + i ] = v;
	for ( i = k + 1;i < static_cast<TqInt>( m_avKnots.size() ); i++ )
		nS.m_avKnots[ i + r ] = m_avKnots[ i ];

	// Save unaltered control points
	std::vector<CqVector4D> R( p + 1 );

	// Insert control points as required on each row.
	TqUint col;
	for ( col = 0; col < m_cuVerts; col++ )
	{
		for ( i = 0; i <= k - p; i++ ) nS.CP( col, i ) = CP( col, i );
		for ( i = k - s; i < static_cast<TqInt>( m_cvVerts ); i++ )
			nS.CP( col, i + r ) = CP( col, i );
		for ( i = 0; i <= p - s; i++ ) R[ i ] = CP( col, k - p + i );

		// Insert the knot r times
		TqUint L = 0 ;
		TqFloat alpha;
		for ( j = 1; j <= r; j++ )
		{
			L = k - p + j;
			for ( i = 0;i <= p - j - s;i++ )
			{
				alpha = ( v - m_avKnots[ L + i ] ) / ( m_avKnots[ i + k + 1 ] - m_avKnots[ L + i ] );
				R[ i ] = CqVector4D( alpha * R[ i + 1 ].x() + ( 1.0 - alpha ) * R[ i ].x(),
				                     alpha * R[ i + 1 ].y() + ( 1.0 - alpha ) * R[ i ].y(),
				                     alpha * R[ i + 1 ].z() + ( 1.0 - alpha ) * R[ i ].z(),
				                     alpha * R[ i + 1 ].h() + ( 1.0 - alpha ) * R[ i ].h() );
				//				R[i]=alpha*R[i+1]+(1.0-alpha)*R[i];
			}
			nS.CP( col, L ) = R[ 0 ];
			if ( p - j - s > 0 )
				nS.CP( col, k + r - j - s ) = R[ p - j - s ];
		}

		// Load remaining control points
		for ( i = L + 1; i < k - s; i++ )
		{
			nS.CP( col, i ) = R[ i - L ];
		}
	}
	*this = nS;
	P() = nS.P();

	return ( r );
}



//---------------------------------------------------------------------
/** Insert the specified knots into the U knot vector, and refine the control points accordingly.
 */

void CqSurfaceNURBS::RefineKnotU( const std::vector<TqFloat>& X )
{
	if ( X.size() <= 0 )
		return ;

	TqInt n = m_cuVerts - 1;
	TqInt p = uDegree();
	TqInt m = n + p + 1;
	TqInt a, b;
	TqInt r = X.size() - 1;

	CqSurfaceNURBS nS( *this );
	nS.P() = P();

	nS.Init( m_uOrder, m_vOrder, r + 1 + n + 1, m_cvVerts );

	a = FindSpanU( X[ 0 ] );
	b = FindSpanU( X[ r ] );
	++b;

	TqInt j, row;
	for ( row = 0; row < static_cast<TqInt>( m_cvVerts ); row++ )
	{
		for ( j = 0; j <= a - p ; j++ )
			nS.CP( j, row ) = CP( j, row );
		for ( j = b - 1; j <= n; j++ )
			nS.CP( j + r + 1, row ) = CP( j, row );
	}

	for ( j = 0; j <= a; j++ )
		nS.m_auKnots[ j ] = m_auKnots[ j ];

	for ( j = b + p; j <= m; j++ )
		nS.m_auKnots[ j + r + 1 ] = m_auKnots[ j ];

	TqInt i = b + p - 1;
	TqInt k = b + p + r;

	for ( j = r; j >= 0; j-- )
	{
		while ( X[ j ] <= m_auKnots[ i ] && i > a )
		{
			for ( row = 0; row < static_cast<TqInt>( m_cvVerts ); row++ )
				nS.CP( k - p - 1, row ) = CP( i - p - 1, row );
			nS.m_auKnots[ k ] = m_auKnots[ i ];
			--k;
			--i;
		}
		for ( row = 0; row < static_cast<TqInt>( m_cvVerts ); row++ )
			nS.CP( k - p - 1, row ) = nS.CP( k - p, row );

		TqInt l;
		for ( l = 1; l <= p ; l++ )
		{
			TqUint ind = k - p + l;
			TqFloat alpha = nS.m_auKnots[ k + l ] - X[ j ];
			if ( alpha == 0.0 )
			{
				for ( row = 0; row < static_cast<TqInt>( m_cvVerts ); row++ )
					nS.CP( ind - 1, row ) = nS.CP( ind, row );
			}
			else
			{
				alpha /= nS.m_auKnots[ k + l ] - m_auKnots[ i - p + l ];

				for ( row = 0; row < static_cast<TqInt>( m_cvVerts ); row++ )
					nS.CP( ind - 1, row ) = CqVector4D( alpha * nS.CP( ind - 1, row ).x() + ( 1.0 - alpha ) * nS.CP( ind, row ).x(),
					                                    alpha * nS.CP( ind - 1, row ).y() + ( 1.0 - alpha ) * nS.CP( ind, row ).y(),
					                                    alpha * nS.CP( ind - 1, row ).z() + ( 1.0 - alpha ) * nS.CP( ind, row ).z(),
					                                    alpha * nS.CP( ind - 1, row ).h() + ( 1.0 - alpha ) * nS.CP( ind, row ).h() );
			}
		}
		nS.m_auKnots[ k ] = X[ j ];
		--k;
	}
	*this = nS;
	P() = nS.P();
}


//---------------------------------------------------------------------
/** Insert the specified knots into the V knot vector, and refine the control points accordingly.
 */

void CqSurfaceNURBS::RefineKnotV( const std::vector<TqFloat>& X )
{
	if ( X.size() <= 0 )
		return ;

	TqInt n = m_cvVerts - 1;
	TqInt p = vDegree();
	TqInt m = n + p + 1;
	TqInt a, b;
	TqInt r = X.size() - 1;
	
	CqSurfaceNURBS nS( *this );
	nS.P() = P();

	nS.Init( m_uOrder, m_vOrder, m_cuVerts, r + 1 + n + 1 );

	a = FindSpanV( X[ 0 ] ) ;
	b = FindSpanV( X[ r ] ) ;
	++b;

	TqInt j, col;
	for ( col = 0; col < static_cast<TqInt>( m_cuVerts ); col++ )
	{
		for ( j = 0; j <= a - p; j++ )
			nS.CP( col, j ) = CP( col, j );
		for ( j = b - 1; j <= n; j++ )
			nS.CP( col, j + r + 1 ) = CP( col, j );
	}
	for ( j = 0; j <= a; j++ )
		nS.m_avKnots[ j ] = m_avKnots[ j ];

	for ( j = b + p; j <= m; j++ )
		nS.m_avKnots[ j + r + 1 ] = m_avKnots[ j ];

	TqInt i = b + p - 1;
	TqInt k = b + p + r;

	for ( j = r; j >= 0 ; j-- )
	{
		while ( X[ j ] <= m_avKnots[ i ] && i > a )
		{
			for ( col = 0; col < static_cast<TqInt>( m_cuVerts ); col++ )
				nS.CP( col, k - p - 1 ) = CP( col, i - p - 1 );
			nS.m_avKnots[ k ] = m_avKnots[ i ];
			--k;
			--i;
		}
		for ( col = 0; col < static_cast<TqInt>( m_cuVerts ); col++ )
			nS.CP( col, k - p - 1 ) = nS.CP( col, k - p );

		TqInt l;
		for ( l = 1; l <= p; l++ )
		{
			TqUint ind = k - p + l;
			TqFloat alpha = nS.m_avKnots[ k + l ] - X[ j ];
			if ( alpha == 0.0 )
			{
				for ( col = 0; col < static_cast<TqInt>( m_cuVerts ); col++ )
					nS.CP( col, ind - 1 ) = nS.CP( col, ind );
			}
			else
			{
				alpha /= nS.m_avKnots[ k + l ] - m_avKnots[ i - p + l ];
				for ( col = 0; col < static_cast<TqInt>( m_cuVerts ); col++ )
					nS.CP( col, ind - 1 ) = CqVector4D( alpha * nS.CP( col, ind - 1 ).x() + ( 1.0 - alpha ) * nS.CP( col, ind ).x(),
					                                    alpha * nS.CP( col, ind - 1 ).y() + ( 1.0 - alpha ) * nS.CP( col, ind ).y(),
					                                    alpha * nS.CP( col, ind - 1 ).z() + ( 1.0 - alpha ) * nS.CP( col, ind ).z(),
					                                    alpha * nS.CP( col, ind - 1 ).h() + ( 1.0 - alpha ) * nS.CP( col, ind ).h() );
			}
		}
		nS.m_avKnots[ k ] = X[ j ];
		--k;
	}
	*this = nS;
	P() = nS.P();
}


//---------------------------------------------------------------------
/** Ensure a nonperiodic (clamped) knot vector by inserting U[p] and U[m-p] multiple times.
 */

void CqSurfaceNURBS::ClampU()
{
	TqUint n1 = InsertKnotU( m_auKnots[ uDegree() ], uDegree() );
	TqUint n2 = InsertKnotU( m_auKnots[ m_cuVerts ], uDegree() );

	// Now trim unnecessary knots and control points
	if ( n1 || n2 )
	{
		CqSurfaceNURBS nS( *this );
		nS.P() = P();
		m_auKnots.resize( m_auKnots.size() - n1 - n2 );
		P().SetSize( ( m_cuVerts - n1 - n2 ) * m_cvVerts );
		m_cuVerts -= n1 + n2;
		TqUint i;
		for ( i = n1; i < nS.m_auKnots.size() - n2; i++ )
			m_auKnots[ i - n1 ] = nS.m_auKnots[ i ];
		TqUint row;
		for ( row = 0; row < m_cvVerts; row++ )
		{
			TqUint i;
			for ( i = n1; i < nS.m_cuVerts - n2; i++ )
				CP( i - n1, row ) = nS.CP( i, row );
		}
	}
}


//---------------------------------------------------------------------
/** Ensure a nonperiodic (clamped) knot vector by inserting V[p] and V[m-p] multiple times.
 */

void CqSurfaceNURBS::ClampV()
{
	TqUint n1 = InsertKnotV( m_avKnots[ vDegree() ], vDegree() );
	TqUint n2 = InsertKnotV( m_avKnots[ m_cvVerts ], vDegree() );

	// Now trim unnecessary knots and control points
	if ( n1 || n2 )
	{
		CqSurfaceNURBS nS( *this );
		nS.P() = P();
		m_avKnots.resize( m_avKnots.size() - n1 - n2 );
		P().SetSize( ( m_cvVerts - n1 - n2 ) * m_cuVerts );
		m_cvVerts -= n1 + n2;
		TqUint i;
		for ( i = n1; i < nS.m_avKnots.size() - n2; i++ )
			m_avKnots[ i - n1 ] = nS.m_avKnots[ i ];
		TqUint col;
		for ( col = 0; col < m_cuVerts; col++ )
		{
			TqUint i;
			for ( i = n1; i < nS.m_cvVerts - n2; i++ )
				CP( col, i - n1 ) = nS.CP( col, i );
		}
	}
}


//---------------------------------------------------------------------
/** Split this NURBS surface into two subsurfaces along u or v depending on dirflag (TRUE=u)
 */

void CqSurfaceNURBS::SplitNURBS( CqSurfaceNURBS& nrbA, CqSurfaceNURBS& nrbB, TqBool dirflag )
{
	CqSurfaceNURBS tmp( *this );
	tmp.P() = P();

	std::vector<TqFloat>& aKnots = ( dirflag ) ? m_auKnots : m_avKnots;
	TqUint Order = ( dirflag ) ? m_uOrder : m_vOrder;

	// Add a multiplicty k knot to the knot vector in the direction
	// specified by dirflag, and refine the surface.  This creates two
	// adjacent surfaces with c0 discontinuity at the seam.
	TqUint extra = 0L;
	TqUint last = ( dirflag ) ? ( m_cuVerts + m_uOrder - 1 ) : ( m_cvVerts + m_vOrder - 1 );
	//    TqUint middex=last/2;
	//    TqFloat midVal=aKnots[middex];
	TqFloat midVal = ( aKnots[ 0 ] + aKnots[ last ] ) / 2;
	TqUint middex = ( dirflag ) ? FindSpanU( midVal ) : FindSpanV( midVal );

	// Search forward and backward to see if multiple knot is already there
	TqUint i = 0;
	TqUint same = 0L;
	if ( aKnots[ middex ] == midVal )
	{
		i = middex + 1L;
		same = 1L;
		while ( ( i < last ) && ( aKnots[ i ] == midVal ) )
		{
			i++;
			same++;
		}

		i = middex - 1L;
		while ( ( i > 0L ) && ( aKnots[ i ] == midVal ) )
		{
			i--;
			middex--;	// middex is start of multiple knot
			same++;
		}
	}

	if ( i <= 0L )         	    // No knot in middle, must create it
	{
		midVal = ( aKnots[ 0L ] + aKnots[ last ] ) / 2.0;
		middex = 0;
		while ( aKnots[ middex + 1L ] < midVal )
			middex++;
		same = 0L;
	}

	extra = Order - same;
	std::vector<TqFloat> anewKnots( extra );

	if ( same < Order )         	    // Must add knots
	{
		for ( i = 0; i < extra; i++ )
			anewKnots[ i ] = midVal;
	}

	TqUint SplitPoint = ( extra < Order ) ? middex - 1L : middex;
	if ( dirflag ) tmp.RefineKnotU( anewKnots );
	else	tmp.RefineKnotV( anewKnots );

	// Build the two child surfaces, and copy the data from the refined
	// version of the parent (tmp) into the two children

	// First half
	nrbA.Init( m_uOrder, m_vOrder, ( dirflag ) ? SplitPoint + 1L : m_cuVerts, ( dirflag ) ? m_cvVerts : SplitPoint + 1L );
	TqUint j;
	for ( i = 0L; i < nrbA.m_cvVerts; i++ )
		for ( j = 0L; j < nrbA.m_cuVerts; j++ )
			nrbA.CP( j, i ) = tmp.CP( j, i );

	for ( i = 0L; i < nrbA.m_uOrder + nrbA.m_cuVerts; i++ )
		nrbA.m_auKnots[ i ] = tmp.m_auKnots[ i ];
	for ( i = 0L; i < nrbA.m_vOrder + nrbA.m_cvVerts; i++ )
		nrbA.m_avKnots[ i ] = tmp.m_avKnots[ i ];

	// Second half
	SplitPoint++;
	nrbB.Init( m_uOrder, m_vOrder, ( dirflag ) ? tmp.m_cuVerts - SplitPoint : m_cuVerts, ( dirflag ) ? m_cvVerts : tmp.m_cvVerts - SplitPoint );
	for ( i = 0L; i < nrbB.m_cvVerts; i++ )
	{
		for ( j = 0L; j < nrbB.m_cuVerts; j++ )
			nrbB.CP( j, i ) = tmp.CP( ( dirflag ) ? j + SplitPoint : j, ( dirflag ) ? i : ( i + SplitPoint ) );
	}
	for ( i = 0L; i < nrbB.m_uOrder + nrbB.m_cuVerts; i++ )
		nrbB.m_auKnots[ i ] = tmp.m_auKnots[ ( dirflag ) ? ( i + SplitPoint ) : i ];
	for ( i = 0L; i < nrbB.m_vOrder + nrbB.m_cvVerts; i++ )
		nrbB.m_avKnots[ i ] = tmp.m_avKnots[ ( dirflag ) ? i : ( i + SplitPoint ) ];
}


//---------------------------------------------------------------------
/** Split this NURBS surface into an array of Bezier segments
 */

void CqSurfaceNURBS::Decompose( std::vector<CqSurfaceNURBS*>& S )
{
	TqInt i, m, a, b, nb, mult, j, r, save, _s, k, row, col ;
	TqFloat numer, alpha ;

	TqInt lUses = Uses();

	std::vector<TqFloat> alphas( MAX( m_uOrder, m_vOrder ) );

	std::vector<TqFloat> nU( 2 * m_uOrder );
	for ( i = 0;i < static_cast<TqInt>( nU.size() / 2 );++i )
		nU[ i ] = 0;
	for ( i = nU.size() / 2;i < static_cast<TqInt>( nU.size() );++i )
		nU[ i ] = 1;

	std::vector<TqFloat> nV( 2 * m_vOrder );
	for ( i = 0;i < static_cast<TqInt>( nV.size() / 2 );++i )
		nV[ i ] = 0;
	for ( i = nV.size() / 2;i < static_cast<TqInt>( nV.size() );++i )
		nV[ i ] = 1;

	std::vector<CqSurfaceNURBS> Su( m_cuVerts - uDegree() );
	for ( i = 0;i < static_cast<TqInt>( Su.size() );i++ )
	{
		Su[ i ].Init( m_uOrder, m_vOrder, m_uOrder, m_cvVerts ) ;
		Su[ i ].m_auKnots = nU;
		Su[ i ].m_avKnots = m_avKnots;
	}

	m = m_cuVerts + uDegree();
	a = uDegree();
	b = m_uOrder;
	nb = 0;

	for ( i = 0;i <= static_cast<TqInt>( uDegree() );i++ )
		for ( col = 0;col < static_cast<TqInt>( m_cvVerts );col++ )
			Su[ nb ].CP( i, col ) = CP( i, col );

	while ( b < m )
	{
		i = b;
		while ( b < m && m_auKnots[ b + 1 ] <= m_auKnots[ b ] ) b++;
		mult = b - i + 1;
		if ( mult < static_cast<TqInt>( uDegree() ) )
		{
			numer = m_auKnots[ b ] - m_auKnots[ a ];	// the enumerator of the alphas
			for ( j = uDegree();j > mult;j-- )         		// compute and store the alphas
				alphas[ j - mult - 1 ] = numer / ( m_auKnots[ a + j ] - m_auKnots[ a ] );
			r = uDegree() - mult;				// insert knot r times
			for ( j = 1;j <= r;j++ )
			{
				save = r - j;
				_s = mult + j;					// this many new points
				for ( k = uDegree();k >= _s;k-- )
				{
					alpha = alphas[ k - _s ];
					for ( row = 0;row < static_cast<TqInt>( m_cvVerts );++row )
						Su[ nb ].CP( k, row ) = alpha * Su[ nb ].CP( k, row ) + ( 1.0 - alpha ) * Su[ nb ].CP( k - 1, row );
				}
				if ( b < m )          // control point of next patch
					for ( row = 0;row < static_cast<TqInt>( m_cvVerts );++row )
						Su[ nb + 1 ].CP( save, row ) = Su[ nb ].CP( uDegree(), row );
			}
		}
		++nb;
		if ( b < m )
		{ // initialize for next segment
			for ( i = uDegree() - mult; i <= static_cast<TqInt>( uDegree() ); ++i )
				for ( row = 0;row < static_cast<TqInt>( m_cvVerts );++row )
					Su[ nb ].CP( i, row ) = CP( b - uDegree() + i, row );
			a = b;
			++b;
		}
	}
	Su.resize( nb );

	S.resize( Su.size() * ( m_cvVerts - vDegree() ) ) ;

	for ( i = 0;i < static_cast<TqInt>( S.size() );i++ )
	{
		S[ i ] = new CqSurfaceNURBS(*this);
		S[ i ]->Init( m_uOrder, m_vOrder, m_uOrder, m_vOrder );
		S[ i ]->m_auKnots = nU;
		S[ i ]->m_avKnots = nV;
	}

	nb = 0;

	TqUint np;
	for ( np = 0;np < Su.size();++np )
	{
		for ( i = 0;i <= static_cast<TqInt>( uDegree() );i++ )
			for ( j = 0;j <= static_cast<TqInt>( vDegree() );++j )
				S[ nb ]->CP( i, j ) = Su[ np ].CP( i, j );
		m = m_cvVerts + vDegree();
		a = vDegree() ;
		b = m_vOrder;
		while ( b < m )
		{
			i = b;
			while ( b < m && m_avKnots[ b + 1 ] <= m_avKnots[ b ] ) b++;
			mult = b - i + 1;
			if ( mult < static_cast<TqInt>( vDegree() ) )
			{
				numer = m_avKnots[ b ] - m_avKnots[ a ];	// the enumerator of the alphas
				for ( j = vDegree();j > mult;j-- )         			// compute and store the alphas
					alphas[ j - mult - 1 ] = numer / ( m_avKnots[ a + j ] - m_avKnots[ a ] );
				r = vDegree() - mult;					// insert knot r times
				for ( j = 1;j <= r;j++ )
				{
					save = r - j;
					_s = mult + j; // this many new points
					for ( k = vDegree();k >= _s;k-- )
					{
						alpha = alphas[ k - _s ];
						for ( col = 0;col <= static_cast<TqInt>( uDegree() );++col )
							S[ nb ]->CP( col, k ) = alpha * S[ nb ]->CP( col, k ) + ( 1.0 - alpha ) * S[ nb ]->CP( col, k - 1 );
					}
					if ( b < m )          // control point of next patch
						for ( col = 0;col <= static_cast<TqInt>( uDegree() );++col )
							S[ nb + 1 ]->CP( col, save ) = S[ nb ]->CP( col, vDegree() );
				}
			}
			++nb;
			if ( b < m )
			{ // initialize for next patch
				for ( i = vDegree() - mult; i <= static_cast<TqInt>( vDegree() ); ++i )
					for ( col = 0;col <= static_cast<TqInt>( uDegree() );++col )
						S[ nb ]->CP( col, i ) = Su[ np ].CP( col, b - vDegree() + i );
				a = b;
				++b;
			}
		}
	}

	S.resize( nb );

	TqInt irow, icol;
	TqInt nuSegs = cuSegments();
	TqInt nvSegs = cvSegments();
	for( icol = 0; icol < nvSegs; icol++ )
	{
		for( irow = 0; irow < nuSegs; irow++ )
		{
			/// \note: We swap here due to the way the patches in the array are constructed.
			TqInt iPatch = ( irow * nvSegs ) + icol;
			TqInt iA = ( icol * ( nuSegs + 1 ) ) + irow;
			TqInt iB = ( icol * ( nuSegs + 1 ) ) + irow + 1;
			TqInt iC = ( ( icol + 1 ) * ( nuSegs + 1 ) ) + irow;
			TqInt iD = ( ( icol + 1 ) * ( nuSegs + 1 ) ) + irow + 1;

			std::vector<CqParameter*>::iterator iUP;
			for( iUP = aUserParams().begin(); iUP != aUserParams().end(); iUP++ )
			{
				switch( (*iUP)->Type() )
				{
					case type_integer:
					{
						CqParameterTyped<TqInt, TqFloat>* pUPV = static_cast<CqParameterTyped<TqInt, TqFloat>*>( (*iUP) );
						CqParameterTyped<TqInt, TqFloat>* pNUPV = static_cast<CqParameterTyped<TqInt, TqFloat>*>( (*iUP)->Clone() );
						S[ iPatch ]->AddPrimitiveVariable(pNUPV);
						pNUPV->SetSize(4);
						*pNUPV->pValue( 0 ) = *pUPV->pValue( iA );
						*pNUPV->pValue( 1 ) = *pUPV->pValue( iB );
						*pNUPV->pValue( 2 ) = *pUPV->pValue( iC );
						*pNUPV->pValue( 3 ) = *pUPV->pValue( iD );
					}
					break;

					case type_float:
					{
						CqParameterTyped<TqFloat, TqFloat>* pUPV = static_cast<CqParameterTyped<TqFloat, TqFloat>*>( (*iUP) );
						CqParameterTyped<TqFloat, TqFloat>* pNUPV = static_cast<CqParameterTyped<TqFloat, TqFloat>*>( (*iUP)->Clone() );
						S[ iPatch ]->AddPrimitiveVariable(pNUPV);
						pNUPV->SetSize(4);
						*pNUPV->pValue( 0 ) = *pUPV->pValue( iA );
						*pNUPV->pValue( 1 ) = *pUPV->pValue( iB );
						*pNUPV->pValue( 2 ) = *pUPV->pValue( iC );
						*pNUPV->pValue( 3 ) = *pUPV->pValue( iD );
					}
					break;

					case type_color:
					{
						CqParameterTyped<CqColor, CqColor>* pUPV = static_cast<CqParameterTyped<CqColor, CqColor>*>( (*iUP) );
						CqParameterTyped<CqColor, CqColor>* pNUPV = static_cast<CqParameterTyped<CqColor, CqColor>*>( (*iUP)->Clone() );
						S[ iPatch ]->AddPrimitiveVariable(pNUPV);
						pNUPV->SetSize(4);
						*pNUPV->pValue( 0 ) = *pUPV->pValue( iA );
						*pNUPV->pValue( 1 ) = *pUPV->pValue( iB );
						*pNUPV->pValue( 2 ) = *pUPV->pValue( iC );
						*pNUPV->pValue( 3 ) = *pUPV->pValue( iD );
					}
					break;

					case type_point:
					case type_normal:
					case type_vector:
					{
						CqParameterTyped<CqVector3D, CqVector3D>* pUPV = static_cast<CqParameterTyped<CqVector3D, CqVector3D>*>( (*iUP) );
						CqParameterTyped<CqVector3D, CqVector3D>* pNUPV = static_cast<CqParameterTyped<CqVector3D, CqVector3D>*>( (*iUP)->Clone() );
						S[ iPatch ]->AddPrimitiveVariable(pNUPV);
						pNUPV->SetSize(4);
						*pNUPV->pValue( 0 ) = *pUPV->pValue( iA );
						*pNUPV->pValue( 1 ) = *pUPV->pValue( iB );
						*pNUPV->pValue( 2 ) = *pUPV->pValue( iC );
						*pNUPV->pValue( 3 ) = *pUPV->pValue( iD );
					}
					break;

					case type_hpoint:
					{
						CqParameterTyped<CqVector4D, CqVector3D>* pUPV = static_cast<CqParameterTyped<CqVector4D, CqVector3D>*>( (*iUP) );
						CqParameterTyped<CqVector4D, CqVector3D>* pNUPV = static_cast<CqParameterTyped<CqVector4D, CqVector3D>*>( (*iUP)->Clone() );
						S[ iPatch ]->AddPrimitiveVariable(pNUPV);
						pNUPV->SetSize(4);
						*pNUPV->pValue( 0 ) = *pUPV->pValue( iA );
						*pNUPV->pValue( 1 ) = *pUPV->pValue( iB );
						*pNUPV->pValue( 2 ) = *pUPV->pValue( iC );
						*pNUPV->pValue( 3 ) = *pUPV->pValue( iD );
					}
					break;

					case type_string:
					{
						CqParameterTyped<CqString, CqString>* pUPV = static_cast<CqParameterTyped<CqString, CqString>*>( (*iUP) );
						CqParameterTyped<CqString, CqString>* pNUPV = static_cast<CqParameterTyped<CqString, CqString>*>( (*iUP)->Clone() );
						S[ iPatch ]->AddPrimitiveVariable(pNUPV);
						pNUPV->SetSize(4);
						*pNUPV->pValue( 0 ) = *pUPV->pValue( iA );
						*pNUPV->pValue( 1 ) = *pUPV->pValue( iB );
						*pNUPV->pValue( 2 ) = *pUPV->pValue( iC );
						*pNUPV->pValue( 3 ) = *pUPV->pValue( iD );
					}
					break;

					case type_matrix:
					{
						CqParameterTyped<CqMatrix, CqMatrix>* pUPV = static_cast<CqParameterTyped<CqMatrix, CqMatrix>*>( (*iUP) );
						CqParameterTyped<CqMatrix, CqMatrix>* pNUPV = static_cast<CqParameterTyped<CqMatrix, CqMatrix>*>( (*iUP)->Clone() );
						S[ iPatch ]->AddPrimitiveVariable(pNUPV);
						pNUPV->SetSize(4);
						*pNUPV->pValue( 0 ) = *pUPV->pValue( iA );
						*pNUPV->pValue( 1 ) = *pUPV->pValue( iB );
						*pNUPV->pValue( 2 ) = *pUPV->pValue( iC );
						*pNUPV->pValue( 3 ) = *pUPV->pValue( iD );
					}
					break;
				}
			}
		}
	}
}


//---------------------------------------------------------------------
/** Find the point at which two infinite lines intersect.
 * The algorithm generates a plane from one of the lines and finds the 
 * intersection point between this plane and the other line.
 * \return TqFalse if they are parallel, TqTrue if they intersect.
 */

TqBool CqSurfaceNURBS::IntersectLine( CqVector3D& P1, CqVector3D& T1, CqVector3D& P2, CqVector3D& T2, CqVector3D& P )
{
	CqVector3D	v, px;

	px = T1 % ( P1 - T2 );
	v = px % T1;

	TqFloat	t = ( P1 - P2 ) * v;
	TqFloat vw = v * T2;
	if ( ( vw * vw ) < 1.0e-07 )
		return ( TqFalse );
	t /= vw;
	P = P2 + ( ( ( P1 - P2 ) * v ) / vw ) * T2 ;
	return ( TqTrue );
}


//---------------------------------------------------------------------
/** Project a point onto a line, returns the projection point in p.
 */

void CqSurfaceNURBS::ProjectToLine( const CqVector3D& S, const CqVector3D& Trj, const CqVector3D& pnt, CqVector3D& p )
{
	CqVector3D a = pnt - S;
	TqFloat fraction, denom;
	denom = Trj.Magnitude2();
	fraction = ( denom == 0.0 ) ? 0.0 : ( Trj * a ) / denom;
	p = fraction * Trj;
	p += S;
}


//---------------------------------------------------------------------
/** Generate a circle segment between as and ae of radius r, using X,Y as the x and y axes and O as the origin.
 */

void CqSurfaceNURBS::Circle( const CqVector3D& O, const CqVector3D& X, const CqVector3D& Y, TqFloat r, TqFloat as, TqFloat ae )
{
	TqFloat theta, angle, dtheta;
	TqUint narcs;

	while ( ae < as )
		ae += 2 * RI_PI;

	theta = ae - as;
	if ( theta <= RI_PIO2 )
		narcs = 1;
	else
	{
		if ( theta <= RI_PI )
			narcs = 2;
		else
		{
			if ( theta <= 1.5 * RI_PI )
				narcs = 3;
			else
				narcs = 4;
		}
	}
	dtheta = theta / static_cast<TqFloat>( narcs );
	TqUint n = 2 * narcs + 1;				// n control points ;
	TqFloat w1 = cos( dtheta / 2.0 );		// dtheta/2.0 is base angle

	CqVector3D P0, T0, P2, T2, P1;
	P0 = O + r * cos( as ) * X + r * sin( as ) * Y;
	T0 = -sin( as ) * X + cos( as ) * Y;		// initialize start values
	Init( 3, 0, n, 1 );

	P() [ 0 ] = P0;
	TqUint index = 0;
	angle = as;

	TqUint i;
	for ( i = 1; i <= narcs; i++ )
	{
		angle += dtheta;
		P2 = O + r * cos( angle ) * X + r * sin( angle ) * Y;
		P() [ index + 2 ] = P2;
		T2 = -sin( angle ) * X + cos( angle ) * Y;
		IntersectLine( P0, T0, P2, T2, P1 );
		P() [ index + 1 ] = P1 * w1;
		P() [ index + 1 ].h( w1 );
		index += 2;
		if ( i < narcs )
		{
			P0 = P2;
			T0 = T2;
		}
	}

	TqUint j = 2 * narcs + 1;				// load the knot vector
	for ( i = 0; i < 3; i++ )
	{
		m_auKnots[ i ] = 0.0;
		m_auKnots[ i + j ] = 1.0;
	}

	switch ( narcs )
	{
			case 1:
			break;

			case 2:
			m_auKnots[ 3 ] = m_auKnots[ 4 ] = 0.5;
			break ;

			case 3:
			m_auKnots[ 3 ] = m_auKnots[ 4 ] = 1.0 / 3.0;
			m_auKnots[ 5 ] = m_auKnots[ 6 ] = 2.0 / 3.0;
			break ;

			case 4:
			m_auKnots[ 3 ] = m_auKnots[ 4 ] = 0.25;
			m_auKnots[ 5 ] = m_auKnots[ 6 ] = 0.50;
			m_auKnots[ 7 ] = m_auKnots[ 8 ] = 0.75;
			break ;
	}
}


//---------------------------------------------------------------------
/** Generate a linear segment as a nurbs curve for use in surface generation.
 */

void CqSurfaceNURBS::LineSegment( const CqVector3D& P1, CqVector3D& P2 )
{
	// Initialise the curve to the required size.
	Init( 2, 0, 2, 1 );

	// Fill in the knot vector.
	m_auKnots[ 0 ] = m_auKnots[ 1 ] = 0.0;
	m_auKnots[ 2 ] = m_auKnots[ 3 ] = 1.0;

	P() [ 0 ] = P1;
	P() [ 1 ] = P2;
}


//---------------------------------------------------------------------
/** Generate a surface by revolving a curve around a given axis by a given amount.
 */

void CqSurfaceNURBS::SurfaceOfRevolution( const CqSurfaceNURBS& profile, const CqVector3D& S, const CqVector3D& Tvec, TqFloat theta )
{
	TqFloat angle, dtheta;
	TqUint narcs;
	TqUint i, j;

	if ( fabs( theta ) > 2.0 * RI_PI )
	{
		if ( theta < 0 )
			theta = -( 2.0 * RI_PI );
		else
			theta = 2.0 * RI_PI;
	}

	if ( fabs( theta ) <= RI_PIO2 )
		narcs = 1;
	else
	{
		if ( fabs( theta ) <= RI_PI )
			narcs = 2;
		else
		{
			if ( fabs( theta ) <= 1.5 * RI_PI )
				narcs = 3;
			else
				narcs = 4;
		}
	}
	dtheta = theta / static_cast<TqFloat>( narcs );

	TqUint n = 2 * narcs + 1;					// n control points ;
	Init( 3, profile.uOrder(), n, profile.cuVerts() );

	switch ( narcs )
	{
			case 1:
			break;

			case 2:
			m_auKnots[ 3 ] = m_auKnots[ 4 ] = 0.5;
			break;

			case 3:
			m_auKnots[ 3 ] = m_auKnots[ 4 ] = 1.0 / 3.0;
			m_auKnots[ 5 ] = m_auKnots[ 6 ] = 2.0 / 3.0;
			break;

			case 4:
			m_auKnots[ 3 ] = m_auKnots[ 4 ] = 0.25;
			m_auKnots[ 5 ] = m_auKnots[ 6 ] = 0.50;
			m_auKnots[ 7 ] = m_auKnots[ 8 ] = 0.75;
			break;
	}

	j = 3 + 2 * ( narcs - 1 );					// loading the end knots
	for ( i = 0;i < 3;j++, i++ )
	{
		m_auKnots[ i ] = 0.0;
		m_auKnots[ j ] = 1.0;
	}

	m_avKnots = profile.m_auKnots;

	TqFloat wm = cos( dtheta / 2.0 );			// dtheta/2.0 is base angle

	std::vector<TqFloat> cosines( narcs + 1 );
	std::vector<TqFloat> sines( narcs + 1 );

	angle = 0.0;
	for ( i = 1; i <= narcs; i++ )
	{
		angle = dtheta * static_cast<TqFloat>( i );
		cosines[ i ] = cos( angle );
		sines[ i ] = sin( angle );
	}

	CqVector3D P0, T0, P2, T2, P1;

	for ( j = 0; j < m_cvVerts; j++ )
	{
		CqVector3D O;
		CqVector3D pj( profile.CP( j, 0 ) );
		TqFloat wj = profile.CP( j, 0 ).h();

		ProjectToLine( S, Tvec, pj, O );
		CqVector3D X, Y;
		X = pj - O;

		TqFloat r = X.Magnitude();

		if ( r < 1e-7 )
		{
			for ( i = 0;i < m_cuVerts; i += 2 )
			{
				CP( i, j ) = O * wj;
				CP( i, j ).h( wj );
				if ( i + 1 < m_cuVerts )
				{
					CP( i + 1, j ) = O * cos( RI_PIO2 / 2 );
					CP( i + 1, j ).h( cos( RI_PIO2 / 2 ) );
				}
			}
			continue;
		}

		X.Unit();
		Y = Tvec % X;
		Y.Unit();

		P0 = profile.CP( j, 0 );
		CP( 0, j ) = profile.CP( j, 0 );

		T0 = Y;
		TqUint index = 0;

		for ( i = 1; i <= narcs; ++i )
		{
			angle = dtheta * static_cast<TqFloat>( i );
			P2 = O + r * cosines[ i ] * X + r * sines[ i ] * Y;
			CP( index + 2, j ) = P2 * wj;
			CP( index + 2, j ).h( wj );
			T2 = -sines[ i ] * X + cosines[ i ] * Y;
			IntersectLine( P0, T0, P2, T2, P1 );
			CP( index + 1, j ) = P1 * ( wm * wj );
			CP( index + 1, j ).h( wm * wj );
			index += 2;
			if ( i < narcs )
			{
				P0 = P2;
				T0 = T2;
			}
		}
	}
}


//---------------------------------------------------------------------
/** Subdivide a bicubic patch in the u direction, return the left side.
 */

void CqSurfaceNURBS::uSubdivide( CqSurfaceNURBS*& pnrbA, CqSurfaceNURBS*& pnrbB )
{
	pnrbA = new CqSurfaceNURBS( *this );
	pnrbB = new CqSurfaceNURBS( *this );
	SplitNURBS( *pnrbA, *pnrbB, TqTrue );

	// Subdivide the normals
	if ( USES( Uses(), EnvVars_N ) ) 
	{
		pnrbA->N() = N();
		pnrbA->N().uSubdivide( &pnrbB->N() );
	}

	uSubdivideUserParameters( pnrbA, pnrbB );
}


//---------------------------------------------------------------------
/** Subdivide a bicubic patch in the v direction, return the top side.
 */

void CqSurfaceNURBS::vSubdivide( CqSurfaceNURBS*& pnrbA, CqSurfaceNURBS*& pnrbB )
{
	pnrbA = new CqSurfaceNURBS( *this );
	pnrbB = new CqSurfaceNURBS( *this );
	SplitNURBS( *pnrbA, *pnrbB, TqFalse );

	// Subdivide the normals
	if ( USES( Uses(), EnvVars_N ) ) 
	{
		pnrbA->N() = N();
		pnrbA->N().vSubdivide( &pnrbB->N() );
	}

	vSubdivideUserParameters( pnrbA, pnrbB );
}


//---------------------------------------------------------------------
/** Return the boundary extents in object space of the surface patch
 */

CqBound CqSurfaceNURBS::Bound() const
{
	// Get the boundary in camera space.
	CqVector3D	vecA( FLT_MAX, FLT_MAX, FLT_MAX );
	CqVector3D	vecB( -FLT_MAX, -FLT_MAX, -FLT_MAX );
	TqUint i;
	for ( i = 0; i < m_cuVerts*m_cvVerts; i++ )
	{
		CqVector3D	vecV = P() [ i ];
		if ( vecV.x() < vecA.x() ) vecA.x( vecV.x() );
		if ( vecV.y() < vecA.y() ) vecA.y( vecV.y() );
		if ( vecV.x() > vecB.x() ) vecB.x( vecV.x() );
		if ( vecV.y() > vecB.y() ) vecB.y( vecV.y() );
		if ( vecV.z() < vecA.z() ) vecA.z( vecV.z() );
		if ( vecV.z() > vecB.z() ) vecB.z( vecV.z() );
	}
	CqBound	B;
	B.vecMin() = vecA;
	B.vecMax() = vecB;

	return ( B );
}


//---------------------------------------------------------------------
/** Dice the patch into a mesh of micropolygons.
 */

void CqSurfaceNURBS::NaturalInterpolate(CqParameter* pParameter, TqInt uDiceSize, TqInt vDiceSize, IqShaderData* pData)
{
	CqVector4D vec1;
	TqInt iv;
	for ( iv = 0; iv <= vDiceSize; iv++ )
	{
		TqFloat sv = ( static_cast<TqFloat>( iv ) / static_cast<TqFloat>( vDiceSize ) )
					 * ( m_avKnots[ m_cvVerts ] - m_avKnots[ m_vOrder - 1 ] )
					 + m_avKnots[ m_vOrder - 1 ];
		TqInt iu;
		for ( iu = 0; iu <= uDiceSize; iu++ )
		{
			TqInt igrid = ( iv * ( uDiceSize + 1) ) + iu;
			TqFloat su = ( static_cast<TqFloat>( iu ) / static_cast<TqFloat>( uDiceSize ) )
						 * ( m_auKnots[ m_cuVerts ] - m_auKnots[ m_uOrder - 1 ] )
						 + m_auKnots[ m_uOrder - 1 ];

			pData->SetPoint( static_cast<CqVector3D>( Evaluate( su, sv ) ), igrid );
		}
	}
}


//---------------------------------------------------------------------
/** Generate the vertex normals if not specified.
 */

void CqSurfaceNURBS::GenerateGeometricNormals( TqInt uDiceSize, TqInt vDiceSize, IqShaderData* pNormals )
{
	// Get the handedness of the coordinate system (at the time of creation) and
	// the coordinate system specified, to check for normal flipping.
	TqInt O = pAttributes() ->GetIntegerAttribute("System", "Orientation")[0];

	CqVector3D	N;
	TqInt iv, iu;
	for ( iv = 0; iv <= vDiceSize; iv++ )
	{
		TqFloat sv = ( static_cast<TqFloat>( iv ) / static_cast<TqFloat>( vDiceSize ) )
					 * ( m_avKnots[ m_cvVerts ] - m_avKnots[ m_vOrder - 1 ] )
					 + m_avKnots[ m_vOrder - 1 ];
		for ( iu = 0; iu <= uDiceSize; iu++ )
		{
			TqFloat su = ( static_cast<TqFloat>( iu ) / static_cast<TqFloat>( uDiceSize ) )
						 * ( m_auKnots[ m_cuVerts ] - m_auKnots[ m_uOrder - 1 ] )
						 + m_auKnots[ m_uOrder - 1 ];
			TqInt igrid = ( iv * ( uDiceSize + 1 ) ) + iu;
			N = EvaluateNormal( su, sv );
			N = ( O == OrientationLH )? N : -N;
			pNormals->SetNormal( N, igrid );
		}
	}
}


//---------------------------------------------------------------------
/** Split the patch into smaller patches.
 */

TqInt CqSurfaceNURBS::Split( std::vector<CqBasicSurface*>& aSplits )
{
	TqInt cSplits = 0;

	if( fPatchMesh() )
	{
		std::vector<CqSurfaceNURBS*> S;

		SubdivideSegments( S );
		TqInt i;
		for( i = 0; i < S.size(); i++ )
		{
			S[ i ]->SetSurfaceParameters( *this );
			S[ i ]->m_fDiceable = TqTrue;
			S[ i ]->m_SplitDir = m_SplitDir;
			S[ i ]->m_EyeSplitCount = m_EyeSplitCount;
			S[ i ]->AddRef();
			aSplits.push_back( S[ i ] );
		}
		return( i );
	}

	// Split the surface in u or v
	CqSurfaceNURBS * pNew1;
	CqSurfaceNURBS * pNew2;

	// If this primitive is being split because it spans the e and hither planes, then
	// we should split in both directions to ensure we overcome the crossing.
	if ( m_SplitDir == SplitDir_U || !m_fDiceable )
		uSubdivide( pNew1, pNew2 );
	else
		vSubdivide( pNew1, pNew2 );

	pNew1->SetSurfaceParameters( *this );
	pNew2->SetSurfaceParameters( *this );
	pNew1->m_fDiceable = TqTrue;
	pNew2->m_fDiceable = TqTrue;
	pNew1->m_SplitDir = m_SplitDir;
	pNew2->m_SplitDir = m_SplitDir;
	pNew1->m_EyeSplitCount = m_EyeSplitCount;
	pNew2->m_EyeSplitCount = m_EyeSplitCount;
	pNew1->AddRef();
	pNew2->AddRef();

	if ( !m_fDiceable )
	{
		CqSurfaceNURBS * pNew3;
		CqSurfaceNURBS * pNew4;

		if ( m_SplitDir == SplitDir_U )
			pNew1->vSubdivide( pNew3, pNew4 );
		else
			pNew1->uSubdivide( pNew3, pNew4 );

		pNew3->SetSurfaceParameters( *this );
		pNew4->SetSurfaceParameters( *this );
		pNew3->m_fDiceable = TqTrue;
		pNew4->m_fDiceable = TqTrue;
		pNew3->m_EyeSplitCount = m_EyeSplitCount;
		pNew4->m_EyeSplitCount = m_EyeSplitCount;
		pNew3->AddRef();
		pNew4->AddRef();

		aSplits.push_back( pNew3 );
		aSplits.push_back( pNew4 );

		cSplits += 2;

		if ( m_SplitDir == SplitDir_U )
			pNew2->vSubdivide( pNew3, pNew4 );
		else
			pNew2->uSubdivide( pNew3, pNew4 );

		pNew3->SetSurfaceParameters( *this );
		pNew4->SetSurfaceParameters( *this );
		pNew3->m_fDiceable = TqTrue;
		pNew4->m_fDiceable = TqTrue;
		pNew3->m_EyeSplitCount = m_EyeSplitCount;
		pNew4->m_EyeSplitCount = m_EyeSplitCount;
		pNew3->AddRef();
		pNew4->AddRef();

		aSplits.push_back( pNew3 );
		aSplits.push_back( pNew4 );

		cSplits += 2;

		pNew1->Release();
		pNew2->Release();
	}
	else
	{
		aSplits.push_back( pNew1 );
		aSplits.push_back( pNew2 );

		cSplits += 2;
	}

	return ( cSplits );
}

//---------------------------------------------------------------------
/** Return whether or not the patch is diceable
 */

TqBool	CqSurfaceNURBS::Diceable()
{
	// If the cull check showed that the primitive cannot be diced due to crossing the e and hither planes,
	// then we can return immediately.
	if ( !m_fDiceable )
		return ( TqFalse );

	// Otherwise we should continue to try to find the most advantageous split direction, OR the dice size.
	// Convert the control hull to raster space.
	CqVector2D * avecHull = new CqVector2D[ m_cuVerts * m_cvVerts ];
	TqUint i;
	TqInt gridsize;

	const TqInt* poptGridSize = QGetRenderContext() ->optCurrent().GetIntegerOption( "limits", "gridsize" );

	TqInt m_XBucketSize = 16;
	TqInt m_YBucketSize = 16;
	const TqInt* poptBucketSize = QGetRenderContext() ->optCurrent().GetIntegerOption( "limits", "bucketsize" );
	if ( poptBucketSize != 0 )
	{
		m_XBucketSize = poptBucketSize[ 0 ];
		m_YBucketSize = poptBucketSize[ 1 ];
	}
	TqFloat ShadingRate = pAttributes() ->GetFloatAttribute("System", "ShadingRate")[0];

	if ( poptGridSize )
		gridsize = poptGridSize[ 0 ];
	else
		gridsize = static_cast<TqInt>( m_XBucketSize * m_XBucketSize / ShadingRate );

	for ( i = 0; i < m_cuVerts*m_cvVerts; i++ )
		avecHull[ i ] = QGetRenderContext() ->matSpaceToSpace( "camera", "raster", CqMatrix(), pTransform() ->matObjectToWorld() ) * P() [ i ];

	// Now work out the longest continuous line in raster space for u and v.
	TqFloat uLen = 0;
	TqFloat vLen = 0;
	TqFloat MaxuLen = 0;
	TqFloat MaxvLen = 0;

	TqUint v;
	for ( v = 0; v < m_cvVerts; v++ )
	{
		TqUint u;
		for ( u = 0; u < m_cuVerts - 1; u++ )
			uLen += CqVector2D( avecHull[ ( v * m_cuVerts ) + u + 1 ] - avecHull[ ( v * m_cuVerts ) + u ] ).Magnitude();
		if ( uLen > MaxuLen ) MaxuLen = uLen;
		uLen = 0;
	}

	TqUint u;
	for ( u = 0; u < m_cuVerts; u++ )
	{
		for ( v = 0; v < m_cvVerts - 1; v++ )
			vLen += CqVector2D( avecHull[ ( ( v + 1 ) * m_cuVerts ) + u ] - avecHull[ ( v * m_cuVerts ) + u ] ).Magnitude();
		if ( vLen > MaxvLen ) MaxvLen = vLen;
		vLen = 0;
	}

	if ( MaxvLen > 255 || MaxuLen > 255 )
	{
		m_SplitDir = ( MaxuLen > MaxvLen ) ? SplitDir_U : SplitDir_V;
		delete[] ( avecHull );
		return ( TqFalse );
	}

	ShadingRate = static_cast<TqFloat>( sqrt( ShadingRate ) );
	MaxuLen /= ShadingRate;
	MaxvLen /= ShadingRate;
	m_uDiceSize = static_cast<TqUint>( MAX( ROUND( MaxuLen ), 1 ) );
	m_vDiceSize = static_cast<TqUint>( MAX( ROUND( MaxvLen ), 1 ) );

	// Ensure power of 2 to avoid cracking
	m_uDiceSize = CEIL_POW2(m_uDiceSize);
	m_vDiceSize = CEIL_POW2(m_vDiceSize);

	TqFloat Area = m_uDiceSize * m_vDiceSize;

	if ( MaxuLen < FLT_EPSILON || MaxvLen < FLT_EPSILON )
	{
		m_fDiscard = TqTrue;
		delete[] ( avecHull );
		return ( TqFalse );
	}


	delete[] ( avecHull );
	if ( fabs( Area ) > gridsize )
	{
		m_SplitDir = ( MaxuLen > MaxvLen ) ? SplitDir_U : SplitDir_V;
		return ( TqFalse );
	}
	else
		return ( TqTrue );
}


//---------------------------------------------------------------------
/** Transform the patch by the specified matrix.
 */

void	CqSurfaceNURBS::Transform( const CqMatrix& matTx, const CqMatrix& matITTx, const CqMatrix& matRTx )
{
	// Tansform the control hull by the specified matrix.
	TqUint i;
	for ( i = 0; i < P().Size(); i++ )
		P() [ i ] = matTx * P() [ i ];
}


//---------------------------------------------------------------------
/** Determine the segment count for the specified trim curve to make each segment the appropriate size
 *  for the current shading rate.
 */

TqInt	CqSurfaceNURBS::TrimDecimation( const CqTrimCurve& Curve )
{
	TqFloat Len = 0;
	TqFloat MaxLen = 0;
	TqInt cSegments = 0;
	CqMatrix matCtoR = QGetRenderContext() ->matSpaceToSpace( "camera", "raster", CqMatrix(), pTransform() ->matObjectToWorld() );

	TqUint iTrimCurvePoint;
	for ( iTrimCurvePoint = 0; iTrimCurvePoint < Curve.cVerts() - 1; iTrimCurvePoint++ )
	{
		// Get the u,v of the current point.
		TqFloat u, v;
		CqVector3D vecCP;
		vecCP = Curve.CP( iTrimCurvePoint );
		u = vecCP.x();
		v = vecCP.y();

		// Get the u,v of the next point.
		TqFloat u2, v2;
		vecCP = Curve.CP( iTrimCurvePoint + 1 );
		u2 = vecCP.x();
		v2 = vecCP.y();

		CqVector3D vecP = Evaluate( u, v );
		vecP = matCtoR * vecP;
		CqVector3D vecP2 = Evaluate( u2, v2 );
		vecP2 = matCtoR * vecP2;

		Len = ( vecP2 - vecP ).Magnitude();
		if ( Len > MaxLen ) MaxLen = Len;
		cSegments++;
	}
	TqFloat ShadingRate = pAttributes() ->GetFloatAttribute("System", "ShadingRate")[0];
	ShadingRate = static_cast<TqFloat>( sqrt( ShadingRate ) );
	MaxLen /= ShadingRate;

	TqInt SplitCount = static_cast<TqUint>( MAX( MaxLen, 1 ) );

	return ( SplitCount * cSegments );
}



void CqSurfaceNURBS::OutputMesh()
{
	TqUint Granularity = 30;  // Controls the number of steps in u and v


	std::vector<CqSurfaceNURBS*>	S(1);
	S[ 0 ] = this;
//	Decompose(S);

	// Save the grid as a .raw file.
	FILE* fp = fopen( "NURBS.RAW", "w" );

	TqUint s;
	for ( s = 0; s < S.size(); s++ )
	{
		fprintf( fp, "Surface_%d\n", s );
		std::vector<std::vector<CqVector3D> > aaPoints( Granularity + 1 );
		TqUint p;
		for ( p = 0; p <= Granularity; p++ ) aaPoints[ p ].resize( Granularity + 1 );


		// Compute points on curve

		TqUint i;
		for ( i = 0; i <= Granularity; i++ )
		{
			TqFloat v = ( static_cast<TqFloat>( i ) / static_cast<TqFloat>( Granularity ) )
			            * ( S[ s ]->m_avKnots[ S[ s ]->m_cvVerts ] - S[ s ]->m_avKnots[ S[ s ]->m_vOrder - 1 ] )
			            + S[ s ]->m_avKnots[ S[ s ]->m_vOrder - 1 ];

			TqUint j;
			for ( j = 0; j <= Granularity; j++ )
			{
				TqFloat u = ( static_cast<TqFloat>( j ) / static_cast<TqFloat>( Granularity ) )
				            * ( S[ s ]->m_auKnots[ S[ s ]->m_cuVerts ] - S[ s ]->m_auKnots[ S[ s ]->m_uOrder - 1 ] )
				            + S[ s ]->m_auKnots[ S[ s ]->m_uOrder - 1 ];

				aaPoints[ i ][ j ] = S[ s ]->Evaluate( u, v );
			}
		}


		for ( i = 0; i < Granularity; i++ )
		{
			TqUint j;
			for ( j = 0; j < Granularity; j++ )
			{
				fprintf( fp, "%f %f %f %f %f %f %f %f %f\n",
				         aaPoints[ i ][ j ].x(), aaPoints[ i ][ j ].y(), aaPoints[ i ][ j ].z(),
				         aaPoints[ i + 1 ][ j + 1 ].x(), aaPoints[ i + 1 ][ j + 1 ].y(), aaPoints[ i + 1 ][ j + 1 ].z(),
				         aaPoints[ i + 1 ][ j ].x(), aaPoints[ i + 1 ][ j ].y(), aaPoints[ i + 1 ][ j ].z() );
				fprintf( fp, "%f %f %f %f %f %f %f %f %f\n",
				         aaPoints[ i ][ j ].x(), aaPoints[ i ][ j ].y(), aaPoints[ i ][ j ].z(),
				         aaPoints[ i ][ j + 1 ].x(), aaPoints[ i ][ j + 1 ].y(), aaPoints[ i ][ j + 1 ].z(),
				         aaPoints[ i + 1 ][ j + 1 ].x(), aaPoints[ i + 1 ][ j + 1 ].y(), aaPoints[ i + 1 ][ j + 1 ].z() );
			}
		}
	}
	fclose( fp );
}


void CqSurfaceNURBS::Output( const char* name )
{
	TqUint i;

	// Save the grid as a .out file.
	FILE * fp = fopen( name, "w" );
	fputs( "NuPatch ", fp );

	fprintf( fp, "%d ", m_cuVerts );
	fprintf( fp, "%d ", m_uOrder );

	fputs( "[", fp );
	for ( i = 0; i < m_auKnots.size(); i++ )
		fprintf( fp, "%f ", m_auKnots[ i ] );
	fputs( "]", fp );
	fprintf( fp, "%f %f ", 0.0f, 1.0f );

	fprintf( fp, "%d ", m_cvVerts );
	fprintf( fp, "%d ", m_vOrder );

	fputs( "[", fp );
	for ( i = 0; i < m_avKnots.size(); i++ )
		fprintf( fp, "%f ", m_avKnots[ i ] );
	fputs( "]", fp );
	fprintf( fp, "%f %f ", 0.0f, 1.0f );

	fputs( "\"Pw\" [", fp );
	for ( i = 0; i < P().Size(); i++ )
		fprintf( fp, "%f %f %f %f ", P() [ i ].x(), P() [ i ].y(), P() [ i ].z(), P() [ i ].h() );
	fputs( "]\n", fp );

	fclose( fp );
}


void CqSurfaceNURBS::SetDefaultPrimitiveVariables( TqBool bUseDef_st )
{
	TqInt bUses = Uses();

	if ( USES( bUses, EnvVars_u ) )
	{
		AddPrimitiveVariable(new CqParameterTypedVarying<TqFloat, type_float, TqFloat>("u") );
		u()->SetSize( cVarying() );

		TqFloat uinc = ( m_umax - m_umin ) / (cuSegments());

		TqInt c,r;
		TqInt i = 0;
		for( c = 0; c < cvSegments()+1; c++ )
		{
			TqFloat uval = m_umin;
			for( r = 0; r < cuSegments()+1; r++ )
			{
				u()->pValue() [ i++ ] = uval;
				uval += uinc;
			}
		}
	}

	if ( USES( bUses, EnvVars_v ) )
	{
		AddPrimitiveVariable(new CqParameterTypedVarying<TqFloat, type_float, TqFloat>("v") );
		v()->SetSize( cVarying() );

		TqFloat vinc = ( m_vmax - m_vmin ) / (cvSegments());
		TqFloat vval = m_vmin;

		TqInt c,r;
		TqInt i = 0;
		for( c = 0; c < cvSegments()+1; c++ )
		{
			for( r = 0; r < cuSegments()+1; r++ )
				v()->pValue() [ i++ ] = vval;
			vval += vinc;
		}
	}

	const TqFloat* pTC = pAttributes() ->GetFloatAttribute("System", "TextureCoordinates");
	CqVector2D st1( pTC[0], pTC[1]);
	CqVector2D st2( pTC[2], pTC[3]);
	CqVector2D st3( pTC[4], pTC[5]);
	CqVector2D st4( pTC[6], pTC[7]);

	if( USES( bUses, EnvVars_s ) && !bHass() && bUseDef_st)
	{
		AddPrimitiveVariable(new CqParameterTypedVarying<TqFloat, type_float, TqFloat>("s") );
		s()->SetSize( cVarying() );

		TqInt c,r;
		TqInt i = 0;
		for( c = 0; c < cvSegments(); c++ )
		{
			TqFloat v = ( 1.0f / cvSegments()+1 ) * c;
			for( r = 0; r < cuSegments()+1; r++ )
			{
				TqFloat u = ( 1.0f / cuSegments() ) * r;
				s()->pValue() [ i++ ] = BilinearEvaluate(st1.x(), st2.x(), st3.x(), st4.x(), u,v );
			}
		}
	}

	if( USES( bUses, EnvVars_t ) && !bHast() && bUseDef_st )
	{
		AddPrimitiveVariable(new CqParameterTypedVarying<TqFloat, type_float, TqFloat>("t") );
		t()->SetSize( cVarying() );

		TqInt c,r;
		TqInt i = 0;
		for( c = 0; c < cvSegments(); c++ )
		{
			TqFloat v = ( 1.0f / cvSegments() ) * c;
			for( r = 0; r < cuSegments()+1; r++ )
			{
				TqFloat u = ( 1.0f / cuSegments()+1 ) * r;
				t()->pValue() [ i++ ] = BilinearEvaluate(st1.y(), st2.y(), st3.y(), st4.y(), u,v );
			}
		}
	}

}


/** Split the NURBS surface into B-Spline (sub) surfaces
 */

void CqSurfaceNURBS::SubdivideSegments(std::vector<CqSurfaceNURBS*>& S)
{
	TqInt uSplits = cuSegments();
	TqInt vSplits = cvSegments();

	// Resize the array to hold the aplit surfaces.
	S.resize( uSplits * vSplits );
	
	TqInt iu, iv;

	// An array to hold the split points in u and v, fill in the first one for us.
	std::vector<TqInt> uSplitPoint(uSplits+1), vSplitPoint(vSplits+1);
	uSplitPoint[0] = vSplitPoint[0] = 0;

	// Refine the knot vectors as appropriate to generate the required split points in u
	for ( iu = 1; iu < uSplits; iu++ )
	{
		TqFloat su = ( static_cast<TqFloat>( iu ) / static_cast<TqFloat>( uSplits ) )
					 * ( m_auKnots[ m_cuVerts ] - m_auKnots[ m_uOrder - 1 ] )
					 + m_auKnots[ m_uOrder - 1 ];

		TqUint extra = 0L;
		TqUint last = m_cuVerts + m_uOrder - 1;
		TqFloat midVal = su;
		TqUint middex = FindSpanU( midVal );
		
		// Search forward and backward to see if multiple knot is already there
		TqUint i = 0;
		TqUint same = 0L;
		if ( auKnots()[ middex ] == midVal )
		{
			i = middex + 1L;
			same = 1L;
			while ( ( i < last ) && ( auKnots()[ i ] == midVal ) )
			{
				i++;
				same++;
			}

			i = middex - 1L;
			while ( ( i > 0L ) && ( auKnots()[ i ] == midVal ) )
			{
				i--;
				middex--;	// middex is start of multiple knot
				same++;
			}
		}

		if ( i <= 0L )         	    // No knot in middle, must create it
		{
			middex = 0;
			while ( auKnots()[ middex + 1L ] < midVal )
				middex++;
			same = 0L;
		}

		extra = m_uOrder - same;
		std::vector<TqFloat> anewKnots( extra );

		if ( same < m_uOrder )         	    // Must add knots
		{
			for ( i = 0; i < extra; i++ )
				anewKnots[ i ] = midVal;
		}

		uSplitPoint[ iu ] = ( extra < m_uOrder ) ? middex - 1L : middex;
		RefineKnotU( anewKnots );
	}

	// Refine the knot vectors as appropriate to generate the required split points in v
	for ( iv = 1; iv < vSplits; iv++ )
	{
		TqFloat sv = ( static_cast<TqFloat>( iv ) / static_cast<TqFloat>( vSplits ) )
					 * ( m_avKnots[ m_cvVerts ] - m_avKnots[ m_vOrder - 1 ] )
					 + m_avKnots[ m_vOrder - 1 ];

		TqUint extra = 0L;
		TqUint last = m_cvVerts + m_vOrder - 1;
		TqFloat midVal = sv;
		TqUint middex = FindSpanV( midVal );
		// Search forward and backward to see if multiple knot is already there
		TqUint i = 0;
		TqUint same = 0L;
		if ( avKnots()[ middex ] == midVal )
		{
			i = middex + 1L;
			same = 1L;
			while ( ( i < last ) && ( avKnots()[ i ] == midVal ) )
			{
				i++;
				same++;
			}

			i = middex - 1L;
			while ( ( i > 0L ) && ( avKnots()[ i ] == midVal ) )
			{
				i--;
				middex--;	// middex is start of multiple knot
				same++;
			}
		}

		if ( i <= 0L )         	    // No knot in middle, must create it
		{
			middex = 0;
			while ( avKnots()[ middex + 1L ] < midVal )
				middex++;
			same = 0L;
		}

		extra = m_vOrder - same;
		std::vector<TqFloat> anewKnots( extra );

		if ( same < m_vOrder )         	    // Must add knots
		{
			for ( i = 0; i < extra; i++ )
				anewKnots[ i ] = midVal;
		}

		vSplitPoint[ iv ] = ( extra < m_vOrder ) ? middex - 1L : middex;
		RefineKnotV( anewKnots );
	}

	// Fill in the end points for the last split.
	uSplitPoint[uSplits] = m_cuVerts-1;
	vSplitPoint[vSplits] = m_cvVerts-1;

	// Now go over the surface, generating the new patches at the split points in the arrays.
	TqInt uPatch, vPatch;
	// Initialise the offset for the first segment.
	TqInt vOffset = 0;
	for( vPatch = 0; vPatch < vSplits; vPatch++ )
	{
		// Initialise the offset for the first segment.
		TqInt uOffset = 0;
		// Get the end of the next segment in v.
		TqInt vEnd = vSplitPoint[ vPatch+1 ];
		
		// Loop across u rows, filling points and knot vectors.
		for( uPatch = 0; uPatch < uSplits; uPatch++ )
		{
			TqInt uEnd = uSplitPoint[ uPatch+1 ];

			// The index of the patch we are working on.
			TqInt iS = ( vPatch * uSplits ) + uPatch;
			S[iS] = new CqSurfaceNURBS(*this);
			S[iS]->SetfPatchMesh( TqFalse );
			// Initialise it to the same orders as us, with the calculated control point densities.
			S[iS]->Init( m_uOrder, m_vOrder, (uEnd+1)-uOffset, (vEnd+1)-vOffset );

			// Copy the control points out of the our storage.
			TqInt iPu, iPv;
			for( iPv = 0; iPv <= vEnd; iPv++ )
			{
				TqInt iPIndex = ( ( vOffset + iPv ) * m_cuVerts ) + uOffset;
				for( iPu = 0; iPu <= uEnd; iPu++ )
				{
					TqInt iSP = ( iPv * S[iS]->cuVerts() ) + iPu; 
					S[iS]->P()[ iSP ] = P()[ iPIndex++ ];
				}
			}
			
			// Copy the knot vectors 
			TqInt iuK, ivK;
			for( iuK = 0; iuK < S[iS]->uOrder() + S[iS]->cuVerts(); iuK++ )
				S[iS]->auKnots()[iuK] = auKnots()[ uOffset + iuK ];
			for( ivK = 0; ivK < S[iS]->vOrder() + S[iS]->cvVerts(); ivK++ )
				S[iS]->avKnots()[ivK] = avKnots()[ vOffset + ivK ];

			// Set the offset to just after the end of this segment.
			uOffset = uEnd + 1;
		}
		// Set the offset to just after the end of this segment.
		vOffset = vEnd + 1;
	}

	// Now setup any user variables on the segments.
	TqInt irow, icol;
	TqInt nuSegs = uSplits;
	TqInt nvSegs = vSplits;
	for( icol = 0; icol < nvSegs; icol++ )
	{
		for( irow = 0; irow < nuSegs; irow++ )
		{
			TqInt iPatch = ( icol * nvSegs ) + irow;
			TqInt iA = ( icol * ( nuSegs + 1 ) ) + irow;
			TqInt iB = ( icol * ( nuSegs + 1 ) ) + irow + 1;
			TqInt iC = ( ( icol + 1 ) * ( nuSegs + 1 ) ) + irow;
			TqInt iD = ( ( icol + 1 ) * ( nuSegs + 1 ) ) + irow + 1;

			std::vector<CqParameter*>::iterator iUP;
			for( iUP = aUserParams().begin(); iUP != aUserParams().end(); iUP++ )
			{
				switch( (*iUP)->Type() )
				{
					case type_integer:
					{
						CqParameterTyped<TqInt, TqFloat>* pUPV = static_cast<CqParameterTyped<TqInt, TqFloat>*>( (*iUP) );
						CqParameterTyped<TqInt, TqFloat>* pNUPV = static_cast<CqParameterTyped<TqInt, TqFloat>*>( (*iUP)->Clone() );
						S[ iPatch ]->AddPrimitiveVariable(pNUPV);
						pNUPV->SetSize(4);
						*pNUPV->pValue( 0 ) = *pUPV->pValue( iA );
						*pNUPV->pValue( 1 ) = *pUPV->pValue( iB );
						*pNUPV->pValue( 2 ) = *pUPV->pValue( iC );
						*pNUPV->pValue( 3 ) = *pUPV->pValue( iD );
					}
					break;

					case type_float:
					{
						CqParameterTyped<TqFloat, TqFloat>* pUPV = static_cast<CqParameterTyped<TqFloat, TqFloat>*>( (*iUP) );
						CqParameterTyped<TqFloat, TqFloat>* pNUPV = static_cast<CqParameterTyped<TqFloat, TqFloat>*>( (*iUP)->Clone() );
						S[ iPatch ]->AddPrimitiveVariable(pNUPV);
						pNUPV->SetSize(4);
						*pNUPV->pValue( 0 ) = *pUPV->pValue( iA );
						*pNUPV->pValue( 1 ) = *pUPV->pValue( iB );
						*pNUPV->pValue( 2 ) = *pUPV->pValue( iC );
						*pNUPV->pValue( 3 ) = *pUPV->pValue( iD );
					}
					break;

					case type_color:
					{
						CqParameterTyped<CqColor, CqColor>* pUPV = static_cast<CqParameterTyped<CqColor, CqColor>*>( (*iUP) );
						CqParameterTyped<CqColor, CqColor>* pNUPV = static_cast<CqParameterTyped<CqColor, CqColor>*>( (*iUP)->Clone() );
						S[ iPatch ]->AddPrimitiveVariable(pNUPV);
						pNUPV->SetSize(4);
						*pNUPV->pValue( 0 ) = *pUPV->pValue( iA );
						*pNUPV->pValue( 1 ) = *pUPV->pValue( iB );
						*pNUPV->pValue( 2 ) = *pUPV->pValue( iC );
						*pNUPV->pValue( 3 ) = *pUPV->pValue( iD );
					}
					break;

					case type_point:
					case type_normal:
					case type_vector:
					{
						CqParameterTyped<CqVector3D, CqVector3D>* pUPV = static_cast<CqParameterTyped<CqVector3D, CqVector3D>*>( (*iUP) );
						CqParameterTyped<CqVector3D, CqVector3D>* pNUPV = static_cast<CqParameterTyped<CqVector3D, CqVector3D>*>( (*iUP)->Clone() );
						S[ iPatch ]->AddPrimitiveVariable(pNUPV);
						pNUPV->SetSize(4);
						*pNUPV->pValue( 0 ) = *pUPV->pValue( iA );
						*pNUPV->pValue( 1 ) = *pUPV->pValue( iB );
						*pNUPV->pValue( 2 ) = *pUPV->pValue( iC );
						*pNUPV->pValue( 3 ) = *pUPV->pValue( iD );
					}
					break;

					case type_hpoint:
					{
						CqParameterTyped<CqVector4D, CqVector3D>* pUPV = static_cast<CqParameterTyped<CqVector4D, CqVector3D>*>( (*iUP) );
						CqParameterTyped<CqVector4D, CqVector3D>* pNUPV = static_cast<CqParameterTyped<CqVector4D, CqVector3D>*>( (*iUP)->Clone() );
						S[ iPatch ]->AddPrimitiveVariable(pNUPV);
						pNUPV->SetSize(4);
						*pNUPV->pValue( 0 ) = *pUPV->pValue( iA );
						*pNUPV->pValue( 1 ) = *pUPV->pValue( iB );
						*pNUPV->pValue( 2 ) = *pUPV->pValue( iC );
						*pNUPV->pValue( 3 ) = *pUPV->pValue( iD );
					}
					break;

					case type_string:
					{
						CqParameterTyped<CqString, CqString>* pUPV = static_cast<CqParameterTyped<CqString, CqString>*>( (*iUP) );
						CqParameterTyped<CqString, CqString>* pNUPV = static_cast<CqParameterTyped<CqString, CqString>*>( (*iUP)->Clone() );
						S[ iPatch ]->AddPrimitiveVariable(pNUPV);
						pNUPV->SetSize(4);
						*pNUPV->pValue( 0 ) = *pUPV->pValue( iA );
						*pNUPV->pValue( 1 ) = *pUPV->pValue( iB );
						*pNUPV->pValue( 2 ) = *pUPV->pValue( iC );
						*pNUPV->pValue( 3 ) = *pUPV->pValue( iD );
					}
					break;

					case type_matrix:
					{
						CqParameterTyped<CqMatrix, CqMatrix>* pUPV = static_cast<CqParameterTyped<CqMatrix, CqMatrix>*>( (*iUP) );
						CqParameterTyped<CqMatrix, CqMatrix>* pNUPV = static_cast<CqParameterTyped<CqMatrix, CqMatrix>*>( (*iUP)->Clone() );
						S[ iPatch ]->AddPrimitiveVariable(pNUPV);
						pNUPV->SetSize(4);
						*pNUPV->pValue( 0 ) = *pUPV->pValue( iA );
						*pNUPV->pValue( 1 ) = *pUPV->pValue( iB );
						*pNUPV->pValue( 2 ) = *pUPV->pValue( iC );
						*pNUPV->pValue( 3 ) = *pUPV->pValue( iD );
					}
					break;
				}
			}
		}
	}
}



//-------------------------------------------------------

END_NAMESPACE( Aqsis )

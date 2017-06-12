/*
 * DIPlib 3.0
 * This file contains definitions of functions that implement the convolution.
 *
 * (c)2017, Cris Luengo.
 * Based on original DIPlib code: (c)1995-2014, Delft University of Technology.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "diplib.h"
#include "diplib/linear.h"
#include "diplib/transform.h"
#include "diplib/framework.h"
#include "diplib/overload.h"

namespace dip {

namespace {

enum class FilterSymmetry {
      GENERAL,
      EVEN,
      ODD,
      D_EVEN,
      D_ODD
};

struct InternOneDimensionalFilter {
   FloatArray const& filter;
   dip::uint size = 0;
   dip::uint origin = 0;
   FilterSymmetry symmetry = FilterSymmetry::GENERAL;
   InternOneDimensionalFilter( OneDimensionalFilter const& in ) : filter( in.filter ) {
      size = filter.size();
      if( size != 0 ) {
         if( in.symmetry.empty() || ( in.symmetry == "general" ) ) {
            symmetry = FilterSymmetry::GENERAL;
         } else if( in.symmetry == "even" ) {
            symmetry = FilterSymmetry::EVEN;
            size += size - 1;
         } else if( in.symmetry == "odd" ) {
            symmetry = FilterSymmetry::ODD;
            size += size - 1;
         } else if( in.symmetry == "d-even" ) {
            symmetry = FilterSymmetry::D_EVEN;
            size += size;
         } else if( in.symmetry == "d-odd" ) {
            symmetry = FilterSymmetry::D_ODD;
            size += size;
         } else {
            DIP_THROW( "Symmetry string not recognized: " + in.symmetry );
         }
         if( in.origin < 0 ) {
            origin = size / 2;
         } else {
            origin = static_cast< dip::uint >( in.origin );
            DIP_THROW_IF( origin >= size, "Origin outside of filter" );
         }
      }
   }
};

using InternOneDimensionalFilterArray = std::vector< InternOneDimensionalFilter >;

template< typename TPI >
class SeparableConvolutionLineFilter : public Framework::SeparableLineFilter {
   public:
      SeparableConvolutionLineFilter( InternOneDimensionalFilterArray const& filter ) : filter_( filter ) {}
      virtual void Filter( Framework::SeparableLineFilterParameters const& params ) override {
         TPI* in = static_cast< TPI* >( params.inBuffer.buffer );
         dip::uint length = params.inBuffer.length;
         dip::sint inStride = params.inBuffer.stride;
         TPI* out = static_cast< TPI* >( params.outBuffer.buffer );
         dip::sint outStride = params.outBuffer.stride;
         dip::uint procDim = 0;
         if( filter_.size() > 1 ) {
            procDim = params.dimension;
            if( params.tensorToSpatial ) {
               DIP_ASSERT( procDim > 0 );
               --procDim;
            }
         }
         FloatArray const& filter = filter_[ procDim ].filter;
         dip::uint origin = filter_[ procDim ].origin;
         dip::uint filterSize = filter_[ procDim ].size;
         dip::uint fsh = filter.size() - 1;
         switch( filter_[ procDim ].symmetry ) {
            case FilterSymmetry::GENERAL:
               in += static_cast< dip::sint >( origin ) * inStride;
               for( dip::uint ii = 0; ii < length; ++ii ) {
                  TPI sum = 0;
                  TPI* in_t = in;
                  for( dip::uint jj = 0; jj < filterSize; ++jj ) {
                     sum += static_cast< FloatType< TPI >>( filter[ jj ] ) * *in_t;
                     in_t -= inStride;
                  }
                  *out = sum;
                  in += inStride;
                  out += outStride;
               }
               break;
            case FilterSymmetry::EVEN: // Always an odd-sized filter
               in += static_cast< dip::sint >( origin - fsh ) * inStride;
               for( dip::uint ii = 0; ii < length; ++ii ) {
                  TPI* in_r = in;
                  TPI sum = static_cast< FloatType< TPI >>( filter[ fsh ] ) * *in_r;
                  TPI* in_l = in_r - inStride;
                  in_r += inStride;
                  dip::uint jj = fsh;
                  while( jj > 0 ) {
                     --jj;
                     sum += static_cast< FloatType< TPI >>( filter[ jj ] ) * ( *in_r + *in_l );
                     in_l -= inStride;
                     in_r += inStride;
                  }
                  *out = sum;
                  in += inStride;
                  out += outStride;
               }
               break;
            case FilterSymmetry::ODD: // Always an odd-sized filter
               in += static_cast< dip::sint >( origin - fsh ) * inStride;
               for( dip::uint ii = 0; ii < length; ++ii ) {
                  TPI* in_r = in;
                  TPI sum = static_cast< FloatType< TPI >>( filter[ fsh ] ) * *in_r;
                  TPI* in_l = in_r - inStride;
                  in_r += inStride;
                  dip::uint jj = fsh;
                  while( jj > 0 ) {
                     --jj;
                     sum += static_cast< FloatType< TPI >>( filter[ jj ] ) * ( *in_r - *in_l );
                     in_l -= inStride;
                     in_r += inStride;
                  }
                  *out = sum;
                  in += inStride;
                  out += outStride;
               }
               break;
            case FilterSymmetry::D_EVEN: // Always an even-sized filter
               in += static_cast< dip::sint >( origin - fsh ) * inStride;
               ++fsh;
               for( dip::uint ii = 0; ii < length; ++ii ) {
                  TPI* in_r = in;
                  TPI sum = 0;
                  TPI* in_l = in_r - inStride;
                  dip::uint jj = fsh;
                  while( jj > 0 ) {
                     --jj;
                     sum += static_cast< FloatType< TPI >>( filter[ jj ] ) * ( *in_r + *in_l );
                     in_l -= inStride;
                     in_r += inStride;
                  }
                  *out = sum;
                  in += inStride;
                  out += outStride;
               }
               break;
            case FilterSymmetry::D_ODD: // Always an even-sized filter
               in += static_cast< dip::sint >( origin - fsh ) * inStride;
               ++fsh;
               for( dip::uint ii = 0; ii < length; ++ii ) {
                  TPI* in_r = in;
                  TPI sum = 0;
                  TPI* in_l = in_r - inStride;
                  dip::uint jj = fsh;
                  while( jj > 0 ) {
                     --jj;
                     sum += static_cast< FloatType< TPI >>( filter[ jj ] ) * ( *in_r - *in_l );
                     in_l -= inStride;
                     in_r += inStride;
                  }
                  *out = sum;
                  in += inStride;
                  out += outStride;
               }
               break;
         }
      }
   private:
      InternOneDimensionalFilterArray const& filter_;
};

inline bool IsMeaninglessFilter( InternOneDimensionalFilter const& filter ) {
   return ( filter.size == 0 ) || (( filter.size == 1 ) && ( filter.filter[ 0 ] == 1.0 ));
}

} // namespace


void SeparableConvolution(
      Image const& in,
      Image& out,
      OneDimensionalFilterArray const& filterArray,
      StringArray const& boundaryCondition,
      BooleanArray process
) {
   DIP_THROW_IF( !in.IsForged(), E::IMAGE_NOT_FORGED );
   dip::uint nDims = in.Dimensionality();
   DIP_THROW_IF( nDims < 1, E::DIMENSIONALITY_NOT_SUPPORTED );
   DIP_THROW_IF(( filterArray.size() != 1 ) && ( filterArray.size() != nDims ), E::ARRAY_ILLEGAL_SIZE );
   InternOneDimensionalFilterArray filterData;
   DIP_START_STACK_TRACE
      for( auto const& f : filterArray ) {
         filterData.emplace_back( f );
      }
   DIP_END_STACK_TRACE
   // Handle `filterArray` and create `border` array
   UnsignedArray border( nDims );
   if( filterData.size() == 1 ) {
      dip::uint sz = filterData[ 0 ].size;
      dip::uint b = filterData[ 0 ].origin;
      b = std::max( b, sz - b - 1 ); // note that b < sz.
      border.fill( b );
   } else {
      for( dip::uint ii = 0; ii < nDims; ++ii ) {
         dip::uint sz = filterData[ ii ].size;
         dip::uint b = filterData[ ii ].origin;
         b = std::max( b, sz - b - 1 ); // note that b < sz.
         border[ ii ] = b;
      }
   }
   // Handle `process` array
   if( process.empty() ) {
      process.resize( nDims, true );
   } else {
      DIP_THROW_IF( process.size() != nDims, E::ARRAY_PARAMETER_WRONG_LENGTH );
   }
   if( filterData.size() == 1 ) {
      if( IsMeaninglessFilter( filterData[ 0 ] )) {
         // Nothing to do for this filter
         process.fill( false );
      }
   } else {
      for( dip::uint ii = 0; ii < nDims; ++ii ) {
         if( IsMeaninglessFilter( filterData[ ii ] ) || ( in.Size( ii ) <= 1 )) {
            process[ ii ] = false;
         }
      }
   }
   DIP_START_STACK_TRACE
      // handle boundary condition array (checks are made in Framework::Separable, no need to repeat them here)
      BoundaryConditionArray bc = StringArrayToBoundaryConditionArray( boundaryCondition );
      // Get callback function
      DataType dtype = DataType::SuggestFlex( in.DataType() );
      std::unique_ptr< Framework::SeparableLineFilter > lineFilter;
      DIP_OVL_NEW_FLEX( lineFilter, SeparableConvolutionLineFilter, ( filterData ), dtype );
      Framework::Separable(
            in,
            out,
            dtype,
            dtype,
            process,
            border,
            bc,
            *lineFilter,
            Framework::Separable_AsScalarImage
      );
   DIP_END_STACK_TRACE
}


void ConvolveFT(
      Image const& in,
      Image const& filter,
      Image& out,
      String const& inRepresentation,
      String const& filterRepresentation,
      String const& outRepresentation
) {
   DIP_THROW_IF( !in.IsForged(), E::IMAGE_NOT_FORGED );
   DIP_THROW_IF( !filter.IsForged(), E::IMAGE_NOT_FORGED );
   bool real = true;
   Image inFT;
   if( inRepresentation == "spatial" ) {
      real &= in.DataType().IsReal();
      FourierTransform( in, inFT );
   } else {
      real = false;
      inFT = in.QuickCopy();
   }
   Image filterFT = filter.QuickCopy();
   if( filterFT.Dimensionality() < in.Dimensionality() ) {
      filterFT.ExpandDimensionality( in.Dimensionality() );
   }
   DIP_THROW_IF( !( filterFT.Sizes() <= in.Sizes() ), E::SIZES_DONT_MATCH ); // Also throws if dimensionalities don't match
   filterFT = filterFT.Pad( in.Sizes() );
   if( filterRepresentation == "spatial" ) {
      real &= filterFT.DataType().IsReal();
      FourierTransform( filterFT, filterFT );
   } else {
      real = false;
   }
   DataType dt = inFT.DataType();
   MultiplySampleWise( inFT, filterFT, out, dt );
   if( outRepresentation == "spatial" ) {
      StringSet options{ "inverse" };
      if( real ) {
         options.insert( "real" );
      }
      FourierTransform( out, out, options );
   }
}


namespace {

template< typename TPI >
class GeneralConvolutionLineFilter : public Framework::FullLineFilter {
   public:
      virtual void Filter( Framework::FullLineFilterParameters const& params ) override {
         TPI* in = static_cast< TPI* >( params.inBuffer.buffer );
         dip::sint inStride = params.inBuffer.stride;
         TPI* out = static_cast< TPI* >( params.outBuffer.buffer );
         dip::sint outStride = params.outBuffer.stride;
         dip::uint length = params.bufferLength;
         PixelTableOffsets const& pixelTable = params.pixelTable;
         std::vector< dfloat > const& weights = pixelTable.Weights();
         for( dip::uint ii = 0; ii < length; ++ii ) {
            TPI sum = 0;
            auto ito = pixelTable.begin();
            auto itw = weights.begin();
            while( !ito.IsAtEnd() ) {
               sum += in[ *ito ] * static_cast< FloatType< TPI >>( *itw );
               ++ito;
               ++itw;
            }
            *out = sum;
            in += inStride;
            out += outStride;
         }
      }
};

} // namespace

void GeneralConvolution(
      Image const& in,
      Image const& c_filter,
      Image& out,
      StringArray const& boundaryCondition
) {
   DIP_THROW_IF( !in.IsForged(), E::IMAGE_NOT_FORGED );
   DIP_THROW_IF( !c_filter.IsForged(), E::IMAGE_NOT_FORGED );
   DIP_START_STACK_TRACE
      Kernel filter{ c_filter };
      filter.Mirror();
      if( c_filter.DataType().IsBinary() ) {
         // For binary filters, apply a uniform filter.
         Uniform( in, out, filter, boundaryCondition );
         return;
      }
      BoundaryConditionArray bc = StringArrayToBoundaryConditionArray( boundaryCondition );
      DataType dtype = DataType::SuggestFlex( in.DataType() );
      std::unique_ptr< Framework::FullLineFilter > lineFilter;
      DIP_OVL_NEW_FLEX( lineFilter, GeneralConvolutionLineFilter, (), dtype );
      Framework::Full(
            in,
            out,
            dtype,
            dtype,
            dtype,
            1,
            bc,
            filter,
            *lineFilter,
            Framework::Full_AsScalarImage
      );
   DIP_END_STACK_TRACE
}


} // namespace dip


#ifdef DIP__ENABLE_DOCTEST
#include "doctest.h"
#include "diplib/statistics.h"
#include "diplib/generation.h"
#include "diplib/iterators.h"

DOCTEST_TEST_CASE("[DIPlib] testing the separable convolution") {
   dip::dfloat meanval = 9563.0;
   dip::Image img{ dip::UnsignedArray{ 80, 6, 5 }, 1, dip::DT_UINT16 };
   {
      DIP_THROW_IF( img.DataType() != dip::DT_UINT16, "Expecting 16-bit unsigned integer image" );
      img.Fill( meanval );
      dip::Random random( 0 );
      dip::GaussianNoise( img, img, random, 500.0 );
   }
   dip::Image out1;
   dip::Image out2;
   // Comparing general to even
   dip::OneDimensionalFilterArray filterArray( 3 );
   filterArray[ 0 ].filter = {
         1.0 / 49.0, 2.0 / 49.0, 3.0 / 49.0, 4.0 / 49.0, 5.0 / 49.0, 6.0 / 49.0, 7.0 / 49.0,
         6.0 / 49.0, 5.0 / 49.0, 4.0 / 49.0, 3.0 / 49.0, 2.0 / 49.0, 1.0 / 49.0
   };
   filterArray[ 0 ].origin = -1;
   filterArray[ 0 ].symmetry = "general";
   dip::SeparableConvolution( img, out1, filterArray, { "periodic" } );
   filterArray[ 0 ].filter = {
         1.0 / 49.0, 2.0 / 49.0, 3.0 / 49.0, 4.0 / 49.0, 5.0 / 49.0, 6.0 / 49.0, 7.0 / 49.0
   };
   filterArray[ 0 ].symmetry = "even";
   dip::SeparableConvolution( img, out2, filterArray, { "periodic" } );
   DOCTEST_CHECK( dip::Mean( out1 - out2 ).As< dip::dfloat >() / meanval == doctest::Approx( 0.0 ));

   // Comparing general to odd
   filterArray[ 0 ].filter = {
         1.0 / 49.0, 2.0 / 49.0, 3.0 / 49.0, 4.0 / 49.0, 5.0 / 49.0, 6.0 / 49.0, 7.0 / 49.0,
         -6.0 / 49.0, -5.0 / 49.0, -4.0 / 49.0, -3.0 / 49.0, -2.0 / 49.0, -1.0 / 49.0
   };
   filterArray[ 0 ].origin = -1;
   filterArray[ 0 ].symmetry = "general";
   dip::SeparableConvolution( img, out1, filterArray, { "periodic" } );
   filterArray[ 0 ].filter = {
         1.0 / 49.0, 2.0 / 49.0, 3.0 / 49.0, 4.0 / 49.0, 5.0 / 49.0, 6.0 / 49.0, 7.0 / 49.0
   };
   filterArray[ 0 ].symmetry = "odd";
   dip::SeparableConvolution( img, out2, filterArray, { "periodic" } );
   DOCTEST_CHECK( dip::Mean( out1 - out2 ).As< dip::dfloat >() / meanval == doctest::Approx( 0.0 ));

   // Comparing general to d-even
   filterArray[ 0 ].filter = {
         1.0 / 49.0, 2.0 / 49.0, 3.0 / 49.0, 4.0 / 49.0, 5.0 / 49.0, 6.0 / 49.0, 7.0 / 49.0,
         7.0 / 49.0, 6.0 / 49.0, 5.0 / 49.0, 4.0 / 49.0, 3.0 / 49.0, 2.0 / 49.0, 1.0 / 49.0
   };
   filterArray[ 0 ].origin = -1;
   filterArray[ 0 ].symmetry = "general";
   dip::SeparableConvolution( img, out1, filterArray, { "periodic" } );
   filterArray[ 0 ].filter = {
         1.0 / 49.0, 2.0 / 49.0, 3.0 / 49.0, 4.0 / 49.0, 5.0 / 49.0, 6.0 / 49.0, 7.0 / 49.0
   };
   filterArray[ 0 ].symmetry = "d-even";
   dip::SeparableConvolution( img, out2, filterArray, { "periodic" } );
   DOCTEST_CHECK( dip::Mean( out1 - out2 ).As< dip::dfloat >() / meanval == doctest::Approx( 0.0 ));

   // Comparing general to d-odd
   filterArray[ 0 ].filter = {
         1.0 / 49.0, 2.0 / 49.0, 3.0 / 49.0, 4.0 / 49.0, 5.0 / 49.0, 6.0 / 49.0, 7.0 / 49.0,
         -7.0 / 49.0, -6.0 / 49.0, -5.0 / 49.0, -4.0 / 49.0, -3.0 / 49.0, -2.0 / 49.0, -1.0 / 49.0
   };
   filterArray[ 0 ].origin = -1;
   filterArray[ 0 ].symmetry = "general";
   dip::SeparableConvolution( img, out1, filterArray, { "periodic" } );
   filterArray[ 0 ].filter = {
         1.0 / 49.0, 2.0 / 49.0, 3.0 / 49.0, 4.0 / 49.0, 5.0 / 49.0, 6.0 / 49.0, 7.0 / 49.0
   };
   filterArray[ 0 ].symmetry = "d-odd";
   dip::SeparableConvolution( img, out2, filterArray, { "periodic" } );
   DOCTEST_CHECK( dip::Mean( out1 - out2 ).As< dip::dfloat >() / meanval == doctest::Approx( 0.0 ));

   // Comparing that last filter to GeneralConvolution
   dip::Image filter{ dip::UnsignedArray{ 19, 1, 1 }, 1, dip::DT_DFLOAT };
   filter.Fill( 0 );
   filter.At( 19/2, 0, 0 ) = 1;
   dip::SeparableConvolution( filter, filter, filterArray, { "add zeros" } );
   dip::GeneralConvolution( img, filter, out2, { "periodic" } );
   DOCTEST_CHECK( dip::Mean( out1 - out2 ).As< dip::dfloat >() / meanval == doctest::Approx( 0.0 ));

   // Comparing that one again, against ConvolveFT
   // Note that we can do this because we've used "periodic" boundary condition everywhere else
   dip::ConvolveFT( img, filter, out2 );
   DOCTEST_CHECK( dip::Mean( out1 - out2 ).As< dip::dfloat >() / meanval == doctest::Approx( 0.0 ));
}

#endif // DIP__ENABLE_DOCTEST

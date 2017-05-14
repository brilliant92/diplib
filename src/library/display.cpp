/*
 * DIPlib 3.0
 * This file contains the definition of the ImageDisplay class
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

#include <sstream>

#include "diplib.h"
#include "diplib/display.h"
#include "diplib/math.h"
#include "diplib/overload.h"

namespace dip {

// Don't call this function if mappingMode_ == MappingMode::MANUAL or mappingMode_ == MappingMode::MODULO!
void ImageDisplay::ComputeLimits( bool set ) {
   Limits* lims;
   Image tmp;
   if( globalStretch_ ) {
      if( mappingMode_ == MappingMode::PERCENTILE ) {
         lims = &( globalLimits_[ static_cast< unsigned >( complexMode_ ) ].percentile );
      } else {
         lims = &( globalLimits_[ static_cast< unsigned >( complexMode_ ) ].maxMin );
      }
      if( std::isnan( lims->lower )) {
         // Compute from image_
         tmp = image_.QuickCopy();
         if( !colorspace_.empty() && ( colorspace_ != "RGB" )) {
            tmp.SetColorSpace( colorspace_ );
            colorSpaceManager_->Convert( tmp, tmp, "RGB" );
         };
      }
   } else {
      if( mappingMode_ == MappingMode::PERCENTILE ) {
         lims = &( sliceLimits_[ static_cast< unsigned >( complexMode_ ) ].percentile );
      } else {
         lims = &( sliceLimits_[ static_cast< unsigned >( complexMode_ ) ].maxMin );
      }
      if( std::isnan( lims->lower )) {
         // Compute from rgbSlice_
         tmp = rgbSlice_.QuickCopy();
         // It's already converted to RGB...
      }
   }
   if( tmp.IsForged() ) {
      // Compute!
      if( tmp.DataType().IsBinary() ) {
         lims->lower = 0.0;
         lims->upper = 1.0;
      } else {
         if( tmp.DataType().IsComplex()) {
            switch( complexMode_ ) {
               //case ComplexMode::MAGNITUDE:
               default:
                  tmp = Abs( tmp );
                  break;
               case ComplexMode::PHASE:
                  tmp = Phase( tmp );
                  break;
               case ComplexMode::REAL:
                  tmp = tmp.Real();
                  break;
               case ComplexMode::IMAG:
                  tmp = tmp.Imaginary();
                  break;
            }
         }
         if( mappingMode_ == MappingMode::PERCENTILE ) {
            lims->lower = static_cast< dfloat >( Image::Sample( Percentile( tmp, {}, 5.0 )));
            lims->upper = static_cast< dfloat >( Image::Sample( Percentile( tmp, {}, 95.0 )));
         } else {
            MinMaxAccumulator res = GetMaximumAndMinimum( tmp );
            lims->lower = res.Minimum();
            lims->upper = res.Maximum();
         }
         // TODO: make sure lims doesn't have a NaN in it.
      }
   }
   if( set ) {
      range_ = *lims;
   }
}

void ImageDisplay::InvalidateSliceLimits() {
   for( auto& lim : sliceLimits_ ) {
      lim.maxMin = { NaN, NaN };
      lim.percentile = { NaN, NaN };
   }
}

ImageDisplay::Limits ImageDisplay::GetLimits( bool compute ) {
   Limits* lims;
   if( globalStretch_ ) {
      lims = &( globalLimits_[ static_cast< unsigned >( complexMode_ ) ].maxMin );
   } else {
      lims = &( sliceLimits_[ static_cast< unsigned >( complexMode_ ) ].maxMin );
   }
   if( compute && std::isnan( lims->lower )) {
      auto tmp = mappingMode_;
      mappingMode_ = MappingMode::MAXMIN;
      ComputeLimits( false ); // updates the value that `lims` points to, but doesn't update `range_`.
      mappingMode_ = tmp;
   }
   return *lims;
}

// Compute projection
void ImageDisplay::UpdateSlice() {
   if( sliceIsDirty_ ) {
      dip::uint nDims = image_.Dimensionality();
      if( nDims > 2 ) {
         switch( projectionMode_ ) {
            //case ProjectionMode::SLICE:
            default: {
               RangeArray rangeArray( nDims ); // By default, covers all image pixels
               for( dip::uint ii = 0; ii < nDims; ++ii ) {
                  if(( ii != dim1_ ) && ( ii != dim2_ )) {
                     rangeArray[ ii ] = Range( static_cast< dip::sint >( coordinates_[ ii ] ));
                  }
               }
               slice_ = image_.At( rangeArray );
               break;
            }
            case ProjectionMode::MAX: {
               BooleanArray process( nDims, true );
               process[ dim1_ ] = false;
               process[ dim2_ ] = false;
               if( image_.DataType().IsComplex() ) {
                  MaximumAbs( image_, {}, slice_, process );
               } else {
                  Maximum( image_, {}, slice_, process );
               }
               break;
            }
            case ProjectionMode::MEAN: {
               BooleanArray process( nDims, true );
               process[ dim1_ ] = false;
               process[ dim2_ ] = false;
               Mean( image_, {}, slice_, "", process );
               break;
            }
         }
         if( dim1_ == dim2_ ) {
            slice_.PermuteDimensions( { dim1_ } );
         } else {
            slice_.PermuteDimensions( { dim1_, dim2_ } );
         }
      } else {
         slice_ = image_.QuickCopy();
      }
      sliceIsDirty_ = false;
      rgbSliceIsDirty_ = true;
   }
}

void ImageDisplay::UpdateRgbSlice() {
   UpdateSlice();
   if( rgbSliceIsDirty_ ) {
      if( slice_.IsScalar() || ( colorspace_ == "RGB" )) {
         rgbSlice_ = slice_.QuickCopy();
      } else if( colorspace_.empty() ) {
         if( rgbSlice_.SharesData( slice_ )) {
            rgbSlice_.Strip();
         }
         rgbSlice_.ReForge( slice_.Sizes(), 3, slice_.DataType() );
         if( red_ >= 0 ) {
            rgbSlice_[ 0 ].Copy( slice_[ static_cast< dip::uint >( red_ ) ] );
         } else {
            rgbSlice_[ 0 ].Fill( 0 );
         }
         if( green_ >= 0 ) {
            rgbSlice_[ 1 ].Copy( slice_[ static_cast< dip::uint >( green_ ) ] );
         } else {
            rgbSlice_[ 1 ].Fill( 0 );
         }
         if( blue_ >= 0 ) {
            rgbSlice_[ 2 ].Copy( slice_[ static_cast< dip::uint >( blue_ ) ] );
         } else {
            rgbSlice_[ 2 ].Fill( 0 );
         }
      } else {
         slice_.SetColorSpace( colorspace_ );
         colorSpaceManager_->Convert( slice_, rgbSlice_, "RGB" );
      }
      rgbSliceIsDirty_ = false;
      outputIsDirty_ = true;
      InvalidateSliceLimits();
   }
}

namespace {

template< typename T >
dfloat convert( T v, bool /*usePhase*/ ) {
   return v;
}

template< typename T >
dfloat convert( std::complex< T > v, bool usePhase ) {
   if( usePhase ) {
      return std::arg( v );
   } else {
      return std::abs( v );
   }
}

template< typename T >
T ToReal( T v, bool /*usePhase*/ ) {
   return v;
}

template< typename T >
T ToReal( std::complex< T > v, bool usePhase ) {
   if( usePhase ) {
      return std::arg( v );
   } else {
      return std::abs( v );
   }
}

template< typename TPI >
void CastToUint8(
      Image const& slice,
      Image& out,
      bool usePhase,
      bool logarithmic,
      bool useModulo,
      dip::dfloat offset,
      dip::dfloat scale
) {
   dip::uint width = slice.Size( 0 );
   dip::uint height = slice.Dimensionality() == 2 ? slice.Size( 1 ) : 1;
   dip::sint sliceStride0 = slice.Stride( 0 );
   dip::sint sliceStride1 = slice.Stride( 1 );
   dip::sint outStride0 = out.Stride( 0 );
   dip::sint outStride1 = out.Stride( 1 );
   dip::uint telems = slice.TensorElements();
   dip::sint sliceStrideT = slice.TensorStride();
   dip::sint outStrideT = out.TensorStride();
   for( dip::sint kk = 0; kk < static_cast< dip::sint >( telems ); ++kk ) {
      TPI* slicePtr = static_cast< TPI* >( slice.Pointer( sliceStrideT * kk ) );
      uint8* outPtr = static_cast< uint8* >( out.Pointer( outStrideT * kk ) );
      for( dip::uint jj = 0; jj < height; ++jj ) {
         TPI* iPtr = slicePtr;
         uint8* oPtr = outPtr;
         if( logarithmic ) {
            for( dip::uint ii = 0; ii < width; ++ii ) {
               *oPtr = clamp_cast< uint8 >( std::log( convert( *iPtr, usePhase ) + offset ) * scale );
               iPtr += sliceStride0;
               oPtr += outStride0;
            }
         } else if( useModulo ) {
            for( dip::uint ii = 0; ii < width; ++ii ) {
               dfloat scaled = ( convert( *iPtr, usePhase ) + offset ) * scale;
               scaled = scaled == 0 ? 0 : ( std::fmod( scaled - 1, 255.0 ) + 1 );
               *oPtr = clamp_cast< uint8 >( scaled );
               iPtr += sliceStride0;
               oPtr += outStride0;
            }
         } else {
            for( dip::uint ii = 0; ii < width; ++ii ) {
               *oPtr = clamp_cast< uint8 >(( convert( *iPtr, usePhase ) + offset ) * scale );
               iPtr += sliceStride0;
               oPtr += outStride0;
            }
         }
         slicePtr += sliceStride1;
         outPtr += outStride1;
      }
   }
}
template<>
void CastToUint8< bin >(
      Image const& slice,
      Image& out,
      bool /*usePhase*/,
      bool /*logarithmic*/,
      bool /*useModulo*/,
      dip::dfloat /*offset*/,
      dip::dfloat /*scale*/
) {
   dip::uint width = slice.Size( 0 );
   dip::uint height = slice.Dimensionality() == 2 ? slice.Size( 1 ) : 1;
   dip::sint sliceStride0 = slice.Stride( 0 );
   dip::sint sliceStride1 = slice.Stride( 1 );
   dip::sint outStride0 = out.Stride( 0 );
   dip::sint outStride1 = out.Stride( 1 );
   dip::uint telems = slice.TensorElements();
   dip::sint sliceStrideT = slice.TensorStride();
   dip::sint outStrideT = out.TensorStride();
   for( dip::sint kk = 0; kk < static_cast< dip::sint >( telems ); ++kk ) {
      bin* slicePtr = static_cast< bin* >( slice.Pointer( sliceStrideT * kk ) );
      uint8* outPtr = static_cast< uint8* >( out.Pointer( outStrideT * kk ) );
      for( dip::uint jj = 0; jj < height; ++jj ) {
         bin* iPtr = slicePtr;
         uint8* oPtr = outPtr;
         for( dip::uint ii = 0; ii < width; ++ii ) {
            *oPtr = *iPtr ? uint8( 255 ) : uint8( 0 );
            iPtr += sliceStride0;
            oPtr += outStride0;
         }
         slicePtr += sliceStride1;
         outPtr += outStride1;
      }
   }
}

} // namespace

void ImageDisplay::UpdateOutput() {
   UpdateRgbSlice();
   if( outputIsDirty_ ) {
      // Input range to map to output
      if(( mappingMode_ != MappingMode::MANUAL ) && ( mappingMode_ != MappingMode::MODULO )) {
         ComputeLimits();
         if( mappingMode_ == MappingMode::BASED ) {
            dfloat bound = std::max( std::abs( range_.lower ), std::abs( range_.upper ));
            range_ = { -bound, bound };
         }
      }
      // Mapping function
      bool logarithmic = mappingMode_ == MappingMode::LOGARITHMIC;
      bool useModulo = mappingMode_ == MappingMode::MODULO;
      dfloat offset;
      dfloat scale;
      if( logarithmic ) {
         offset = 1.0 - range_.lower;
         scale = 255.0 / std::log( range_.upper + offset );
      } else {
         offset = -range_.lower;
         scale = 255.0 / ( range_.upper - range_.lower );
      }
      // Complex to real
      Image slice = rgbSlice_.QuickCopy();
      bool usePhase = false;
      if( slice.DataType().IsComplex() ) {
         switch( complexMode_ ) {
            //case ComplexMode::MAGNITUDE:
            default:
               // Nothing to do.
               break;
            case ComplexMode::PHASE:
               usePhase = true;
               break;
            case ComplexMode::REAL:
               slice = slice.Real();
               break;
            case ComplexMode::IMAG:
               slice = slice.Imaginary();
               break;
         }
      }
      // Create output
      DIP_ASSERT(( !twoDimOut_ && ( slice.Dimensionality() == 1 )) || ( twoDimOut_ && ( slice.Dimensionality() == 2 )));
      output_.ReForge( slice.Sizes(), slice.TensorElements(), DT_UINT8 );
      // Stretch and convert the data
      DIP_OVL_CALL_ALL( CastToUint8, ( slice, output_, usePhase, logarithmic, useModulo, offset, scale ), slice.DataType() );
      outputIsDirty_ = false;
   }
}

Image const& ImageDisplay::Slice() {
   UpdateSlice();
   return slice_;
}

Image const& ImageDisplay::Output() {
   UpdateOutput();
   return output_;
}

} // namespace dip
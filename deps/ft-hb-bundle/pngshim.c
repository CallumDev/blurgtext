/****************************************************************************
 *
 * pngshim.c
 *
 *   PNG Bitmap glyph support.
 *
 * Copyright (C) 2013-2023 by
 * Google, Inc.
 * Written by Stuart Gill and Behdad Esfahbod.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.  By continuing to use, modify, or distribute
 * this file you indicate that you have read the license and
 * understand and accept it fully.
 *
 */

/* pngshim rewritten to use embedded stb_image.h for gamedev purposes */

#include <freetype/internal/ftdebug.h>
#include <freetype/internal/ftstream.h>
#include <freetype/tttags.h>
#include FT_CONFIG_STANDARD_LIBRARY_H


#if defined( TT_CONFIG_OPTION_EMBEDDED_BITMAPS ) && \
defined( FT_CONFIG_OPTION_USE_PNG )

// PNG-only
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "pngshim.h"

#include "sferrors.h"


/* This code is freely based on cairo-png.c.  There's so many ways */
/* to call libpng, and the way cairo does it is defacto standard.  */

static unsigned int
multiply_alpha( unsigned int  alpha,
                unsigned int  color )
{
    unsigned int  temp = alpha * color + 0x80;


    return ( temp + ( temp >> 8 ) ) >> 8;
}


/* Premultiplies data and converts RGBA bytes => BGRA. */
static void
premultiply_data( int rowbytes,
                  stbi_uc*      data )
{
    unsigned int  i = 0, limit;

    /* The `vector_size' attribute was introduced in gcc 3.1, which */
    /* predates clang; the `__BYTE_ORDER__' preprocessor symbol was */
    /* introduced in gcc 4.6 and clang 3.2, respectively.           */
    /* `__builtin_shuffle' for gcc was introduced in gcc 4.7.0.     */
    /*                                                              */
    /* Intel compilers do not currently support __builtin_shuffle;  */

    /* The Intel check must be first. */
    #if !defined( __INTEL_COMPILER )                                       && \
    ( ( defined( __GNUC__ )                                &&             \
    ( ( __GNUC__ >= 5 )                              ||               \
    ( ( __GNUC__ == 4 ) && ( __GNUC_MINOR__ >= 7 ) ) ) )         ||   \
    ( defined( __clang__ )                                       &&     \
    ( ( __clang_major__ >= 4 )                               ||       \
    ( ( __clang_major__ == 3 ) && ( __clang_minor__ >= 2 ) ) ) ) ) && \
    defined( __OPTIMIZE__ )                                            && \
    defined( __SSE__ )                                                 && \
    __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

    #ifdef __clang__
    /* the clang documentation doesn't cover the two-argument case of */
    /* `__builtin_shufflevector'; however, it is is implemented since */
    /* version 2.8                                                    */
    #define vector_shuffle  __builtin_shufflevector
    #else
    #define vector_shuffle  __builtin_shuffle
    #endif

    typedef unsigned short  v82 __attribute__(( vector_size( 16 ) ));


    if ( rowbytes > 15 )
    {
        /* process blocks of 16 bytes in one rush, which gives a nice speed-up */
        limit = rowbytes - 16 + 1;
        for ( ; i < limit; i += 16 )
        {
            unsigned char*  base = &data[i];

            v82  s, s0, s1, a;

            /* clang <= 3.9 can't apply scalar values to vectors */
            /* (or rather, it needs a different syntax)          */
            v82  n0x80 = { 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80 };
            v82  n0xFF = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
            v82  n8    = { 8, 8, 8, 8, 8, 8, 8, 8 };

            v82  ma = { 1, 1, 3, 3, 5, 5, 7, 7 };
            v82  o1 = { 0, 0xFF, 0, 0xFF, 0, 0xFF, 0, 0xFF };
            v82  m0 = { 1, 0, 3, 2, 5, 4, 7, 6 };


            ft_memcpy( &s, base, 16 );            /* RGBA RGBA RGBA RGBA */
            s0 = s & n0xFF;                       /*  R B  R B  R B  R B */
            s1 = s >> n8;                         /*  G A  G A  G A  G A */

            a   = vector_shuffle( s1, ma );       /*  A A  A A  A A  A A */
            s1 |= o1;                             /*  G 1  G 1  G 1  G 1 */
            s0  = vector_shuffle( s0, m0 );       /*  B R  B R  B R  B R */

            s0 *= a;
            s1 *= a;
            s0 += n0x80;
            s1 += n0x80;
            s0  = ( s0 + ( s0 >> n8 ) ) >> n8;
            s1  = ( s1 + ( s1 >> n8 ) ) >> n8;

            s = s0 | ( s1 << n8 );
            ft_memcpy( base, &s, 16 );
        }
    }
    #endif /* use `vector_size' */

    limit = rowbytes;
    for ( ; i < limit; i += 4 )
    {
        unsigned char*  base  = &data[i];
        unsigned int    alpha = base[3];


        if ( alpha == 0 )
            base[0] = base[1] = base[2] = base[3] = 0;

        else
        {
            unsigned int  red   = base[0];
            unsigned int  green = base[1];
            unsigned int  blue  = base[2];


            if ( alpha != 0xFF )
            {
                red   = multiply_alpha( alpha, red   );
                green = multiply_alpha( alpha, green );
                blue  = multiply_alpha( alpha, blue  );
            }

            base[0] = (unsigned char)blue;
            base[1] = (unsigned char)green;
            base[2] = (unsigned char)red;
            base[3] = (unsigned char)alpha;
        }
    }
}

static int eof_FT_Stream(void* user)
{
    FT_Stream stream = (FT_Stream)user;
    return stream->pos >= stream->size ? 1 : 0;
}

static void skip_FT_Stream(void* user, int n)
{
   FT_Stream  stream = (FT_Stream)user;
   FT_Stream_Skip(stream, n);
}

static int read_FT_Stream( void* user, char *data, int size)
{
    FT_Stream  stream = (FT_Stream)user;
    return FT_Stream_TryRead(stream, data, size);
}


FT_LOCAL_DEF( FT_Error )
Load_SBit_Png( FT_GlyphSlot     slot,
               FT_Int           x_offset,
               FT_Int           y_offset,
               FT_Int           pix_bits,
               TT_SBit_Metrics  metrics,
               FT_Memory        memory,
               FT_Byte*         data,
               FT_UInt          png_len,
               FT_Bool          populate_map_and_metrics,
               FT_Bool          metrics_only )
{
    FT_Bitmap    *map   = &slot->bitmap;
    FT_Error      error = FT_Err_Ok;
    FT_StreamRec  stream;

    stbi_uc* imgBuffer = NULL;
    stbi_uc** rows = NULL;
    stbi_io_callbacks stbicb;
    int  imgWidth, imgHeight;
    FT_Int      i;


    if ( x_offset < 0 ||
        y_offset < 0 )
    {
        error = FT_THROW( Invalid_Argument );
        goto Exit;
    }

    if ( !populate_map_and_metrics                            &&
        ( (FT_UInt)x_offset + metrics->width  > map->width ||
        (FT_UInt)y_offset + metrics->height > map->rows  ||
        pix_bits != 32                                   ||
        map->pixel_mode != FT_PIXEL_MODE_BGRA            ) )
    {
        error = FT_THROW( Invalid_Argument );
        goto Exit;
    }

    FT_Stream_OpenMemory( &stream, data, png_len );

    stbicb.read = &read_FT_Stream;
    stbicb.skip = &skip_FT_Stream;
    stbicb.eof = &eof_FT_Stream;

    imgBuffer = stbi_load_from_callbacks(&stbicb, &stream, &imgWidth, &imgHeight, NULL, 4);

    if(!imgBuffer)
    {
        error = FT_THROW ( Invalid_Glyph_Format );
        goto DestroyExit;
    }

    if ( !imgBuffer                            ||
        ( !populate_map_and_metrics                &&
        ( (FT_Int)imgWidth  != metrics->width  ||
        (FT_Int)imgHeight != metrics->height ) ) )
        goto DestroyExit;

    if ( populate_map_and_metrics )
    {
        /* reject too large bitmaps similarly to the rasterizer */
        if ( imgHeight > 0x7FFF || imgWidth > 0x7FFF )
        {
            error = FT_THROW( Array_Too_Large );
            goto DestroyExit;
        }

        metrics->width  = (FT_UShort)imgWidth;
        metrics->height = (FT_UShort)imgHeight;

        map->width      = metrics->width;
        map->rows       = metrics->height;
        map->pixel_mode = FT_PIXEL_MODE_BGRA;
        map->pitch      = (int)( map->width * 4 );
        map->num_grays  = 256;
    }

    if ( metrics_only )
        goto DestroyExit;

    if ( populate_map_and_metrics )
    {
        /* this doesn't overflow: 0x7FFF * 0x7FFF * 4 < 2^32 */
        FT_ULong  size = map->rows * (FT_ULong)map->pitch;

        error = ft_glyphslot_alloc_bitmap( slot, size );
        if ( error )
            goto DestroyExit;
    }

    if ( FT_QNEW_ARRAY( rows, imgHeight ) )
    {
        error = FT_THROW( Out_Of_Memory );
        goto DestroyExit;
    }

    for ( i = 0; i < (FT_Int)imgHeight; i++ )
        rows[i] = map->buffer + ( y_offset + i ) * map->pitch + x_offset * 4;

    for( i = 0; i < (FT_Int) imgHeight; i++)
    {
        int stride = imgWidth * 4;
        premultiply_data(stride, &imgBuffer[i * stride]);
        ft_memcpy(rows[i], &imgBuffer[i * stride], stride);
    }

    DestroyExit:
    /* even if reading fails with longjmp, rows must be freed */
    FT_FREE( rows );
    if(imgBuffer)
    {
        stbi_image_free(imgBuffer);
    }
    FT_Stream_Close( &stream );

    Exit:
    return error;
}

#else /* !(TT_CONFIG_OPTION_EMBEDDED_BITMAPS && FT_CONFIG_OPTION_USE_PNG) */

/* ANSI C doesn't like empty source files */
typedef int  pngshim_dummy_;

#endif /* !(TT_CONFIG_OPTION_EMBEDDED_BITMAPS && FT_CONFIG_OPTION_USE_PNG) */


/* END */

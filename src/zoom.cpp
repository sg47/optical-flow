// This program is free software: you can use, modify and/or redistribute it
// under the terms of the simplified BSD License. You should have received a
// copy of this license along this program. If not, see
// <http://www.opensource.org/licenses/bsd-license.html>.
//
// Copyright (C) 2012, Javier Sánchez Pérez <jsanchez@dis.ulpgc.es>
// All rights reserved.

#include "zoom.h"

#include "operators.h"
#include "bicubic_interpolation.h"
#include "math.h"

#define ZOOM_SIGMA_ZERO 0.6

/**
 *
 * Compute the size of a zoomed image from the zoom factor
 *
 **/
void
zoom_size(int nx, // width of the orignal image
          int ny, // height of the orignal image
          int *nxx, // width of the zoomed image
          int *nyy, // height of the zoomed image
          double factor // zoom factor between 0 and 1
          )
{
    //compute the new size corresponding to factor
    //we add 0.5 for rounding off to the closest number
    *nxx = (int)( nx * factor + 0.5 );
    *nyy = (int)( ny * factor + 0.5 );
}

/**
 *
 * Downsample an image
 *
 **/
void
zoom_out(const ofpix_t *I, // input image
         ofpix_t *Iout,  // output image
         const int nx, // image width
         const int ny, // image height
         const double factor // zoom factor between 0 and 1
         )
{
    // temporary working image
    ofpix_t *Is = new ofpix_t [nx * ny];

    for (int i = 0; i < nx * ny; i++) {
        Is[i] = I[i];
    }

    // compute the size of the zoomed image
    int nxx, nyy;
    zoom_size(nx, ny, &nxx, &nyy, factor);

    // compute the Gaussian sigma for smoothing
    const double sigma = ZOOM_SIGMA_ZERO * sqrt(1.0 / (factor * factor) - 1.0);

    // pre-smooth the image
    gaussian(Is, nx, ny, sigma);

    // re-sample the image using bicubic interpolation
    #pragma omp parallel for
    for (int i1 = 0; i1 < nyy; i1++) {
        for (int j1 = 0; j1 < nxx; j1++) {
            const double i2  = i1 / factor;
            const double j2  = j1 / factor;
            ofpix_t g = bicubic_interpolation_at(Is, j2, i2, nx, ny, false);
            Iout[i1 * nxx + j1] = g;
        }
    }

    delete [] Is;
}

/**
 *
 * Downsample an image
 *
 **/
void
zoom_out_color(const ofpix_t *I, // input image
               ofpix_t *Iout, // output image
               const int nx, // image width
               const int ny, // image height
               const int nz, // number of color channels in image
               const double factor // zoom factor between 0 and 1
               )
{
    // temporary working image
    ofpix_t *Is = new ofpix_t [nx * ny];

    for (int i = 0; i < nx * ny; i++) {
        Is[i] = I[i];
    }

    // compute the size of the zoomed image
    int nxx, nyy;
    zoom_size(nx, ny, &nxx, &nyy, factor);

    // compute the Gaussian sigma for smoothing
    const double sigma = ZOOM_SIGMA_ZERO * sqrt(1.0 / (factor * factor) - 1.0);

    // pre-smooth the image
    gaussian(Is, nx, ny, sigma);

    // re-sample the image using bicubic interpolation
    for (int index_multichannel = 0; index_multichannel < nz; index_multichannel++) {
        #pragma omp parallel for
        for (int i1 = 0; i1 < nyy; i1++) {
            for (int j1 = 0; j1 < nxx; j1++) {
                const double i2  = i1 / factor;
                const double j2  = j1 / factor;

                Iout[(i1 * nxx + j1) * nz + index_multichannel] = bicubic_interpolation_at_color (Is, j2, i2, nx, ny, nz, index_multichannel);
            }
        }
    }

    delete [] Is;
}

/**
 *
 * Function to upsample the image
 *
 **/
void
zoom_in(const ofpix_t *I, // input image
        ofpix_t *Iout, // output image
        int nx,     // width of the original image
        int ny,     // height of the original image
        int nxx,    // width of the zoomed image
        int nyy     // height of the zoomed image
        )
{
    // compute the zoom factor
    const double factorx = ( (double)nxx / nx );
    const double factory = ( (double)nyy / ny );

    // re-sample the image using bicubic interpolation
    #pragma omp parallel for
    for (int i1 = 0; i1 < nyy; i1++) {
        for (int j1 = 0; j1 < nxx; j1++) {
            double i2 =  i1 / factory;
            double j2 =  j1 / factorx;
            double g = bicubic_interpolation_at(I, j2, i2, nx, ny, false);
            Iout[i1 * nxx + j1] = g;
        }
    }
}


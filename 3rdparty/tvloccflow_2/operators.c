// This program is free software: you can use, modify and/or redistribute it
// under the terms of the simplified BSD License. You should have received a
// copy of this license along this program. If not, see
// <http://www.opensource.org/licenses/bsd-license.html>.
//
// Copyright (C) 2011, Javier S�nchez P�rez <jsanchez@dis.ulpgc.es>
// Copyright (C) 2012, Coloma Ballester <coloma.ballester@upf.edu>
// Copyright (C) 2013-2014, J. F. Garamendi <jf.garamendi@upf.edu>
// All rights reserved.

#include "operators.h"
#include "xmalloc.h"
#include "utils.h"

/**
 *
 * Details on how to compute the divergence and the grad(u) can be found in:
 * [2] A. Chambolle, "An Algorithm for Total Variation Minimization and
 * Applications", Journal of Mathematical Imaging and Vision, 20: 89-97, 2004
 *
 **/

void divergence(const double *v1, const double *v2, double *div, const int nx,
		const int ny) {
	//apply the divergence to the center body of the image
	for (int i = 1; i < ny - 1; i++) {
		for (int j = 1; j < nx - 1; j++) {
			const int p = i * nx + j;
			const int p1 = p - 1;
			const int p2 = p - nx;

			const double v1x = v1[p] - v1[p1];
			const double v2y = v2[p] - v2[p2];

			div[p] = v1x + v2y;
		}
	}

	//apply the divergence to the first and last rows
	for (int j = 1; j < nx - 1; j++) {
		const int p = (ny - 1) * nx + j;

		div[j] = v1[j] - v1[j - 1] + v2[j];
		div[p] = v1[p] - v1[p - 1] - v2[p - nx];
	}

	//apply the divergence to the first and last columns
	for (int i = 1; i < ny - 1; i++) {
		const int p1 = i * nx;
		const int p2 = (i + 1) * nx - 1;

		div[p1] = v1[p1] + v2[p1] - v2[p1 - nx];
		div[p2] = -v1[p2 - 1] + v2[p2] - v2[p2 - nx];

	}

	div[0] = v1[0] + v2[0];
	div[nx - 1] = -v1[nx - 2] + v2[nx - 1];
	div[(ny - 1) * nx] = v1[(ny - 1) * nx] - v2[(ny - 2) * nx];
	div[ny * nx - 1] = -v1[ny * nx - 2] - v2[(ny - 1) * nx - 1];
}

/**
 *
 * Function to compute the gradient with forward differences
 * (see [2] for details)
 *
 **/
void forward_gradient(const double *f, //input image
		double *fx,      //computed x derivative
		double *fy,      //computed y derivative
		const int nx,   //image width
		const int ny    //image height
) {
	// compute the gradient on the central body of the image
#pragma omp parallel for schedule(dynamic)
	for (int i = 0; i < ny - 1; i++) {
		for (int j = 0; j < nx - 1; j++) {
			const int p = i * nx + j;
			const int p1 = p + 1;
			const int p2 = p + nx;

			fx[p] = f[p1] - f[p];
			fy[p] = f[p2] - f[p];
		}
	}

	// compute the gradient on the last row
	for (int j = 0; j < nx - 1; j++) {
		const int p = (ny - 1) * nx + j;

		fx[p] = f[p + 1] - f[p];
		fy[p] = 0;
	}

	// compute the gradient on the last column
	for (int i = 1; i < ny; i++) {
		const int p = i * nx - 1;

		fx[p] = 0;
		fy[p] = f[p + nx] - f[p];
	}

	fx[ny * nx - 1] = 0;
	fy[ny * nx - 1] = 0;
}

void centered_gradient(double *input, double *dx, double *dy, const int nx,
		const int ny) {
	double filter_der[5] = { -1.0 / 12.0, 8.0 / 12.0, 0.0, -8.0 / 12.0, 1.0
			/ 12.0 };
	double filter_id[1] = { 1.0 };

	me_sepconvol(input, dx, nx, ny, filter_der, filter_id, 5, 1);
	me_sepconvol(input, dy, nx, ny, filter_id, filter_der, 1, 5);
}


/**
 *
 * In-place Gaussian smoothing of an image
 *
 */
void gaussian(double *I,             // input/output image
		const int xdim,       // image width
		const int ydim,       // image height
		const double sigma    // Gaussian sigma
) {
	const int boundary_condition = DEFAULT_BOUNDARY_CONDITION;
	const int window_size = DEFAULT_GAUSSIAN_WINDOW_SIZE;

	const double den = 2 * sigma * sigma;
	const int size = (int) (window_size * sigma) + 1;
	const int bdx = xdim + size;
	const int bdy = ydim + size;

	if (boundary_condition && size > xdim) {
		fprintf(stderr, "GaussianSmooth: sigma too large\n");
		abort();
	}

	// compute the coefficients of the 1D convolution kernel
	double B[size];
	for (int i = 0; i < size; i++)
		B[i] = 1 / (sigma * sqrt(2.0 * 3.1415926)) * exp(-i * i / den);

	// normalize the 1D convolution kernel
	double norm = 0;
	for (int i = 0; i < size; i++)
		norm += B[i];
	norm *= 2;
	norm -= B[0];
	for (int i = 0; i < size; i++)
		B[i] /= norm;

	// convolution of each line of the input image
	double *R = (double*) malloc((size + xdim + size) * sizeof(double));

	for (int k = 0; k < ydim; k++) {
		int i, j;
		for (i = size; i < bdx; i++)
			R[i] = I[k * xdim + i - size];

		switch (boundary_condition) {
		case BOUNDARY_CONDITION_DIRICHLET:
			for (i = 0, j = bdx; i < size; i++, j++)
				R[i] = R[j] = 0;
			break;

		case BOUNDARY_CONDITION_REFLECTING:
			for (i = 0, j = bdx; i < size; i++, j++) {
				R[i] = I[k * xdim + size - i];
				R[j] = I[k * xdim + xdim - i - 1];
			}
			break;

		case BOUNDARY_CONDITION_PERIODIC:
			for (i = 0, j = bdx; i < size; i++, j++) {
				R[i] = I[k * xdim + xdim - size + i];
				R[j] = I[k * xdim + i];
			}
			break;
		}

		for (i = size; i < bdx; i++) {
			double sum = B[0] * R[i];
			for (j = 1; j < size; j++)
				sum += B[j] * (R[i - j] + R[i + j]);
			I[k * xdim + i - size] = sum;
		}
	}

	// convolution of each column of the input image
	double *T = (double*) malloc((size + ydim + size) * sizeof(double));

	for (int k = 0; k < xdim; k++) {
		int i, j;
		for (i = size; i < bdy; i++)
			T[i] = I[(i - size) * xdim + k];

		switch (boundary_condition) {
		case BOUNDARY_CONDITION_DIRICHLET:
			for (i = 0, j = bdy; i < size; i++, j++)
				T[i] = T[j] = 0;
			break;

		case BOUNDARY_CONDITION_REFLECTING:
			for (i = 0, j = bdy; i < size; i++, j++) {
				T[i] = I[(size - i) * xdim + k];
				T[j] = I[(ydim - i - 1) * xdim + k];
			}
			break;

		case BOUNDARY_CONDITION_PERIODIC:
			for (i = 0, j = bdx; i < size; i++, j++) {
				T[i] = I[(ydim - size + i) * xdim + k];
				T[j] = I[i * xdim + k];
			}
			break;
		}

		for (i = size; i < bdy; i++) {
			double sum = B[0] * T[i];
			for (j = 1; j < size; j++)
				sum += B[j] * (T[i - j] + T[i + j]);
			I[(i - size) * xdim + k] = sum;
		}
	}

	free(R);
	free(T);
}


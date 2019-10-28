
#include <math.h>
#include <stdint.h>
#include <iostream>
#include <stdlib.h>
#include <complex>
#include "kiss_fftnd.h"
#include <time.h>

using namespace std;
extern "C" {
    int sub2ind(int x, int y, int z, int dimX, int dimY, int dimZ) {
        return dimX*dimY*z+dimX*y+x;
    }

    float* getMinMax(float* base, int dimX, int dimY) {
        float minMax[2];
        minMax[0] = base[0]; //min
        minMax[1] = base[0]; //max

        for (int i=0; i < dimX*dimY; i++) {
            if ( base[i] < minMax[0] ) {
                minMax[0] = base[i];
            } else if ( base[i] > minMax[1]) {
                minMax[1] = base[i];
            }
        }
        return minMax;
    }

    float* normalize(float* base, float scale, int dimX, int dimY) {
        float* minMax = getMinMax(base, dimX, dimY);
        for (int i=0; i<dimX*dimY; i++ ) {
            base[i] = (base[i]-minMax[0])/ minMax[1]*scale;
        }
        return base;
    }

    int polarMeshnOapp (float* rr, float* pp, float* oapp, float r_max, float obj_ap_r, int numPx) {
        float center = numPx/2;
        int idx;
        float xval;
        float yval;
        for(int j = 0; j <numPx; j++) {
            for( int i=0; i<numPx; i++) {
                idx = i + numPx*j;
                xval = (i-center)/center*r_max;
                yval = (j-center)/center*r_max;
                rr[idx] = sqrt( pow(xval,2) + pow(yval,2));
                pp[idx] = atan2(yval,xval);
                if (rr[idx] > obj_ap_r) {
                    oapp[idx] = 0;
                } else {
                    oapp[idx] = 1;
                }
            }
        }
        return 0;
    }
    
    float* noisyGrating(int dimX, int dimY){
        srand(time(NULL));
        float* vals = new float[dimX*dimY];
        for(int i=0; i<dimX*dimY; i++)
        {
            vals[i] = rand() / float(RAND_MAX);
        }
        return vals;
    }

    float* generateSample(int dimX, int dimY, int scaleFactor)
    {
        float* subsample = noisyGrating(dimX/scaleFactor,dimX/scaleFactor);
        float* supersample = new float[dimX*dimY];
        for (int j=0; j<dimY; j++) {
            for (int i=0; i<dimX; i++) {
                int idx = sub2ind(i,j,0,dimX,dimY,1);
                int idxsub = sub2ind(i/scaleFactor,j/scaleFactor,0,dimX/scaleFactor,dimY/scaleFactor,1);
                supersample[idx] = subsample[idxsub];
            }
        }
        return supersample;
    }

    complex<float>* generateTransmissionFn(float* sample, int dimX, int dimY, float interactionParam )
    {
        complex<float> * trans = new complex<float>[dimX*dimY];
        complex<float> imag(0.0,1.0);
        for (int i=0; i< dimX*dimY; i++) {
            complex<float> real_part(M_PI/4*sample[i]*interactionParam,0.0);
            trans[i] = exp(-imag*real_part);
        }
        return trans;
    }

    float* complexToReal(complex<float>* orig, int dimX, int dimY)
    {
        float * real = new float[dimX*dimY];
        for(int i =0; i < dimX*dimY; i++)
        {
            real[i] = abs(orig[i]);
        }
        return real;
    }

    complex<float>* realToComplex(float* orig, int dimX, int dimY) {
        complex<float>* comp = new complex<float>[dimX*dimY];
        for(int i = 0; i<dimX*dimY; i++)
        {
            comp[i].real(orig[i]);
            comp[i].imag(0);
        }

        return comp;
    }

    kiss_fft_cpx* complexToKiss(complex<float>* orig, int dimX, int dimY) {
        kiss_fft_cpx* kisscpx = new kiss_fft_cpx[dimX*dimY];
        kiss_fft_cpx holder;
        for (int i=0; i <dimX*dimY; i++) {
            holder.r = real(orig[i]);
            holder.i = imag(orig[i]);
            kisscpx[i] = holder;
        }
        return kisscpx;
    }

    complex<float>*  kissToComplex(kiss_fft_cpx* orig, int dimX, int dimY) {
        complex<float>* comp = new complex<float>[dimX*dimY];
        for (int i =0; i <dimX*dimY; i++) {
            //comp[i] = (orig[i].r,orig[i].i);
            comp[i].real(orig[i].r);
            comp[i].imag(orig[i].i);
        }
        return comp;
    }


    float* packageOutput(float* base1, float* base2, float* scalars, int dimX, int dimY, int nScal)
    {
        int sz = dimX*dimY;
        float* imageStack = new float[sz*2+nScal];
        for(size_t i = 0; i < sz; i++) {
            imageStack[i] = base1[i]; // copy the allocated memory
            imageStack[sz+i] = base2[i]; 
        }
        for(size_t i = 0; i < nScal; i++)
        {
            imageStack[2*sz+i] = scalars[i];
        }
        return imageStack;
    }

    float* calculateChi0(float* magptr, float* angleptr, float * alrr, float* alpp, int numPx, int numAbs)
    {
        float* chi0 = new float[numPx*numPx];
        int n[14] = {1,1,2,2,3,3,3,4,4,4,5,5,5,5};
        int m[14] = {0,2,1,3,0,2,4,1,3,5,0,2,4,6};
        float lambda = 1.97e-12;
        for (int j=0; j<numPx; j++) {
            for (int i=0; i<numPx; i++) {
                int idx = sub2ind(i,j,0,numPx,numPx,1);
                chi0[idx] = 0;
            }
        }

        for (int j=0; j<numPx; j++) {
            for (int i=0; i<numPx; i++) {
                int idx = sub2ind(i,j,0,numPx,numPx,1);
                for(int k = 0; k < numAbs; k++)
                {
                    chi0[idx] = chi0[idx]+2*M_PI/lambda*magptr[k]*pow(alrr[idx],n[k]+1)*cos(m[k]*(alpp[idx]-angleptr[k]))/(n[k]+1);
                }
            }
        }
        return chi0;
    }

    complex<float>* calculateChi(float* chi0, int numPx)
    {
        complex<float>*chi = new complex<float>[numPx*numPx];
        complex<float> imag(0.0,1.0);
        for(int i = 0; i < numPx*numPx; i++)
        {
            chi[i] = exp(-imag * chi0[i]);
        }
        return chi;
    }

    float* maskChi0(float* chi0, int numPx, float threshold)
    {
        float* maskedChi0 = new float[numPx*numPx];
        for(int i = 0; i < numPx*numPx; i++)
        {
            if(chi0[i]< threshold && chi0[i] > -threshold)
            {
                maskedChi0[i] = 1;
            }
            else
            {
                maskedChi0[i] = 0;
            }
        }
        return maskedChi0;
    }

    float* fftShift(float* original, int numPx)
    {
        // only for square matrices...
        float* shifted = new float[numPx*numPx];

        for (int j=0; j<numPx/2; j++) {
            for (int i=0; i<numPx/2; i++) {
                int idx_q1 = sub2ind(i,j,0,numPx,numPx,1);
                int idx_q3 = sub2ind(i+numPx/2,j+numPx/2,0,numPx,numPx,1);
                int idx_q2 = sub2ind(i+numPx/2,j,0,numPx,numPx,1);
                int idx_q4 = sub2ind(i,j+numPx/2,0,numPx,numPx,1);
                shifted[idx_q1] = original[idx_q3];
                shifted[idx_q3] = original[idx_q1];
                shifted[idx_q4] = original[idx_q2];
                shifted[idx_q2] = original[idx_q4];
            }
        }
        return shifted;
    }

    complex<float>* cmplxFFT(complex<float>* comp, int dimX, int dimY) {
        int isInverseFFT = 0;
        int ndims = 2;
        int dims[2];
        dims[0] = dimX;
        dims[1] = dimY;

        kiss_fft_cpx* cxin;
        kiss_fft_cpx* cxout;
        
        kiss_fftnd_cfg cfg = kiss_fftnd_alloc(dims,ndims,isInverseFFT,0,0);
        // preallocation
        cxin  = complexToKiss(comp,dimX,dimY);
        cxout = complexToKiss(comp,dimX,dimY);
        kiss_fftnd(cfg,cxin, cxout);
        complex<float>* fftResult = kissToComplex(cxout,dimX,dimY);

        return fftResult;
    }

    float* calcDiffract(complex<float>* chi, complex<float>* trans, float* oapp, int numPx)
    {
        // want: abs(fft(trans*fft(chi)))

        //in place b/c chi won't be reused
        chi = cmplxFFT(chi,numPx,numPx);
        for(int i = 0; i < numPx*numPx; i++)
        {
            chi[i] = chi[i]*trans[i];
        }
        chi = cmplxFFT(chi,numPx,numPx);
        float* diffInt = new float[numPx*numPx];
        diffInt = complexToReal(chi,numPx,numPx);
        for(int i = 0; i < numPx*numPx; i++)
        {
            diffInt[i] = diffInt[i]*oapp[i]*diffInt[i];
        }
        return diffInt;
    }
    float getPi4Aperture(float * chi0, float* alrr, int numPx){


        float rmax = 1e5;
        for(int i = 0; i < numPx*numPx; i++)
        {
            //1 outside, 0 inside
            float cv = (1-chi0[i])*alrr[i];
            if(cv > 0 && cv < rmax)
            {
                rmax = cv;
            }
        }
        return rmax*1000;
    }

    float* calcRonch(float *buffer, int bufSize) {
        int numPx = static_cast<int>(buffer[0]);
        float al_max = buffer[1]; //mrad
        float obj_ap_r = buffer[2]; //mrad

        float alrr[numPx*numPx];
        float alpp[numPx*numPx];
        float oapp[numPx*numPx];
        polarMeshnOapp(alrr,alpp, oapp, al_max, obj_ap_r, numPx);

        float* sample = generateSample(numPx,numPx,8);

        complex<float>* trans = generateTransmissionFn(sample,numPx,numPx,1);

        float* chi0 = calculateChi0(&buffer[3], &buffer[17], alrr, alpp, numPx, 14);

        complex<float> * chi = calculateChi(chi0, numPx);
        chi0 = maskChi0(chi0,numPx,M_PI/4);
        float* outputScalars = new float[1];
        outputScalars[0] = getPi4Aperture(chi0, alrr, numPx);
        chi0 = normalize(chi0, 255, numPx,numPx);
        float* ronch = normalize(calcDiffract(chi,trans,oapp,numPx),255,numPx,numPx);

        auto arrayPtr = packageOutput(ronch, chi0, outputScalars, numPx, numPx,1);
        return arrayPtr;
    }


}
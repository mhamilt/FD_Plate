//
//  main.cpp
//  FDTD_C_Plate
//
//	A Finite difference plate model in C++. Output will consist of a .wav being
//	produced in the present working directory.
//
//  Created by mhamilt on 09/10/2017.
//  Copyright © 2017 mhamilt. All rights reserved.
//
//==============================================================================
#define pi 3.14159265358979323846
#include <iostream>
#include <cstring>
#include <cmath>

#include "cpp-cli-audio-tools/src/AudioWavFileReadWrite.cpp"

//==============================================================================
// Quick Signum Function used in the initial force
int sgn(double d)
{
  if(d <= 0)
  {
    return 0;
  }
  else
  {
    return 1;
  }
}
//==============================================================================
int main(int argc, const char *argv[])
{
  // This section checks argv[1] and sets the output filename
  // which needs to be of type char* and not const char*

  char *outputfname;
  int length;

  if(!argv[1])

  {
    const char *homedir = getenv("HOME");
    if(!homedir)
    {
      printf("Couldn't find Home Directory. Enter a filename\n");
      return -1;
    }
    printf("NO FILE NAME ENTERED\nUSING DEFAULT FILE DESTINATION\n");
    const char *def_fname = "/Downloads/Plate.wav";
    length = int(strlen(homedir)) + int(strlen(def_fname));
    outputfname = new char[length+1]();
    strncpy(outputfname,homedir, int(strlen(homedir)));
    strcat(outputfname, def_fname);
  }
  else
  {
    length = int(strlen(argv[1]));
    outputfname = new char[length+1]();
    strncpy(outputfname, argv[1], length);
  }
  //--------------------------------------------------------------------------
  // START EDIT HERE
  //--------------------------------------------------------------------------
  //--------------------------------------------------------------------------
  // Flags
  //--------------------------------------------------------------------------

  // Conditions
  bool bctype = 0;          // boundary condition type: 0: simply supported, 1: clamped
  bool outtype = 1;         // output type: 0: displacement, 1: velocity

  //--------------------------------------------------------------------------
  // Parameters
  //--------------------------------------------------------------------------

  // simulation
  double Tf = 2;							// duration
  double nu = .5;							// Poisson Ratios (< .5)
  double ctr [2]=
  {.45, .45};             // centre point of excitation as percentage
  double wid = .25;						// width (m)
  double u0 = 0; double v0 = 1;			// excitation displacement and velocity
  double rp [4] =
  {.45, .65, .85, .15};	// readout position as percentage on grid.

  // physical parameters


  // // wood
  double E = 11e9;						// Young's modulus
  double rho = 480;						// density (kg/m^3)

  // // steel
  //	E = 2e11;							// Young's modulus
  //	rho = 7850;							// density (kg/m^3)

  double H = .005;						// thickness (m)
  double Lx = 1;							// x-axis plate length (m)
  double Ly = 1;							// y-axis plate length (m)
  double loss [4] =
  {100, 8, 1000, 1};    // loss [freq.(Hz), T60;...]

  // I/O
  int OSR = 1;							// Oversampling ratio
  double SR = 44.1e3;						// sample rate (Hz)

  //--------------------------------------------------------------------------
  // END EDIT HERE
  //--------------------------------------------------------------------------

  //--------------------------------------------------------------------------
  // Derived Parameters
  //--------------------------------------------------------------------------

  // Redefine SR from OSR
  SR = OSR*SR;
  // Looping
  int n, yi, xi, cp; // for loop indeces

  // Motion Coefficients
  double D = (E*(pow(H, 3)))/(12*(1-pow(nu,2)));
  double kappa = sqrt(D / (rho*  H) );

  //--------------------------------------------------------------------------
  // Loss coefficients
  //--------------------------------------------------------------------------
  double sigma0 = 0.0;
  double sigma1 = 0.0;
  double z1;
  double z2;


  z1 = 2*kappa*(2*pi*loss[0])/(2*pow(kappa,2));
  z2 = 2*kappa*(2*pi*loss[2])/(2*pow(kappa,2));

  sigma0 = 6*log(10)*(-z2/loss[1] + z1/loss[3])/(z1-z2);
  sigma1 = 6*log(10)*(1/loss[1] - 1/loss[3])/(z1-z2);



  //--------------------------------------------------------------------------
  // Grid Spacing
  //--------------------------------------------------------------------------

  double k = 1/SR;						// time step

  // stability condition
  double hmin = (sqrt(4*k*(sigma1+sqrt(pow(sigma1,2)+pow(kappa,2)))));

  int Nx = floor(Lx/hmin);				// number of segments x-axis
  int Ny = floor(Ly/hmin);				// number of segments y-axis
  double h = sqrt(Lx*Ly/(Nx*Ny));;		// adjusted grid spacing x/y
  Nx = Nx+1; Ny = Ny+1;					// grid point number x and y
  double mu = (kappa * k)/pow(h,2);		// scheme parameter
  int Nf = floor(SR*Tf);					// number of time steps
  int ss = Nx*Ny;							// total grid size.

  //--------------------------------------------------------------------------
  // Allocate Memory
  //--------------------------------------------------------------------------

  double uDATA[ss], u1DATA[ss], u2DATA[ss], out[Nf];
  double * u = uDATA, * u1 = u1DATA, * u2 = u2DATA;
  double *dummy_ptr;

  memset(uDATA, 0, ss * sizeof(double));
  memset(u1DATA, 0, ss * sizeof(double));
  memset(u2DATA, 0, ss * sizeof(double));
  memset(out, 0, Nf * sizeof(double));


  //--------------------------------------------------------------------------
  // Read In/Out
  //--------------------------------------------------------------------------
  int lo, li;
  li = (Ny*( ( (ctr[1]*Nx)-1)) ) + (ctr[0]*Ny)-1;
  lo = (Ny*( ( (rp[1]*Nx)-1)) ) +  (rp[0]*Ny)-1;


  //--------------------------------------------------------------------------
  // Excitation Force
  //--------------------------------------------------------------------------

  double dist, ind, rc, X, Y;


  // raised cosine in 2D
  for(xi=1;xi<Nx-1;xi++)
  {

    X = xi*h;

    for(yi=1;yi<Ny-1;yi++)
    {

      cp = yi+(xi * Ny);

      Y = yi*h;

      dist = sqrt(pow(X-(ctr[0]*Lx),2) + pow(Y-(ctr[1]*Ly),2));

      ind = sgn((wid*0.5)-dist);			// displacement (logical)
      rc = .5*ind*(1+cos(2*pi*dist/wid)); // displacement

      u2[cp] = u0*rc;
      u1[cp] = v0*k*rc;

    }
  }


  //--------------------------------------------------------------------------
  // Coefficient Matrices
  //--------------------------------------------------------------------------

  // coefficients are named based on position on the x and y axes.
  double A00 = 1/(1+k*sigma0); // Central Loss Coeeffient (INVERTED)

  //// Current time step (B) coeffients
  // There are six unique coefficients for B coefs
  double B00 = (-pow(mu,2)*20 + (2*sigma1*k/pow(h,2))*-4 + 2) * A00;	// center
  double B01 = (-pow(mu,2)*-8 + (2*sigma1*k/pow(h,2))) * A00;			// 1-off
  double B11 = (-pow(mu,2)*2) * A00;									// diag
  double B02 = (-pow(mu,2)*1) * A00;									// 2-off

  // Boundary Coefficients
  double BC1, BC2;

  if(bctype)
  {
    BC1 = (-pow(mu,2)*21 + (2*sigma1*k/pow(h,2))*-4 + 2) * A00; // Side
    BC2 = (-pow(mu,2)*22 + (2*sigma1*k/pow(h,2))*-4 + 2) * A00; // Corner
  }
  else
  {
    BC1 = (-pow(mu,2)*19 + (2*sigma1*k/pow(h,2))*-4 + 2) * A00; // Side
    BC2 = (-pow(mu,2)*18 + (2*sigma1*k/pow(h,2))*-4 + 2) * A00; // Corner
  }

  //// Previous time step (C) coeffients
  double C00 = (-(2*sigma1*k/pow(h,2))*-4 - (1-sigma0*k))  * A00;
  double C01 = -(2*sigma1*k/pow(h,2))  * A00;

  //--------------------------------------------------------------------------
  // Print Scheme Info
  //--------------------------------------------------------------------------


  printf("--- Coefficient Info --- \n\n");
  printf("Loss A		: %.4fm \n", A00);
  printf("Centre B    : %.4fm \n", B00);
  printf("1-Grid B    : %.4fm \n", B01);
  printf("2-Grid B	: %.4fm \n", B02);
  printf("Diagonal B  : %.4fm \n", B11);
  printf("Centre C	: %.4fm \n", C00);
  printf("1-Grid C    : %.4fm \n", C01);
  printf("Side Bound	: %.4fm \n", BC1);
  printf("Cornr Bound : %.4fm \n", BC2);

  printf("\n--- Scheme Info --- \n\n");
  printf("Size		: %.1fm2 \n", Nx*h*Ny*h);
  printf("Grid X-Ax   : %d \n", Nx);
  printf("Grid Y-Ax   : %d \n", Ny);
  printf("Total P		: %d \n", ss);
  printf("Dur(samps)	: %d \n", Nf);
  printf("In_cell		: %d\n", li);
  printf("Out_cell	: %d\n", lo);
  printf("Youngs		: %.2e\n", E);
  printf("Sigma 0		: %f\n", sigma0);
  printf("Sigma 1		: %f\n", sigma1);


  // START CLOCK
  std::clock_t start;
  double duration;
  start = std::clock();

  //--------------------------------------------------------------------------
  // Main Loop
  //--------------------------------------------------------------------------

  for(n=0;n<Nf;n++)
  {
    for(xi=Nx-4; xi--; )
    {
      for(yi=Ny-4;yi--; )
      {
        cp = (yi+2)+((xi+2) * Ny); // current point

        u[cp] = B00*u1[cp] +
        B01*( u1[cp-1] + u1[cp+1] + u1[cp-Ny] + u1[cp+Ny] ) +
        B02*( u1[cp-2] + u1[cp+2] +u1[cp-(2*Ny)] + u1[cp+(2*Ny)] ) +
        B11*( u1[cp-1-Ny] + u1[cp+1-Ny] +u1[cp+1+Ny] + u1[cp-1+Ny] ) +
        C00*u2[cp] +
        C01*( u2[cp-1] + u2[cp+1] + u2[cp-Ny] + u2[cp+Ny] );
      }
    }

    // Update Side Boundaries
    //X-Axis

    for(xi=Nx-4; xi--; )
    {
      //North

      cp = 1+((xi+2) * Ny); // current point
      u[cp]  = BC1*u1[cp] +
      B01*( u1[cp+1] + u1[cp-Ny] + u1[cp+Ny] ) +
      B02*( u1[cp-2] + u1[cp+2] +u1[cp-(2*Ny)] + u1[cp+(2*Ny)] ) +
      B11*( u1[cp+1-Ny] + u1[cp+1+Ny] ) +
      C00*u2[cp] +
      C01*( u2[cp+1] + u2[cp-Ny] + u2[cp+Ny] );

      //South

      cp = Ny-2 +((xi+2) * Ny); // current point
      u[cp]  = BC1*u1[cp] +
      B01*( u1[cp-1] + u1[cp-Ny] + u1[cp+Ny] ) +
      B02*( u1[cp-2] + u1[cp-(2*Ny)] + u1[cp+(2*Ny)] ) +
      B11*( u1[cp-1-Ny] + u1[cp-1+Ny] ) +
      C00*u2[cp] +
      C01*( u2[cp-1] + u2[cp-Ny] + u2[cp+Ny] );


    }

    // Y-Axis

    for(yi=Ny-4;yi--; )
    {
      //West

      cp = yi+Ny+2; // current point
      u[cp]  = BC1*u1[cp] +
      B01*( u1[cp-1] + u1[cp+1] + u1[cp+Ny] ) +
      B02*( u1[cp-2] + u1[cp+2] + u1[cp+(2*Ny)] ) +
      B11*( u1[cp+1+Ny] + u1[cp-1+Ny] ) +
      C00*u2[cp] +
      C01*( u2[cp-1] + u2[cp+1] + u2[cp+Ny] );

      //East

      cp = (yi+2) + Ny*(Nx-2); // current point
      u[cp]  = BC1*u1[cp] +
      B01*( u1[cp-1] + u1[cp+1] + u1[cp-Ny] ) +
      B02*( u1[cp-2] + u1[cp+2] +u1[cp-(2*Ny)] ) +
      B11*( u1[cp-1-Ny] + u1[cp+1-Ny] ) +
      C00*u2[cp] +
      C01*( u2[cp-1] + u2[cp+1] + u2[cp-Ny] );

    }

    // Corner Boundaries

    cp = Ny+1;
    u[cp] = BC2*u1[cp] +
    B01*( u1[cp-1] + u1[cp+1] + u1[cp-Ny] + u1[cp+Ny] ) +
    B02*( u1[cp+2] + u1[cp+(2*Ny)] ) +
    B11*( u1[cp-1-Ny] + u1[cp+1-Ny] +u1[cp+1+Ny] + u1[cp-1+Ny] ) +
    C00*u2[cp] +
    C01*( u2[cp-1] + u2[cp+1] + u2[cp-Ny] + u2[cp+Ny] );

    cp = 2*(Ny-1);
    u[cp] = BC2*u1[cp] +
    B01*( u1[cp-1] + u1[cp+1] + u1[cp-Ny] + u1[cp+Ny] ) +
    B02*( u1[cp-2] + u1[cp+(2*Ny)] ) +
    B11*( u1[cp-1-Ny] + u1[cp+1-Ny] +u1[cp+1+Ny] + u1[cp-1+Ny] ) +
    C00*u2[cp] +
    C01*( u2[cp-1] + u2[cp+1] + u2[cp-Ny] + u2[cp+Ny] );

    cp = Ny*(Nx-2)+1;
    u[cp] = BC2*u1[cp] +
    B01*( u1[cp-1] + u1[cp+1] + u1[cp-Ny] + u1[cp+Ny] ) +
    B02*( u1[cp+2] + u1[cp-(2*Ny)] ) +
    B11*( u1[cp-1-Ny] + u1[cp+1-Ny] +u1[cp+1+Ny] + u1[cp-1+Ny] ) +
    C00*u2[cp] +
    C01*( u2[cp-1] + u2[cp+1] + u2[cp-Ny] + u2[cp+Ny] );

    cp = Ny*(Nx-1) - 2;
    u[cp] = BC2*u1[cp] +
    B01*( u1[cp-1] + u1[cp+1] + u1[cp-Ny] + u1[cp+Ny] ) +
    B02*( u1[cp-2] + u1[cp-(2*Ny)] ) +
    B11*( u1[cp-1-Ny] + u1[cp+1-Ny] +u1[cp+1+Ny] + u1[cp-1+Ny] ) +
    C00*u2[cp] +
    C01*( u2[cp-1] + u2[cp+1] + u2[cp-Ny] + u2[cp+Ny] );

    // set output array
    if(outtype)
    {
      out[n] = SR*(u[lo]- u1[lo]); // Velocity out
    }
    else
    {
      out[n] = u[lo]; // Amplitude out
    }
        
    dummy_ptr = u2; u2 = u1; u1 = u; u = dummy_ptr; // swap pointers

  }

  //--------------------------------------------------------------------------
  // Analysis
  //--------------------------------------------------------------------------

  //END CLOCK
  duration = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
  std::cout<<"Time Elapsed (seconds): "<< duration <<'\n';
  //END CLOCK

  std::cout<<"Last Sample Value: "<< out[Nf-1] <<'\n';

  //--------------------------------------------------------------------------
  // Output
  //--------------------------------------------------------------------------
  // AudioWavFileReadWrite rw;
  AudioWavFileReadWrite rw;
  rw.writeWavMS(out, outputfname, Nf, SR);

  printf("\nComplete...\n");

  return 0;

}

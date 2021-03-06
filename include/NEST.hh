#ifndef __NEST_H__
#define __NEST_H__ 1

/*

  NEST.hh and NEST.cpp are the core, the heart, the most important parts of the
  code and model.
  Here the mean light and charge yields are produced but also the fluctuations.
  The yields and fluctuated quanta are functions of the particle type, energy,
  field, and density/phase.
  The raw numbers of photons and electrons are converting into S1 and S2 as
  observed in real life.
  S1 and S2 totals have arrival timing and basic waveform digitization sim added
  on top of them well.
  Lastly, the NEST functions handle a few more advanced detector property
  calculations.
  You'll find the drift v, density as a function of T and P, and XY position
  resolution smearing.

*/

/* *** A word on the units for variables! ***

   distances/lengths are typically mm although a few may be in cm. Velocity
   always uses mm and field always uses cm
   energies are in keV. Work functions are eV sometimes and converted internally
   inside of functions
   fields can be either V/cm or kV/cm, depending on what is most natural for a
   given problem
   times generally end up in ns (and NOT 100 MHz "samples" e.g.) If us or rarely
   ms they're each ultimately converted
   - Microseconds are used as the initial units for drift time, before final
   conversions into ns for digitized pulses
   The magnitude of the drift velocity is given in mm per us, another exception
   to the ns rule
   Temperatures must be in Kelvin not Celsius and pressures are always in bar
   (absolute)
   S1 and S2 units vary from photons and electrons to photo-electrons (labeled
   PE or phe since there are 2 conventions)
   - phd or dph or "photons detected" or "detected photons" refers to PE after
   2pe effect is approximately averaged out (reduction by ~20%)
   - Spikes refer to an attempt at photon counting, hits in PMTs, improving S1
   resolution compared to traditional smeared area
   Even before XYZ position corrections, a spike does not have to be a whole
   number due to things like probabilistic overlap bias correction
   Expect mass densities to be in grams per cm^3 or mL. If atomic density is
   needed, then we derive it
   Particles like photons, electrons, excitons, ions are unit-less, as are the
   "Nuisance Parameters" (multiplicative factors on yields)
   - Event number also dimensionless, and follows C++ convention of starting at
   0, not at 1, for the first event in your simulation
   Probabilities are all initially fractional even if converted to percentages
   for display purposes
   A=atomic mass, Z=mass number (#protons) for simulating the yields of random
   non-noble ions/atoms traveling through medium.
   - These are round numbers even if input as double's since go into
   calculations with other doubles as a mixture
   The interaction type ("species") is a new type defined for use in NEST that
   attempts to cover all possible kinds of particle/scatter
   For waveforms (with output timing set to TRUE) the unit is PE per bin
   (sample, usually 10 ns) but has option of total area in PE

*/

#include "RandomGen.hh"
#include "VDetector.hh"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>

#define W_DEFAULT 13.7  // default work function, in eV
#define W_SCINT 8.5e-3  // the *max* possible energy of 1 scint phot, keV
#define NEST_AVO 6.0221409e+23
#define ATOM_NUM 54.         // period to make float
#define MOLAR_MASS 131.293   // grams per mole
#define PHE_MIN 1e-6         // area
#define ELEC_MASS 9.109e-31  // kg
#define FIELD_MIN 1.         // min elec field to make S2 (in V/cm)
#define DENSITY 2.90         // g/cm^3, ref density for dependent effects

#define EPS_GAS 1.00126
#define EPS_LIQ \
  1.85  // LXe dielectric constant explicitly NOT 1.96 (old). Update thx to Dan
        // McK.

#define SAMPLE_SIZE 10  // nano-seconds
#define PULSE_WIDTH 10  // nano-seconds
#define PULSEHEIGHT \
  0.005                 // threshold height, in PE, for writing to photon_times
#define SPIKES_MAXM 70  // above this switch to pulse area

namespace NEST {

typedef enum {

  // nuclear recoil
  NR = 0,
  WIMP = 1,
  B8 = 2,
  DD = 3,
  AmBe = 4,
  Cf = 5,
  ion = 6,  // includes alphas, Pb-206
  // electron recoil
  gammaRay = 7,
  beta = 8,
  CH3T = 9,
  C14 = 10,
  Kr83m = 11,
  NoneType = 12

} INTERACTION_TYPE;

struct YieldResult {
  double PhotonYield;
  double ElectronYield;
  double ExcitonRatio;
  double Lindhard;
  double ElectricField;
  double DeltaT_Scint;
};

struct QuantaResult {
  int photons;
  int electrons;
  int ions;
  int excitons;
};

typedef std::vector<double> photonstream;

struct NESTresult {
  YieldResult yields;
  QuantaResult quanta;
  photonstream photon_times;
};

class NESTcalc {
 protected:
  VDetector* fdetector;

 private:
  ofstream pulseFile;
  long double Factorial(double x);
  double nCr(double n, double r);

 public:
  NESTcalc();
  NESTcalc(VDetector* detector);
  ~NESTcalc();

  long BinomFluct(long, double);
  // basic binomial fluctuation, which switches to Gaussian for large numbers of
  // quanta, this is called repeatedly, and built upon to produce greater,
  // non-binomial fluctuations
  NESTresult FullCalculation(INTERACTION_TYPE species, double energy,
                             double density, double dfield, double A, double Z,
                             std::vector<double> NuisParam = {1, 1}, bool do_times=true);
  // the so-called full NEST calculation puts together all the individual
  // functions/calculations below
  double PhotonTime(INTERACTION_TYPE species, bool exciton, double dfield,
                    double energy);
  // gives you the birth times of S1 as well as S2 scintillation photons, taking
  // singlet, triplet, and recombination times into account, depending on
  // particle, energy, field
  photonstream AddPhotonTransportTime(photonstream emitted_times, double x,
                                      double y, double z);
  // adds an approximately ray-traced (analytical function) photon travel time
  // in the detector to each photon birth time
  photonstream GetPhotonTimes(INTERACTION_TYPE species, int total_photons,
                              int excitons, double dfield, double energy);
  // this function loops over the photon times above to add times to multiple
  // photons in a loop
  YieldResult GetYields(INTERACTION_TYPE species, double energy, double density,
                        double dfield, double A, double Z,
                        std::vector<double> NuisParam);
  // the innermost heart of NEST, this provides floating-point average values
  // for photons and electrons per keV. Nuis(ance)Param included for varying the
  // NR Ly & Qy up and down
  QuantaResult GetQuanta(YieldResult yields, double density);
  // GetQuanta takes the yields from above and fluctuates them, both the total
  // quanta (photons+electrons) with a Fano-like factor, and the "slosh" between
  // photons and electrons
  // Namely, the recombination fluctuations
  std::vector<double> GetS1(QuantaResult quanta, double truthPos[3],
                            double smearPos[3], double driftSpeed,
                            double dS_mid, INTERACTION_TYPE species,
                            long evtNum, double dfield, double energy,
                            int useTiming, bool outputTiming,
                            vector<long int>& wf_time, vector<double>& wf_amp);
  // Very comprehensive conversion of the "original" intrinsic scintillation
  // photons into the many possible definitions of S1 as measured by
  // photo-sensors
  std::vector<double> GetSpike(int Nph, double dx, double dy, double dz,
                               double driftSpeed, double dS_mid,
                               std::vector<double> origScint);
  // GetSpike takes the extremely basic digital/integer number of spike counts
  // provided by GetS1 and does more realistic smearing
  std::vector<double> GetS2(int Ne, double truthPos[3], double smearPos[3],
                            double dt, double driftSpeed, long evtNum,
                            double dfield, int useTiming, bool outputTiming,
                            vector<long int>& wf_time, vector<double>& wf_amp,
                            vector<double>& g2_params);
  // Exhaustive conversion of the intrinsic ionization electrons into the many
  // possible definitions of S2 pulse areas as observed in the photo-tubes
  // This function also applies the extraction efficiency (binomial) and finite
  // electron mean free path or life time caused by electronegative impurities
  // (exponential)
  std::vector<double> CalculateG2(bool verbosity = true);
  // Calculates "g2" by combining the single electron size with the extraction
  // efficiency. Called by GetS2 above. Includes helper variables like gas gap
  // and SE width.
  double SetDriftVelocity(double T, double D, double F);
  // Gives one the drift velocity as a function of temperature and electric
  // field in liquid or solid. If density implies gas, kicks calculation down to
  // the next function below
  double SetDriftVelocity_MagBoltz(double D, double F);
  // Gas electron drift speed for S2 gas gap in 2-phase TPCs or the whole
  // detector for all gas. Based on simple fits to complicated MagBoltz software
  // output.
  std::vector<double> SetDriftVelocity_NonUniform(double rho, double zStep,
                                                  double dx, double dy);
  // Special handling for the case of non-uniform electric fields in a detector,
  // this integrates over position to find the correct total drift time from any
  // starting point
  double SetDensity(double T, double P);
  // A simple, approximate but good, density is returned for solid, liquid, or
  // gaseous xenon, as a function of temperature and pressure
  std::vector<double> xyResolution(double xPos_mm, double yPos_mm,
                                   double A_top);
  // Utilizing a dependence on radius and the size of the S2 signal, takes MC
  // truth X and Y and outputs smeared values as if you did position
  // reconstruction like in real data
  double PhotonEnergy(bool s2Flag, bool state, double tempK);
  // Determines the birth energies in electron-Volts of scintillation photons,
  // for either S1 or S2, including fluctuations in them, so that you can apply
  // proper QE in G4 for ex.
  double CalcElectronLET(double E);
  // Linear Energy Transfer in units of MeV*cm^2/gram which when combined with
  // density can provide the dE/dx, as a function of energy in keV. Will be more
  // useful in the future
  VDetector* GetDetector() { return fdetector; }
};
}

#endif

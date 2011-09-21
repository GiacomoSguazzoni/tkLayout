#ifndef PTERROR_H
#define PTERROR_H

#include <MaterialProperties.h>


class ptError {
  private:
   
   // Global parameters
   static double IP_length;  // in mm // TODO: setter and getter for this parameter
   static double B;          // in T

   static double minimumPt;
   static double maximumPt;

   // Module parameters
   // (These parameters need get and set) // TODO
   double Module_pitch;    // in mm
   double Module_strip_l;  // in mm
   double Module_z;        // in mm
   double Module_r;        // Module's distance form z axis [mm] 
   double Module_d;        // Module thickness [mm]
   Material material;      // Material amount prior to this module
   int moduleType;
   int endcapType;

   static const double defaultModulePitch = 90/1000.; // 90 um
   static const double defaultStripLength = 23.2; // mm
   static const double defaultModuleZ = 1346; // mm // Disk 1
   static const double defaultModuleR = 1080; // last layer 
   static const double defaultModuleD = 2; // mm

   // Internal computers
   void defaultParameters();
   double oneSidedGaussInt(double x);
   double probabilityInside_norm(double cut, double value);

  public:
   // Constructor and destructor
   ptError() {}
   ~ptError() {}

   // Parameter setters
   void setPitch(double newPitch) { Module_pitch = newPitch ; }
   void setStripLength(double newLength) { Module_strip_l = newLength ; }
   void setZ(double newz) { Module_z = newz ; }
   void setR(double newr) { Module_r = newr ; }
   void setDistance(double newDistance) { Module_d = newDistance ; }
   void setModuleType(int newType) { moduleType = newType ; }
   void setEndcapType(int newType) { endcapType = newType ; }
   void setMaterial(Material newMaterial) { material = newMaterial; }

   // Parameter getters
   double getPitch() { return Module_pitch ; }
   double getStripLength() { return Module_strip_l ; }
   double getZ() { return Module_z ; }
   double getR() { return Module_r ; }
   double getDistance() { return Module_d ; }
   int getModuleType() { return moduleType ; }
   int getEndcapType() { return endcapType ; }
   Material getMaterial() { return material ; }

   // Conversion between strips and p
   double stripsToP(double strips);
   double pToStrips(double p);
   // Error computation
   double computeError(double p);
   // Efficiency computation
   double probabilityInside(double cut, double value, double value_err);
   double geometricEfficiency();
   double find_probability(double target, double originalcut);
};

#endif
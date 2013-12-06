
#include "AnalyzerVisitors/TriggerProcessorBandwidth.h"


using AnalyzerHelpers::Circle;
using AnalyzerHelpers::Point;


std::pair<Circle, Circle> AnalyzerHelpers::findCirclesTwoPoints(const Point& p1, const Point& p2, double r) {
  double x1 = p1.x, y1 = p1.y, x2 = p2.x, y2 = p2.y;

  double q = sqrt(pow(x2-x1,2) + pow(y2-y1,2));
  double x3 = (x1+x2)/2;
  double y3 = (y1+y2)/2;

  double xc1 = x3 + sqrt(r*r - pow(q/2,2))*(y1-y2)/q;
  double yc1 = y3 + sqrt(r*r - pow(q/2,2))*(x2-x1)/q;

  double xc2 = x3 - sqrt(r*r - pow(q/2,2))*(y1-y2)/q;
  double yc2 = y3 - sqrt(r*r - pow(q/2,2))*(x2-x1)/q;

  return std::make_pair((Circle){ xc1, yc1, r }, (Circle){ xc2, yc2, r });
}

bool AnalyzerHelpers::isPointInCircle(const Point& p, const Circle& c) { return pow(p.x - c.x0, 2) + pow(p.y - c.y0, 2) <= c.r*c.r; }



double AnalyzerHelpers::calculatePetalAreaMC(const Tracker& tracker, const SimParms& simParms, double crossoverR) {
  static TRandom3 die;

  double r = simParms.triggerPtCut()/(0.3*insur::magnetic_field) * 1e3; // curvature radius of particles with the minimum accepted pt

  std::pair<Circle, Circle> cc = findCirclesTwoPoints((Point){0, 0}, (Point){0, crossoverR}, r);

  // Monte Carlo area calculation
  int hits = 0;
  double maxR = tracker.maxR(); // points randomly generated in a 40 degrees circle slice
  double minR = tracker.minR();
  double aperture = 0.34906585 * 2; // 40 degrees
  //double maxPhi = M_PI/2 + aperture/2;
  double minPhi = M_PI/2 - aperture/2;
  for (int i = 0; i < 100000; i++) {
    double rr  = minR + die.Rndm()*(maxR-minR);
    double phi = minPhi + die.Rndm()*aperture;
    Polar2DPoint rndp(rr, phi);
    bool inFirstCircle  = isPointInCircle((Point){rndp.X(), rndp.Y()}, cc.first);
    bool inSecondCircle = isPointInCircle((Point){rndp.X(), rndp.Y()}, cc.second);
    if ((inFirstCircle && inSecondCircle) || (!inFirstCircle && !inSecondCircle)) hits++; // if it's in both circles means it's in the lower part of the petal (before the crossover), if it's outside both it means it's upper part of the petal (after the crossover)
  }

  return hits;
}


double AnalyzerHelpers::calculatePetalAreaModules(const Tracker& tracker, const SimParms& simParms, double crossoverR) {
  double curvatureR = simParms.particleCurvatureR(simParms.triggerPtCut()); // curvature radius of particles with the minimum accepted pt
  int numTriggerProcessorsPhi = simParms.numTriggerTowersPhi();

  struct PetalAreaVisitor : public ConstGeometryVisitor {
    double curvatureR_, crossoverR_, numTriggerProcessorsPhi_;  
    int hits = 0;
    PetalAreaVisitor(double curvatureR, double crossoverR, double numTriggerProcessorsPhi) : curvatureR_(curvatureR), crossoverR_(crossoverR), numTriggerProcessorsPhi_(numTriggerProcessorsPhi) {}
    void visit(const Module& m) {
      if (m.side() < 0) return;
      const double petalInterval = 2*M_PI / numTriggerProcessorsPhi_; // aka Psi
      for (int i = 0; i < numTriggerProcessorsPhi_; ++i) {
        if (AnalyzerHelpers::isModuleInPetal(m, petalInterval*i, curvatureR_, crossoverR_)) { hits++; } // we could break after the find found hit, but this way we take into account the (admittedly unlikely) situation of petals being so wide that some modules belong to more than one.
      }
    }
  } v(curvatureR, crossoverR, numTriggerProcessorsPhi);

  tracker.accept(v);

  return v.hits;
}

double AnalyzerHelpers::calculatePetalCrossover(const Tracker& tracker, const SimParms& simParms) {
  class Trampoline : public ROOT::Math::IBaseFunctionOneDim {
    const Tracker& t_;
    const SimParms& s_;
    double DoEval(double x) const { return calculatePetalAreaModules(t_, s_, x); }
  public:
    Trampoline(const Tracker& t, const SimParms& s) : t_(t), s_(s) {}
    ROOT::Math::IBaseFunctionOneDim* Clone() const { return new Trampoline(t_, s_); }
  };

  Trampoline t(tracker, simParms);

  ROOT::Math::BrentMinimizer1D minBrent;
  minBrent.SetFunction(t, 0., tracker.maxR());
  bool ok = minBrent.Minimize(100, 0.001, 0.001);


  if (ok) {
    logINFO("Multiple Trigger Towers: Searching for optimized petal crossover point");
    logINFO("  Method converged after " + any2str(minBrent.Iterations()) + " iterations.");
    logINFO("  Found minimum: crossover point = " + any2str(minBrent.XMinimum()) + "  petal area = " + any2str(minBrent.FValMinimum()));
  } else {
    logERROR("Multiple Trigger Towers: Search for optimized petal crossover point failed");
    logERROR("  Method did not converge after " + any2str(minBrent.Iterations()) + " iterations.");
    logERROR("  Last found value: crossover point = " + any2str(minBrent.XMinimum()) + "  petal area = " + any2str(minBrent.FValMinimum()));
  }

  return minBrent.XMinimum();
}


bool AnalyzerHelpers::isModuleInEtaSector(const SimParms& simParms, const Tracker& tracker, const DetectorModule& module, int etaSector) {
  int numProcEta = simParms.numTriggerTowersEta();
  double etaCut = simParms.triggerEtaCut();
  double etaSlice = etaCut*2 / numProcEta;
  double maxR = tracker.maxR();
  double zError = simParms.zErrorCollider();
  double eta = etaSlice*etaSector-etaCut;    

  double modMinZ = module.minZ();
  double modMaxZ = module.maxZ();
  double modMinR = module.minR();                
  double modMaxR = module.maxR();                

  double etaSliceZ1 = maxR/tan(2*atan(exp(-eta)));
  double etaSliceZ2 = maxR/tan(2*atan(exp(-eta-etaSlice)));

  double etaDist1 =  modMaxZ - ((etaSliceZ1 >= 0 ? modMinR : modMaxR)*(etaSliceZ1 + zError)/maxR - zError); // if etaDists are positive it means the module is in the slice
  double etaDist2 = -modMinZ + ((etaSliceZ2 >= 0 ? modMaxR : modMinR)*(etaSliceZ2 - zError)/maxR + zError); 

  return etaDist1 > 0 && etaDist2 > 0;
}

bool AnalyzerHelpers::isModuleInPetal(const DetectorModule& module, double petalPhi, double curvatureR, double crossoverR) {
  Polar2DPoint crossoverPoint(crossoverR, petalPhi);
  double proj = cos(module.center().Phi() - petalPhi); // check if module is in the same semi-plane as the petal by projecting its center on the petal symmetry line
  if (proj < 0.) return false;
  std::pair<Circle, Circle> cc = findCirclesTwoPoints((Point){0.,0.}, (Point){crossoverPoint.X(), crossoverPoint.Y()}, curvatureR);

  int inFirstCircle = 0, inSecondCircle = 0;
  for (int i = 0; i < 4; i++) {
    const XYZVector& corner = module.basePoly().getVertex(i);
    inFirstCircle  |= (isPointInCircle((Point){corner.X(), corner.Y()}, cc.first) << i);
    inSecondCircle |= (isPointInCircle((Point){corner.X(), corner.Y()}, cc.second) << i);
  }
  return (inFirstCircle && inSecondCircle) || (inFirstCircle < 0xF && inSecondCircle < 0xF);
}


bool AnalyzerHelpers::areClockwise(const Point& p1, const Point& p2) { return -p1.x*p2.y + p1.y*p2.x > 0; }
//#define OLD_PHI_SECTOR_CHECK
//
#ifndef OLD_PHI_SECTOR_CHECK
bool AnalyzerHelpers::isModuleInCircleSector(const DetectorModule& module, double startPhi, double endPhi) {

  Point startArm = {cos(startPhi), sin(startPhi)};
  Point endArm   = {cos(endPhi), sin(endPhi)};

  for (int i = 0; i < 4; i++) {
    const XYZVector& corner = module.basePoly().getVertex(i);
    if (!areClockwise(startArm, (Point){corner.X(), corner.Y()}) && areClockwise(endArm, (Point){corner.X(), corner.Y()})) return true;
  }
  return false;
}
#else
bool AnalyzerHelpers::isModuleInCircleSector(const DetectorModule& module, double sliceMinPhi, double sliceMaxPhi) {

  double modMinPhi = module.minPhi() >= 0 ? module.minPhi() : module.minPhi() + 2*M_PI;
  double modMaxPhi = module.maxPhi() >= 0 ? module.maxPhi() : module.maxPhi() + 2*M_PI;


  if (modMinPhi > modMaxPhi && sliceMaxPhi > 2*M_PI) modMaxPhi += 2*M_PI;      // this solves the issue with modules across the 2 PI line
  else if (modMinPhi > modMaxPhi && sliceMaxPhi < 2*M_PI) modMinPhi -= 2*M_PI; // 

  bool inSectorSlice = ((sliceMinPhi < modMaxPhi && modMinPhi < sliceMaxPhi) ||
                        (sliceMinPhi < modMaxPhi+2*M_PI && modMinPhi+2*M_PI < sliceMaxPhi) || // this catches the modules that are at a small angle but must be caught by a sweep crossing the 2 PI line
                        (sliceMinPhi < modMaxPhi-2*M_PI && modMinPhi-2*M_PI < sliceMaxPhi));

  return inSectorSlice;

}
#endif

bool AnalyzerHelpers::isModuleInPhiSector(const SimParms& simParms, const DetectorModule& module, double crossoverR, int phiSector) {
  static const double curvatureR = simParms.triggerPtCut()/(0.3*insur::magnetic_field) * 1e3; // curvature radius of particles with the minimum accepted pt

  double phiSlice = 2*M_PI / simParms.numTriggerTowersPhi();  // aka Psi
  double phi = phiSlice*phiSector;

  double sliceMinPhi = phi;
  double sliceMaxPhi = phi + phiSlice;

  bool inSectorSlice = isModuleInCircleSector(module, sliceMinPhi, sliceMaxPhi);
  bool inPetals = isModuleInPetal(module, sliceMinPhi, curvatureR, crossoverR) || isModuleInPetal(module, sliceMaxPhi, curvatureR, crossoverR);

  return inSectorSlice || inPetals;
}




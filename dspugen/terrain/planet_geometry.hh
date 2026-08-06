#pragma once

#include "planet_raw_data.hh"
#include "unity_math.hh"

namespace dspugen {

class Planet;

namespace terrain {

struct PlanetPose {
    Vector3 position;
    Quaternion rotation;
};

struct BirthPoints {
    Vector3 birthPoint;
    Vector3 resourcePoint0;
    Vector3 resourcePoint1;
};

PlanetPose predictPose(const Planet &planet, double time);
Vector3 predictLocalStarDirection(const Planet &planet, double time);
BirthPoints generateBirthPoints(const Planet &planet, PlanetRawData &rawData);
BirthPoints generateBirthPoints(const Planet &planet, PlanetRawData &rawData,
                                int birthSeed);
Vector3 rawSnap(Vector3 position, float planetRadius);

}

}

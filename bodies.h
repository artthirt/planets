#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <QString>

// Declarative description of a scene body (planet/moon) and its circular orbit
// around a parent body (or the scene origin when parent == -1).
struct Body
{
    QString name;
    float radius = 1.0f;
    QString texture;              // resource path, e.g. ":/data/8k_jupiter.jpg"

    // orbit (ignored when orbitRadius == 0 -> fixed position)
    float orbitRadius = 0.0f;
    float orbitPeriod = 1.0f;     // time units per full revolution
    float orbitPhaseDeg = 0.0f;
    float orbitInclDeg = 0.0f;    // inclination around X axis
    int parent = -1;              // index into the body list, -1 = origin

    float spinPeriodDeg = 60.0f;  // self-rotation, degrees per time unit

    float specularStrength = 0.05f;
    float shininess = 16.0f;

    // atmosphere (stage C: raymarched gas shell)
    bool atmosphereOn = false;
    QVector3D atmosphereColor{0.30f, 0.52f, 1.0f};
    float atmosphereRadiusScale = 1.05f; // shell outer radius = ratio * radius (hugs the surface)
    float atmosphereDensity = 1.0f;      // base optical density at the surface
    float atmosphereScaleHeightRatio = 0.022f; // scale height = ratio * radius (tight falloff)
    float atmosphereNoise = 0.0f;        // 0..1, FBM detail amount

    // ring (stage D2b: flat annulus in the planet's unit space)
    bool ringOn = false;
    QString ringTexture;          // radial band strip, width = ring radius
    float ringInnerScale = 1.2f;  // inner radius, in planet radii
    float ringOuterScale = 2.35f; // outer radius, in planet radii
    float ringTiltDeg = 0.0f;     // tilt around X (real Saturn: ~27)

    // runtime
    QVector3D position;
    unsigned int texId = 0;
    unsigned int ringTexId = 0;
};

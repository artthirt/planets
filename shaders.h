#pragma once

#include <QOpenGLShaderProgram>
#include <QString>

// Thin wrapper: compiles + links from source, logs errors loudly.
class ShaderProgram : public QOpenGLShaderProgram
{
public:
    // Prints the full driver log + source and exits the process on failure:
    // continuing after a failed compile only leads to confusing secondary
    // GL asserts (unbound program, uninitialized state, ...).
    bool compile(const char *name, const char *vertexSrc, const char *fragmentSrc);
};

namespace spaceShaders
{
// Planet body: world-space Blinn-Phong from a directional sun + faint star
// ambient, optional cheap fresnel rim (stage-A stand-in for atmosphere).
extern const char *kPlanetVertex;
extern const char *kPlanetFragment;

// Star sky sphere (camera-centered) with the sun painted "at infinity".
extern const char *kSkyVertex;
extern const char *kSkyFragment;
}

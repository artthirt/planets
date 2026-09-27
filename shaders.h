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
// ambient.
extern const char *kPlanetVertex;
extern const char *kPlanetFragment;

// Star sky sphere (camera-centered) with the sun painted "at infinity".
extern const char *kSkyVertex;
extern const char *kSkyFragment;

// Volumetric atmosphere (stage C): a raymarched gas shell.
// kAtmShellVertex feeds the ray from the shell sphere (camera outside the
// gas); kAtmFsVertex reconstructs the ray per pixel when the camera is
// inside the gas; kAtmFragment is shared by both passes.
extern const char *kAtmShellVertex;
extern const char *kAtmFsVertex;
extern const char *kAtmFragment;

// Ring disc (stage D2b): flat annulus, radial band texture, two-sided
// lighting, hard planet shadow across the disc. Pairs with kPlanetVertex
// (same inputs/outputs: model/view/proj, pos/normal/uv).
extern const char *kRingFragment;
}

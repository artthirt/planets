#include "shaders.h"

#include <QOpenGLShader>
#include <QOpenGLShaderProgram>

#include <cstdio>
#include <cstdlib>

namespace spaceShaders
{

const char *kPlanetVertex = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTex;

void main()
{
    vec4 wp = uModel * vec4(aPos, 1.0);
    vWorldPos = wp.xyz;
    // uniform scale only -> mat3(uModel) is a valid normal transform
    vNormal = normalize(mat3(uModel) * aNormal);
    vTex = aTex;
    gl_Position = uProj * uView * wp;
}
)glsl";

const char *kPlanetFragment = R"glsl(
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTex;

out vec4 fragColor;

uniform sampler2D uTex;
uniform vec3 uCamPos;
uniform vec3 uSunDir;      // normalized, points toward the sun
uniform vec3 uSunColor;
uniform vec3 uAtmColor;
uniform float uAtmOn;      // 0/1, cheap fresnel rim preview
uniform float uSpec;
uniform float uShin;

void main()
{
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 L = normalize(uSunDir);

    // NOTE: no `const` on runtime-initialized locals — in GLSL that requires a
    // compile-time constant expression (error C1059 on NVIDIA).
    float diff = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), uShin) * uSpec;

    vec3 texc = texture(uTex, vTex).rgb;

    // day side lit by the distant sun, night side gets faint starlight
    vec3 col = texc * (uSunColor * diff + vec3(0.015, 0.02, 0.03));

    // glint (ice / ocean)
    col += uSunColor * spec * texc;

    // cheap fresnel rim, stands in for the volumetric atmosphere (stage C)
    float fres = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    float day = smoothstep(-0.15, 0.35, dot(N, L));
    col += uAtmColor * fres * (0.25 + 0.75 * day) * uAtmOn;

    fragColor = vec4(col, 1.0);
}
)glsl";

const char *kSkyVertex = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;

uniform mat4 uModel;   // translate(camPos) * scale(skyRadius)
uniform mat4 uView;
uniform mat4 uProj;
uniform vec3 uCamPos;

out vec3 vDir;         // world direction from camera to this fragment
out vec2 vTex;

void main()
{
    vec4 wp = uModel * vec4(aPos, 1.0);
    vDir = wp.xyz - uCamPos;
    vTex = aTex;
    gl_Position = uProj * uView * wp;
}
)glsl";

const char *kSkyFragment = R"glsl(
#version 330 core
in vec3 vDir;
in vec2 vTex;

out vec4 fragColor;

uniform sampler2D uSkyTex;
uniform vec3 uSunDir;    // normalized, points toward the sun
uniform vec3 uSunColor;

void main()
{
    // (GLSL: runtime-initialized locals must not be `const` — error C1059)
    vec3 dir = normalize(vDir);
    vec3 col = texture(uSkyTex, vTex).rgb;

    // the sun lives at infinity on the sky dome: disc + inner glow + wide halo
    float s = max(dot(dir, normalize(uSunDir)), 0.0);
    col += uSunColor * (pow(s, 1024.0) * 4.0
                      + pow(s, 128.0) * 0.5
                      + pow(s, 16.0) * 0.08);

    fragColor = vec4(col, 1.0);
}
)glsl";

}

namespace
{
// A shader that fails to compile leaves GL state inconsistent; continuing
// only produces confusing secondary asserts. Print the full driver log and
// the source, then stop the process so the error is impossible to miss.
void shaderFail(const char *what, const char *name, const char *src, const char *log)
{
    fprintf(stderr,
            "\n================= SHADER FAILURE: %s (%s) =================\n"
            "--- driver log ---\n%s\n"
            "--- source ---\n%s\n"
            "Exiting: refusing to continue with a broken program.\n\n",
            name, what, log, src);
    fflush(stderr);
    std::exit(1);
}
}

bool ShaderProgram::compile(const char *name, const char *vertexSrc, const char *fragmentSrc)
{
    const QString vsSrc = QString::fromUtf8(vertexSrc);
    QOpenGLShader vs(QOpenGLShader::Vertex);
    if (!vs.compileSourceCode(vsSrc))
        shaderFail("vertex", name, vertexSrc, vs.log().toUtf8().constData());

    const QString fsSrc = QString::fromUtf8(fragmentSrc);
    QOpenGLShader fs(QOpenGLShader::Fragment);
    if (!fs.compileSourceCode(fsSrc))
        shaderFail("fragment", name, fragmentSrc, fs.log().toUtf8().constData());

    addShader(&vs);
    addShader(&fs);

    if (!link())
        shaderFail("link", name, vertexSrc, log().toUtf8().constData());

    return true;
}

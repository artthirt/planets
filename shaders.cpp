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
uniform vec4 uOccluders[8]; // other bodies: xyz center, w radius (shadows)
uniform int uOccluderCount;
uniform float uSunAngle;   // apparent sun radius, radians (penumbra width)

// Does any occluder sphere block the ray p -> p + t*dir (t > 0)?
float occluded(vec3 p, vec3 dir)
{
    for (int i = 0; i < uOccluderCount; i++) {
        vec3 d = uOccluders[i].xyz - p;
        float b = dot(d, dir);
        if (b <= 0.0)
            continue; // sphere behind the point
        float r2 = dot(d, d) - uOccluders[i].w * uOccluders[i].w;
        if (r2 >= b * b)
            continue; // ray misses the sphere
        if (b - sqrt(b * b - r2) > 0.0)
            return 1.0;
    }
    return 0.0;
}

// Soft shadow: the painted sun has an apparent disc, so sample it —
// 1 = full sunlight, 0 = deep umbra, gradient in between (penumbra).
float sunShadowAtt(vec3 p, vec3 L)
{
    vec3 ref = abs(L.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 t1 = normalize(cross(ref, L));
    vec3 t2 = cross(L, t1);
    float a = uSunAngle; // no const: a uniform is not a compile-time constant (C1059)

    float occ = occluded(p, L); // disc center
    for (int i = 0; i < 4; i++) {
        float ang = 0.7853982 * (1.0 + 2.0 * float(i));
        vec3 d = L * cos(a * 0.55) + (t1 * cos(ang) + t2 * sin(ang)) * sin(a * 0.55);
        occ += occluded(p, normalize(d));
    }
    for (int i = 0; i < 4; i++) {
        float ang = 1.5707963 * float(i);
        vec3 d = L * cos(a) + (t1 * cos(ang) + t2 * sin(ang)) * sin(a);
        occ += occluded(p, normalize(d));
    }
    return 1.0 - occ / 9.0;
}

void main()
{
    vec3 N = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 L = normalize(uSunDir);

    // NOTE: no `const` on runtime-initialized locals — in GLSL that requires a
    // compile-time constant expression (error C1059 on NVIDIA).
    float diff = max(dot(N, L), 0.0);
    float shadowAtt = sunShadowAtt(vWorldPos, L);
    float lit = diff * shadowAtt;
    vec3 H = normalize(L + V);
    // glint only where the sun actually hits: no specular on the night side
    float spec = pow(max(dot(N, H), 0.0), uShin) * uSpec * lit;

    vec3 texc = texture(uTex, vTex).rgb;

    // day side lit by the distant sun; the night side in deep space is
    // essentially black (only a whisper of starlight, which nothing blocks)
    vec3 col = texc * (uSunColor * lit + vec3(0.003, 0.004, 0.006));

    // glint (ice / ocean)
    col += uSunColor * spec * texc;

    // cheap fresnel rim, stands in for the volumetric atmosphere (stage C);
    // the night limb stays dark, and the lit limb fades inside a shadow
    float fres = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    float day = smoothstep(-0.15, 0.35, dot(N, L));
    col += uAtmColor * fres * (0.05 + 0.95 * day) * uAtmOn * shadowAtt;

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

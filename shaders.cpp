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
uniform float uSpec;
uniform float uShin;
uniform vec4 uOccluders[16]; // other bodies: xyz center, w radius (shadows)
uniform int uOccluderCount;
uniform float uSunAngle;   // apparent sun radius, radians (penumbra width)
uniform vec3 uRingCenter;  // own ring: plane through this point
uniform vec3 uRingNormal;  // ... with this normal (tilted)
uniform float uRingInner;  // annulus radii, world units
uniform float uRingOuter;
uniform int uRingOn;       // 1 = the body has a ring that can shadow it
uniform sampler2D uRingTex;
uniform sampler2D uNormalMap;   // tangent-space normal map (texture unit 2)
uniform float uNormalStrength;  // 0 = flat shading (no bump)

// How much of the direct sunlight does the body's own ring block at p?
// The ring is a flat annulus, so its shadow is a sharp band: the ray
// p -> p + t*L crosses the ring plane exactly once.
float ringShadow(vec3 p, vec3 L)
{
    if (uRingOn == 0)
        return 0.0;
    float dn = dot(L, uRingNormal);
    if (abs(dn) < 0.0001)
        return 0.0; // sun in the ring plane: rays never cross it
    float t = dot(uRingCenter - p, uRingNormal) / dn;
    if (t <= 0.0)
        return 0.0; // crossing behind the point
    float r = length(p + L * t - uRingCenter);
    if (r < uRingInner || r > uRingOuter)
        return 0.0; // crossing outside the annulus
    float u = (r - uRingInner) / (uRingOuter - uRingInner);
    return texture(uRingTex, vec2(u, 0.5)).a;
}

// Area of overlap of two discs (radii r1, r2; center distance d, all in
// radians). Closed form, degenerate cases handled.
float discOverlap(float r1, float r2, float d)
{
    if (d >= r1 + r2)
        return 0.0;
    if (d <= max(r1, r2) - min(r1, r2))
        return 3.14159265 * min(r1, r2) * min(r1, r2);
    float c1 = clamp((d * d + r1 * r1 - r2 * r2) / (2.0 * d * r1), -1.0, 1.0);
    float c2 = clamp((d * d + r2 * r2 - r1 * r1) / (2.0 * d * r2), -1.0, 1.0);
    float tri = 0.5 * sqrt(max((-d + r1 + r2) * (d + r1 - r2) * (d - r1 + r2) * (d + r1 + r2), 0.0));
    return r1 * r1 * acos(c1) + r2 * r2 * acos(c2) - tri;
}

// Soft shadow: the painted sun has an apparent disc (radius uSunAngle), so
// the true attenuation at p is the fraction of that disc blocked. Compute
// it exactly per occluder: the sphere subtends a disc of radius asin(r/D)
// at p, and the blocked fraction is the overlap of the two discs.
// Stage B's fixed 9-tap grid aliased small moon shadows into a rosette of
// spots (one per tap); the analytic form is smooth at any occluder size.
float sunShadowAtt(vec3 p, vec3 L)
{
    float a = uSunAngle;
    float att = 1.0; // product of per-occluder unblocked fractions
    for (int i = 0; i < uOccluderCount; i++) {
        vec3 d = uOccluders[i].xyz - p;
        float D = length(d);
        float r = uOccluders[i].w;
        if (D < r)
            return 0.0; // point inside the occluder
        float alpha = asin(clamp(r / D, 0.0, 1.0));
        float delta = acos(clamp(dot(L, d / D), -1.0, 1.0));
        float blocked = clamp(discOverlap(a, alpha, delta) / (3.14159265 * a * a), 0.0, 1.0);
        att *= 1.0 - blocked;
        if (att < 1e-4)
            break; // deep umbra: nothing left to darken
    }
    return att;
}

void main()
{
    vec3 Ngeo = normalize(vNormal);
    vec3 V = normalize(uCamPos - vWorldPos);
    vec3 L = normalize(uSunDir);

    // Tangent-space bump/normal mapping (stage D12). The frame is built from
    // screen-space derivatives: T ~ dP/du, B ~ dP/dv (the parameterization
    // directions, not cross(N,T) — this sphere is left-handed in (u,v)). The
    // maps use the standard encoding (R ~ -dh/du, G ~ -dh/dv), so both the
    // authored maps and the ones generated from albedo luminance work as-is.
    vec3 N = Ngeo;
    if (uNormalStrength > 0.0) {
        vec3 dpdx = dFdx(vWorldPos);
        vec3 dpdy = dFdy(vWorldPos);
        vec2 duvdx = dFdx(vTex);
        vec2 duvdy = dFdy(vTex);
        float det = duvdx.x * duvdy.y - duvdx.y * duvdy.x;
        vec3 T, B;
        if (abs(det) > 1e-10) {
            T = (duvdy.y * dpdx - duvdx.y * dpdy) / det;
            B = (duvdx.x * dpdy - duvdy.x * dpdx) / det;
            T = normalize(T - Ngeo * dot(T, Ngeo));
            B = normalize(B - Ngeo * dot(B, Ngeo));
        } else {
            // degenerate pixel (e.g. collapsed pole row): any tangent frame
            vec3 f = abs(Ngeo.y) < 0.99 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
            T = normalize(cross(f, Ngeo));
            B = cross(Ngeo, T);
        }
        vec3 nm = texture(uNormalMap, vTex).rgb * 2.0 - 1.0;
        nm.xy *= uNormalStrength;
        N = normalize(T * nm.x + B * nm.y + Ngeo * nm.z);
    }

    // NOTE: no `const` on runtime-initialized locals — in GLSL that requires a
    // compile-time constant expression (error C1059 on NVIDIA).
    float diff = max(dot(N, L), 0.0);
    float shadowAtt = sunShadowAtt(vWorldPos, L);
    float lit = diff * shadowAtt * (1.0 - ringShadow(vWorldPos, L));
    vec3 H = normalize(L + V);
    // glint only where the sun actually hits: no specular on the night side
    float spec = pow(max(dot(N, H), 0.0), uShin) * uSpec * lit;

    vec3 texc = texture(uTex, vTex).rgb;

    // day side lit by the distant sun; the night side in deep space is
    // essentially black (only a whisper of starlight, which nothing blocks)
    vec3 col = texc * (uSunColor * lit + vec3(0.003, 0.004, 0.006));

    // glint (ice / ocean)
    col += uSunColor * spec * texc;

    // the atmosphere is a separate raymarched pass (kAtmFragment)

    fragColor = vec4(col, 1.0);
}
)glsl";

const char *kAtmShellVertex = R"glsl(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;

uniform mat4 uModel;   // translate(planetCenter) * scale(shellRadius)
uniform mat4 uView;
uniform mat4 uProj;
uniform vec3 uCamPos;

out vec3 vRayDir;      // world direction, camera -> this fragment

void main()
{
    vec4 wp = uModel * vec4(aPos, 1.0);
    vRayDir = wp.xyz - uCamPos;
    gl_Position = uProj * uView * wp;
}
)glsl";

const char *kAtmFsVertex = R"glsl(
#version 330 core
// Fullscreen triangle (aPos = NDC xy); used when the camera is INSIDE the
// gas, where the shell geometry has the wrong depth for compositing.
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTex;

uniform mat4 uInvViewProj;
uniform vec3 uCamPos;

out vec3 vRayDir;

void main()
{
    gl_Position = vec4(aPos.xy, 0.0, 1.0);
    vec4 wp = uInvViewProj * vec4(aPos.xy, 0.0, 1.0);
    vRayDir = wp.xyz / wp.w - uCamPos;
}
)glsl";

const char *kAtmFragment = R"glsl(
#version 330 core
// Raymarched gas: exponential altitude density, per-sample sunlight.
// Shared by the shell pass (camera outside) and the fullscreen pass
// (camera inside the gas). Outputs premultiplied color: composite with
// blend func (ONE, ONE_MINUS_SRC_ALPHA).
// (GLSL: runtime-initialized locals must not be `const` — error C1059)
in vec3 vRayDir;

out vec4 fragColor;

uniform vec3 uCamPos;
uniform vec3 uCenter;      // planet center
uniform float uPlanetR;    // solid-surface radius
uniform float uShellR;     // outer radius of the gas
uniform vec3 uSunDir;      // normalized
uniform vec3 uSunColor;
uniform vec3 uAtmColor;
uniform float uDensity;    // base optical density at the surface
uniform float uScaleH;     // exponential scale height (world units)
uniform sampler2D uTex;    // planet surface texture: luminance masks the gas
uniform float uSpin;       // planet spin angle, radians (world Y)
uniform float uTime;       // scene time, drives the slow gas drift
uniform float uNoiseAmt;   // 0..1, FBM detail amount (0 = smooth gas)
uniform vec4 uOccluders[16]; // solid bodies: xyz center, w radius (sun shadows)
uniform int uOccluderCount;
uniform float uSunAngle;   // apparent sun radius, radians (penumbra width)
uniform vec3 uRingCenter;  // own ring: plane through this point
uniform vec3 uRingNormal;  // ... with this normal (tilted)
uniform float uRingInner;  // annulus radii, world units
uniform float uRingOuter;
uniform int uRingOn;       // 1 = the body has a ring that can shadow its gas
uniform sampler2D uRingTex;

float hash13(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

float vnoise(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash13(i);
    float n100 = hash13(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash13(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash13(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash13(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash13(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash13(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash13(i + vec3(1.0, 1.0, 1.0));
    return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
               mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y),
               f.z);
}

float fbm(vec3 p)
{
    float s = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; i++) {
        s += a * vnoise(p);
        p = p * 2.03 + vec3(11.31);
        a *= 0.5;
    }
    return s; // ~[0, 0.94]
}

// Area of overlap of two discs (radii r1, r2; center distance d, in
// radians). Closed form, degenerate cases handled. (Same as the planet
// shader, stage D8.)
float discOverlap(float r1, float r2, float d)
{
    if (d >= r1 + r2)
        return 0.0;
    if (d <= max(r1, r2) - min(r1, r2))
        return 3.14159265 * min(r1, r2) * min(r1, r2);
    float c1 = clamp((d * d + r1 * r1 - r2 * r2) / (2.0 * d * r1), -1.0, 1.0);
    float c2 = clamp((d * d + r2 * r2 - r1 * r1) / (2.0 * d * r2), -1.0, 1.0);
    float tri = 0.5 * sqrt(max((-d + r1 + r2) * (d + r1 - r2) * (d - r1 + r2) * (d + r1 + r2), 0.0));
    return r1 * r1 * acos(c1) + r2 * r2 * acos(c2) - tri;
}

// How much of the direct sunlight does the body's own ring block at p?
// The ring is a flat annulus, so its shadow is a sharp band: the ray
// p -> p + t*L crosses the ring plane exactly once. (Same as the planet
// shader, stage D2c — without this the band washes out toward the limb,
// where the unshadowed gas dominates.)
float ringShadow(vec3 p, vec3 L)
{
    if (uRingOn == 0)
        return 0.0;
    float dn = dot(L, uRingNormal);
    if (abs(dn) < 0.0001)
        return 0.0; // sun in the ring plane: rays never cross it
    float t = dot(uRingCenter - p, uRingNormal) / dn;
    if (t <= 0.0)
        return 0.0; // crossing behind the point
    float r = length(p + L * t - uRingCenter);
    if (r < uRingInner || r > uRingOuter)
        return 0.0; // crossing outside the annulus
    float u = (r - uRingInner) / (uRingOuter - uRingInner);
    return texture(uRingTex, vec2(u, 0.5)).a;
}

// Exact fraction of the painted sun disc blocked at p (stage D9): each
// occluder sphere subtends a disc of radius asin(r/D) around its center
// direction; blocked fraction = overlap with the sun disc (radius
// uSunAngle), normalized by the sun disc area. Same analytic form as the
// planet shader, so the gas terminator and moon shadows match the surface
// exactly (the old single tap gave a hard, wrong penumbra).
// A trig-free test rejects occluders whose discs cannot reach the sun disc,
// so the common case stays cheap inside the march loop.
float sunShadowAtt(vec3 p, vec3 L)
{
    float a = uSunAngle;
    float ca = cos(a), sa = sin(a);
    float sunArea = 3.14159265 * a * a;
    float att = 1.0;
    for (int i = 0; i < uOccluderCount; i++) {
        vec3 d = uOccluders[i].xyz - p;
        float D = length(d);
        float r = uOccluders[i].w;
        if (D < r)
            return 0.0; // sample inside an occluder
        // Discs overlap iff delta < a + alpha. Rewritten without acos/asin:
        // cos(delta) = b/D and cos(a + alpha) = ca*sqrt(D^2-r^2)/D - sa*r/D
        float b = dot(d, L);
        if (b <= ca * sqrt(max(D * D - r * r, 0.0)) - sa * r)
            continue;
        float alpha = asin(clamp(r / D, 0.0, 1.0));
        float delta = acos(clamp(b / D, -1.0, 1.0));
        float blocked = clamp(discOverlap(a, alpha, delta) / sunArea, 0.0, 1.0);
        att *= 1.0 - blocked;
        if (att < 1e-4)
            break;
    }
    return att;
}

void main()
{
    vec3 rd = normalize(vRayDir);

    // ray vs the shell sphere
    vec3 oc = uCamPos - uCenter;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - uShellR * uShellR;
    float disc = b * b - c;
    if (disc < 0.0)
        discard; // ray misses the gas
    float sq = sqrt(disc);
    float t0 = -b - sq;
    float t1 = -b + sq;
    if (t1 <= 0.0)
        discard; // gas entirely behind the camera
    if (t0 < 0.0)
        t0 = 0.0; // camera inside the gas: march from the eye

    // ~2% of the shell radius per step, clamped to a sane range
    float steps = clamp(floor((t1 - t0) / (uShellR * 0.02) + 0.5), 8.0, 64.0);
    float ds = (t1 - t0) / steps;

    // per-pixel random phase of the sample grid: a fixed grid slides across
    // the density profile as the ray path length changes, and the quadrature
    // error forms visible concentric rings (contours of equal path length).
    // Jittering breaks them into invisible noise. Hash the pixel, not the ray
    // direction: the world-space ray changes every frame while the camera
    // moves, and a ray-dependent jitter makes the atmosphere shimmer
    float jit = hash13(vec3(gl_FragCoord.xy, 0.0) * 71.3);

    float trans = 1.0;    // accumulated transmittance
    vec3 acc = vec3(0.0); // accumulated in-scattered light

    for (float i = 0.0; i < 64.0; i++) {
        if (i >= steps)
            break;
        float t = min(t0 + (i + 0.5 + jit) * ds, t1);
        vec3 p = uCamPos + rd * t;
        float h = length(p - uCenter) - uPlanetR;
        if (h < -1.0)
            break; // solid body: occludes every sample behind it
        if (h <= 0.0)
            continue; // just below the surface: no gas
        float dens = uDensity * exp(-h / uScaleH);
        if (uNoiseAmt > 0.0) {
            // project the sample onto the surface (un-rotate the spin) so the
            // texture luminance can mask the gas: bright cloud tops carry
            // denser atmosphere; FBM adds drifting structure on top
            vec3 d = (p - uCenter) / (h + uPlanetR);
            float cs = cos(uSpin), sn = sin(uSpin);
            vec3 q = vec3(cs * d.x - sn * d.z, d.y, sn * d.x + cs * d.z);
            vec2 tuv = vec2(atan(q.x, q.z) * 0.15915494 + 0.5,
                            acos(clamp(q.y, -1.0, 1.0)) * 0.31830989);
            float luma = dot(texture(uTex, tuv).rgb, vec3(0.299, 0.587, 0.114));
            // noise is fixed in the rotating frame (no uTime drift): the
            // cloud pattern rotates with the surface instead of shimmering
            float n = fbm(q * (uPlanetR * 0.08));
            dens *= mix(0.35, 1.35, luma) * mix(1.0, 0.35 + 1.3 * n, uNoiseAmt);
        }
        float sigma = dens * ds;
        if (sigma < 1e-6)
            continue;
        // per-sample sunlight: smooth terminator, a whisper on the night side.
        // A solid body (the planet itself included) blocks the sun: night-side
        // gas near the surface sits in the planet's own umbra, only high gas
        // can see the sun over the limb
        vec3 N = (p - uCenter) / (h + uPlanetR);
        float day = smoothstep(-0.25, 0.35, dot(N, uSunDir));
        float sh = sunShadowAtt(p, uSunDir);
        sh *= 1.0 - ringShadow(p, uSunDir); // ring shadow band through the gas
        float a = 1.0 - exp(-sigma);
        acc += trans * a * uAtmColor * uSunColor * (0.02 + 0.98 * day * sh);
        trans *= (1.0 - a);
        if (trans < 0.02)
            break; // optically thick: nothing left to add
    }

    fragColor = vec4(acc, 1.0 - trans);
}
)glsl";

const char *kRingFragment = R"glsl(
#version 330 core
// Flat annulus (stage D2b): the texture is a radial strip (width = ring
// radius), so only u matters. Two-sided lighting (|dot|) keeps the disc
// visible from both faces; the planet casts a hard shadow band across it.
// Drawn with the scene's default alpha blend, cull off, depth write off.
// (GLSL: runtime-initialized locals must not be `const` — error C1059)
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTex;

out vec4 fragColor;

uniform sampler2D uRingTex;
uniform vec3 uSunDir;     // normalized, points toward the sun
uniform vec3 uSunColor;
uniform vec3 uCenter;     // shadow-casting planet center
uniform float uPlanetR;   // shadow-casting planet radius

// Does the (single) planet sphere block the ray p -> p + t*dir (t > 0)?
float planetShadow(vec3 p, vec3 dir)
{
    vec3 d = uCenter - p;
    float b = dot(d, dir);
    if (b <= 0.0)
        return 0.0; // planet behind the point
    float r2 = dot(d, d) - uPlanetR * uPlanetR;
    if (r2 >= b * b)
        return 0.0; // ray misses the planet
    return (b - sqrt(b * b - r2)) > 0.0 ? 1.0 : 0.0;
}

void main()
{
    vec4 rt = texture(uRingTex, vec2(vTex.x, 0.5));
    if (rt.a < 0.004)
        discard; // transparent margins / ring gaps

    vec3 L = normalize(uSunDir);
    float light = abs(dot(normalize(vNormal), L));
    float sh = 1.0 - planetShadow(vWorldPos, L); // planet's shadow band

    // the texture is a dim gray-tan (~0.4); brighten to a sunlit ring
    vec3 col = rt.rgb * 2.2 * uSunColor * (light * sh);

    fragColor = vec4(col, rt.a);
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

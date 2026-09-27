#include "meshes.h"

#include <cmath>

#define _USE_MATH_DEFINES

bool Mesh::build(QOpenGLFunctions_3_3_Core *gl,
                 const QVector<float> &positions, const QVector<float> &normals,
                 const QVector<float> &uvs, const QVector<unsigned int> &indices)
{
    if (positions.size() % 3 != 0 || normals.size() != positions.size()
        || indices.isEmpty())
        return false;

    m_gl = gl;
    release(gl);

    const size_t n = positions.size() / 3;
    if (uvs.size() != n * 2)
        return false;

    QVector<float> verts(n * 8);
    for (size_t i = 0; i < n; ++i) {
        float *v = &verts[(std::size_t)i * 8];
        v[0] = positions[3 * i + 0];
        v[1] = positions[3 * i + 1];
        v[2] = positions[3 * i + 2];
        v[3] = normals[3 * i + 0];
        v[4] = normals[3 * i + 1];
        v[5] = normals[3 * i + 2];
        v[6] = uvs[2 * i + 0];
        v[7] = uvs[2 * i + 1];
    }

    gl->glGenVertexArrays(1, &vao);
    gl->glGenBuffers(1, &vbo);
    gl->glGenBuffers(1, &ibo);

    gl->glBindVertexArray(vao);

    gl->glBindBuffer(GL_ARRAY_BUFFER, vbo);
    gl->glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    gl->glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                     indices.data(), GL_STATIC_DRAW);

    const GLsizei stride = 8 * sizeof(float);
    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const GLvoid *)(0));
    gl->glEnableVertexAttribArray(1);
    gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (const GLvoid *)(3 * sizeof(float)));
    gl->glEnableVertexAttribArray(2);
    gl->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (const GLvoid *)(6 * sizeof(float)));

    gl->glBindVertexArray(0);
    indexCount = (GLsizei)indices.size();
    return true;
}

void Mesh::draw() const
{
    if (!vao || !m_gl)
        return;
    m_gl->glBindVertexArray(vao);
    m_gl->glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    m_gl->glBindVertexArray(0);
}

void Mesh::release(QOpenGLFunctions_3_3_Core *gl)
{
    if (!gl)
        return;
    if (vao) {
        gl->glDeleteVertexArrays(1, &vao);
        vao = 0;
    }
    if (vbo) {
        gl->glDeleteBuffers(1, &vbo);
        vbo = 0;
    }
    if (ibo) {
        gl->glDeleteBuffers(1, &ibo);
        ibo = 0;
    }
    indexCount = 0;
}

namespace meshes
{
void sphereData(int lat, int lon,
                QVector<float> &pos, QVector<float> &nrm,
                QVector<float> &uv, QVector<unsigned int> &idx)
{
    if (lat < 2 || lon < 3)
        return;

    pos.clear();
    nrm.clear();
    uv.clear();
    idx.clear();

    for (int y = 0; y <= lat; ++y) {
        const float v = 1.0f * y / lat;          // 0 = north pole
        const float theta = v * (float)M_PI;
        for (int x = 0; x <= lon; ++x) {
            const float u = 1.0f * x / lon;
            const float phi = u * 2.0f * (float)M_PI;

            const float nx = sinf(theta) * sinf(phi);
            const float ny = cosf(theta);
            const float nz = sinf(theta) * cosf(phi);

            pos.append(nx);
            pos.append(ny);
            pos.append(nz);
            nrm.append(nx);
            nrm.append(ny);
            nrm.append(nz);
            // v=0 samples the top image row -> equirect maps stay upright
            uv.append(u);
            uv.append(v);
        }
    }

    for (int y = 0; y < lat; ++y) {
        for (int x = 0; x < lon; ++x) {
            const unsigned int a = (unsigned int)(y * (lon + 1) + x);
            const unsigned int b = a + 1;
            const unsigned int c = a + (lon + 1);
            const unsigned int d = c + 1;
            idx.append(a);
            idx.append(c);
            idx.append(b);
            idx.append(b);
            idx.append(c);
            idx.append(d);
        }
    }
}

void ringData(float rIn, float rOut, int segs,
              QVector<float> &pos, QVector<float> &nrm,
              QVector<float> &uv, QVector<unsigned int> &idx)
{
    if (segs < 3 || rOut <= rIn)
        return;

    pos.clear();
    nrm.clear();
    uv.clear();
    idx.clear();

    for (int i = 0; i <= segs; ++i) {
        const float a = 2.0f * (float)M_PI * (float)i / (float)segs;
        const float ca = cosf(a);
        const float sa = sinf(a);
        const float v = (float)i / (float)segs;

        pos.append(rIn * ca);
        pos.append(0.0f);
        pos.append(rIn * sa);
        nrm.append(0.0f);
        nrm.append(1.0f);
        nrm.append(0.0f);
        uv.append(0.0f);
        uv.append(v);

        pos.append(rOut * ca);
        pos.append(0.0f);
        pos.append(rOut * sa);
        nrm.append(0.0f);
        nrm.append(1.0f);
        nrm.append(0.0f);
        uv.append(1.0f);
        uv.append(v);
    }

    for (int i = 0; i < segs; ++i) {
        const unsigned int a = (unsigned int)(2 * i);
        const unsigned int b = a + 1;
        const unsigned int c = a + 2;
        const unsigned int d = a + 3;
        idx.append(a);
        idx.append(b);
        idx.append(c);
        idx.append(b);
        idx.append(d);
        idx.append(c);
    }
}
}

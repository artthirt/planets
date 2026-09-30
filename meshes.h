#pragma once

#include <QOpenGLFunctions_3_3_Core>
#include <QVector>

#include <cstddef>

// GPU-resident indexed triangle mesh.
// Layout (interleaved, 8 floats): aPos(3) @0, aNormal(3) @1, aTex(2) @2.
struct Mesh
{
    Mesh() = default;
    ~Mesh()
    {
        if (m_gl)
            release(m_gl);
    }

    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;

    bool build(QOpenGLFunctions_3_3_Core *gl,
               const QVector<float> &positions, const QVector<float> &normals,
               const QVector<float> &uvs, const QVector<unsigned int> &indices);

    void draw() const;
    void release(QOpenGLFunctions_3_3_Core *gl);

    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ibo = 0;
    GLsizei indexCount = 0;

private:
    QOpenGLFunctions_3_3_Core *m_gl = nullptr;
};

namespace meshes
{
// Unit UV sphere (lat = rows, lon = columns), indexed, outward winding.
void sphereData(int lat, int lon,
                QVector<float> &pos, QVector<float> &nrm,
                QVector<float> &uv, QVector<unsigned int> &idx);

// Flat annulus in the XZ plane (triangle strip), radii in planet-radius
// units. aTex.x = radial coordinate (0 = inner edge, 1 = outer edge),
// aTex.y = angular fraction (unused by the radial texture).
void ringData(float rIn, float rOut, int segs,
              QVector<float> &pos, QVector<float> &nrm,
              QVector<float> &uv, QVector<unsigned int> &idx);
}

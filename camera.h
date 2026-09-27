#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>

// Free-fly camera: position + yaw/pitch. Right-handed, -Z forward in camera space.
struct Camera
{
    QVector3D position{0.0f, 0.0f, 0.0f};
    float yaw = 0.0f;        // rotation around world Y
    float pitch = 0.0f;      // rotation around the right axis
    float fovDeg = 55.0f;
    float nearDist = 0.5f;
    float farDist = 10000.0f;

    static constexpr float kMaxPitch = 1.55f; // ~88.8 deg, keeps up vector sane

    QVector3D forward() const
    {
        const float cp = cosf(pitch);
        return QVector3D(cp * sinf(yaw), sinf(pitch), cp * cosf(yaw));
    }

    QVector3D right() const
    {
        return QVector3D(cosf(yaw), 0.0f, -sinf(yaw));
    }

    QVector3D up() const
    {
        // this Qt build's QVector3D has no cross product -> compute inline
        const QVector3D r = right();
        const QVector3D f = forward();
        return QVector3D(f.y() * r.z() - f.z() * r.y(),
                         f.z() * r.x() - f.x() * r.z(),
                         f.x() * r.y() - f.y() * r.x());
    }

    void clampPitch()
    {
        pitch = std::clamp(pitch, -kMaxPitch, kMaxPitch);
    }

    // Point yaw/pitch at a world-space target.
    void lookAt(const QVector3D &target)
    {
        QVector3D d = target - position;
        const float len = d.length();
        if (len < 1e-6f)
            return;
        d /= len;
        yaw = atan2f(d.x(), d.z());
        pitch = asinf(std::clamp(d.y(), -1.0f, 1.0f));
    }

    QMatrix4x4 viewMatrix() const
    {
        QMatrix4x4 m;
        m.lookAt(position, position + forward(), QVector3D(0, 1, 0));
        return m;
    }

    QMatrix4x4 projectionMatrix(float aspect) const
    {
        QMatrix4x4 m;
        m.perspective(fovDeg, aspect, nearDist, farDist);
        return m;
    }
};

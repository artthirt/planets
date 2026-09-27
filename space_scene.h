#pragma once

#include <QElapsedTimer>
#include <QMap>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLWindow>
#include <QPoint>
#include <QTimer>
#include <QVector3D>

#include <memory>
#include <vector>

#include "bodies.h"
#include "camera.h"
#include "meshes.h"
#include "shaders.h"

// The space scene: a free-fly camera over a small planetary system.
// The sun is a distant object painted on the sky dome (directional light).
class SpaceScene : public QOpenGLWindow, private QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    explicit SpaceScene();
    ~SpaceScene() override;

    void setSpeed(float timeScale);

    // hidden CLI helpers (screenshot mode)
    void setCameraForShot(const QVector3D &pos, float yawDeg, float pitchDeg);
    void requestScreenshot(const QString &path, int frames);

signals:
    void focusChanged(const QString &bodyName);
    void infoChanged(const QString &text);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void keyReleaseEvent(QKeyEvent *e) override;

private:
    void buildScene();
    unsigned int uploadTexture(const QString &resPath);

    void updateOrbits();
    void updateCamera(float dt);
    void cycleFocus();

    void drawSky(const QMatrix4x4 &proj, const QMatrix4x4 &view);
    void drawBody(size_t i, const QMatrix4x4 &proj, const QMatrix4x4 &view);
    void drawAtmosphere(size_t i, const QMatrix4x4 &proj, const QMatrix4x4 &view);
    void saveScreenshot();

    // GL resources
    std::unique_ptr<ShaderProgram> m_planetProg;
    std::unique_ptr<ShaderProgram> m_skyProg;
    std::unique_ptr<ShaderProgram> m_atmProg;    // gas shell (camera outside)
    std::unique_ptr<ShaderProgram> m_atmFsProg;  // fullscreen (camera in gas)
    std::unique_ptr<Mesh> m_sphere;
    std::unique_ptr<Mesh> m_fsTri;               // fullscreen triangle (NDC)
    std::vector<GLuint> m_textures;
    unsigned int m_skyTex = 0;
    bool m_hasAniso = false;
    float m_maxAniso = 1.0f;

    // shader uniform locations (valid after bind())
    struct PlanetUniforms { int model, view, proj, camPos, sunDir, sunColor, tex, spec, shin, occluders, occluderCount, sunAngle; };
    struct SkyUniforms { int model, view, proj, camPos, sunDir, sunColor, skyTex; };
    struct AtmUniforms { int model, view, proj, camPos, center, planetR, shellR, sunDir, sunColor, atmColor, density, scaleH, tex, spin, time, noiseAmt, occluders, occluderCount; };
    struct AtmFsUniforms { int invViewProj, camPos, center, planetR, shellR, sunDir, sunColor, atmColor, density, scaleH, tex, spin, time, noiseAmt, occluders, occluderCount; };
    PlanetUniforms m_pu{};
    SkyUniforms m_su{};
    AtmUniforms m_au{};
    AtmFsUniforms m_afu{};

    // scene data
    std::vector<Body> m_bodies;
    QVector3D m_sunDir;
    QVector3D m_sunColor{1.0f, 0.94f, 0.84f};
    static constexpr float kSkyRadius = 2500.0f;

    // camera / input
    Camera m_cam;
    int m_focus = -1;            // -1 = free fly, >= 0 = body index
    float m_focusDist = 4.0f;    // body radii
    QPoint m_lastMouse;
    bool m_dragging = false;
    QMap<int, bool> m_keys;

    // animation
    QTimer m_timer;
    float m_time = 0.0f;         // scene time (degrees-ish)
    float m_speed = 0.01f;       // slider scale
    QElapsedTimer m_lastFrame;

    // screenshot mode
    QString m_shotPath;
    int m_shotFramesLeft = 0;
    int m_shotCount = 0;
};

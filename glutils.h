#ifndef GLUTILS_H
#define GLUTILS_H

#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QOpenGLShaderProgram>

class glutils
{
public:
	glutils();
};

class GLBuffer{
public:
	explicit GLBuffer();
	GLBuffer(const GLBuffer& buffer);
	GLBuffer(QOpenGLFunctions_3_3_Core* self);
	void setSelf(QOpenGLFunctions_3_3_Core *self);
	void initBuffer(QOpenGLFunctions_3_3_Core *self, bool tex = false, bool specularText = false,
					bool lighting = true, int instancedCount = 1);
	void addPtTex(float x1, float x2);
	void addPt(float x1, float x2, float x3);
	void addPtN(float x1, float x2, float x3);
	void addCountPoint(int cnt);
	void changeLastCountPoint(int cnt);
	void addTextTexture(const QString& text, int w = 155, int h = 45);
	void initPts(float* data, int len);
	void initPtsTex(float* data, int len);
	void setPt(uint index, float x1, float x2, float x3);

	void drawBuffer(float *proj, float *model);
	void drawBuffers(float *proj, float *model, float *view);

	void setColor(float x1, float x2, float x3, float x4);
	void setWidth(float w);
	void setType(uint type);
	void clearBuffer();
	void clearTexBuffer();
	bool empty();
	void setLightPos(float x1, float x2, float x3);
	void setEyePos(float x1, float x2, float x3);
	void setBlendStrength(float val);
	void setScale(float val);
	void setSpecularStrength(float val);
	void setDiffuseStrength(float val);
	void setViewMatrix(const QMatrix4x4& view);

	void setBlendStrength(int index, float val);
	void setScale(int index, float val);

    void initCylinder(int flates, float h, float rx, float ry,
                      float x = 0, float y = 0, float z = 0);
	void initSphere(int cnt1, int cnt2, float R);
	uint initTexture(const QImage& image);
	uint initTexture(uint tex);
	void initTexture2(const QImage& image);

	size_t countTextures() const;
	size_t countCountPoint() const;
private:
    QOpenGLShaderProgram m_shpr;
    QOpenGLShaderProgram m_shpr2;
	int m_instancedCount;
	int m_vecInt;
	int m_texInt;
	int m_colInt;
	int m_nrmInt;
	int m_lhtInt;
	int m_umodel;
	int m_uview;
	int m_uproj;
	int m_eyeInt;
	int m_useSpecInt;
	int m_baseTexInt;
	int m_specTexInt;
	int m_blendInt;
	int m_specStrInt;
	int m_diffStrInt;
	int m_scaleInt;
	float m_color[4];
	float m_light[3];
	float m_eye[3];
	float m_view[16];
	std::vector<float> m_blendStrength;
	std::vector<float> m_scales;
	float m_specStrength;
	float m_diffStrength;
	float m_width;
	uint m_type;
	std::vector< float > m_buffer;
	std::vector< float > m_nbuffer;
	std::vector< float > m_texbuffer;
	std::vector< int > m_polycnt;
	std::vector< uint > m_polyTex;
	std::vector< uint > m_polyTex2;
	QOpenGLFunctions_3_3_Core *m_self;
	bool m_init;
	bool m_is_tex;
	bool m_is_use_tex2;
};

float A2R(float val);
float R2A(float val);

#endif // GLUTILS_H

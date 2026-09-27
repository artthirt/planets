#include "glutils.h"

#include <QImage>
#include <QPainter>

#define _PI       3.14159265358979323846

#include <math.h>

glutils::glutils()
{

}

////////////////////////////////////////////

const QString vlp =
		"	mat3 normalMatrix = mat3((uModel));\n"
		"	normal = normalize(normalMatrix * vNormal);\n"
		"	vec4 pos = uModel * vPosition;\n"
		"	_lp = lp;\n"
		"	_eye = -vec3(pos);\n";

const QString vlight =
		"   vec3 ld = normalize(_lp);\n"
		"   float diff = max(dot(normal, ld), 0);\n"
		"	float spec = 0;\n"
		"	vec3 e = normalize(_eye);\n;"
		"   vec3 h = normalize(ld + e);\n"
		"	// compute the specular term into spec\n"
		"	float intSpec = max(dot(h, normal), 0.0);\n"
		"   spec = pow(intSpec, 16);\n";

const QString vshaderInstanced =
		"#version 330 core\n"
		"attribute vec4 vPosition;\n"
		"attribute vec3 vNormal;\n"
		"attribute vec2 aTexCoord;\n"
		"uniform mat4 uView;\n"
		"uniform mat4 uModel;\n"
		"uniform mat4 uProj;\n"
		"uniform vec3 eye;\n"
		"uniform vec3 lp;\n"
		"uniform float scale[%1];\n"
		"uniform float blend[%1];\n"
		"varying vec3 _lp;\n"
		"varying vec2 vTexCoord;\n"
		"varying float _blend;\n"
		"varying vec3 normal;\n"
		"varying vec3 _eye;\n"
		"void main(){\n"
		+ vlp +
		"   mat3 md = mat3(uModel);\n"
		"	mat4 um = uModel;\n"
		"	_blend = blend[gl_InstanceID];\n"
		"	float sc = scale[gl_InstanceID];\n"
		"	vec4 vp = vec4((1 + sc) * vec3(vPosition), vPosition.w);\n"
		"   vTexCoord = aTexCoord;\n"
		"   gl_Position = uProj * uView * uModel * vp;\n"
		"}";

const QString vshader =
		"#version 330 core\n"
		"attribute vec4 vPosition;\n"
		"attribute vec3 vNormal;\n"
		"attribute vec2 aTexCoord;\n"
		"uniform mat4 uView;\n"
		"uniform mat4 uModel;\n"
		"uniform mat4 uProj;\n"
		"uniform vec3 lp;\n"
		"uniform vec3 eye;\n"
		"uniform float blend;\n"
		"varying vec3 _lp;\n"
		"varying vec2 vTexCoord;\n"
		"varying float _blend;\n"
		"varying vec3 normal;\n"
		"varying vec3 _eye;\n"
		"void main(){\n"
		+ vlp +
		"   mat3 md = mat3(uModel);\n"
		"	mat4 um = uModel;\n"
		"	_blend = blend;\n"
		"   vTexCoord = aTexCoord;\n"
		"   gl_Position = uProj * uView * uModel * vPosition;\n"
		"}";

const QString fTexShaderSpecular =
		"#version 330 core\n"
		#ifdef ARM
			"precision mediump float;"
		#endif
			"uniform vec4 uColor;\n"
			"uniform sampler2D uTexture;\n"
			"uniform sampler2D uSpec;\n"
			"uniform int useSpec;\n"
			"uniform float specStrength;"
			"uniform float diffStrength;"
			"varying vec3 _lp;\n"
			"varying float _blend;\n"
			"varying vec2 vTexCoord;\n"
			"varying vec3 normal;\n"
			"varying vec3 _eye;\n"
			"void main(){\n"
			"	vec4 spect = texture2D(uSpec, vTexCoord);\n"
			"	vec4 tex = texture2D(uTexture, vTexCoord);\n"
			+ vlight +
			"	vec4 col =  uColor * ((diff * (1. - spect) + 0.04) * tex);\n"
			"	col += uColor * (spect * (0.01 + (0.01 + specStrength * spec + diffStrength * diff)) * tex);\n"
			"	gl_FragColor = col;\n"
			"}";

const QString fTexShader =
		"#version 330 core\n"
		#ifdef ARM
			"precision mediump float;"
		#endif
			"uniform vec4 uColor;\n"
			"uniform sampler2D uTexture;\n"
			"uniform sampler2D uSpec;\n"
			"uniform int useSpec;\n"
			"uniform float specStrength;"
			"uniform float diffStrength;"
			"varying vec3 _lp;\n"
			"varying float _blend;\n"
			"varying vec2 vTexCoord;\n"
			"varying vec3 normal;\n"
			"varying vec3 _eye;\n"
			"void main(){\n"
			"	vec4 tex = texture2D(uTexture, vTexCoord);\n"
			+ vlight +
			"	vec4 col = uColor * (0.01 + (0.01 + specStrength * spec + diffStrength * diff) * tex);\n"
			"	float at = (tex.r + tex.g + tex.b)/3;\n"
			//"	at = at * at * at;\n"
			"	col.a = _blend * at;\n"
			"	gl_FragColor = col;\n"
			"}";

const QString fTexNoLightShader =
		"#version 330 core\n"
		#ifdef ARM
			"precision mediump float;"
		#endif
			"uniform vec4 uColor;\n"
			"uniform sampler2D uTexture;\n"
			"uniform float blend;\n"
			"varying vec2 vTexCoord;\n"
			"void main(){\n"
			"	vec4 tex = texture2D(uTexture, vTexCoord);\n"
			"	vec4 col = uColor * tex;\n"
			"	col.a = blend * (tex.r + tex.g + tex.b)/3;\n"
			"	gl_FragColor = col;\n"
			"}";

const QString fShader =
		#ifdef ARM
			"precision mediump float;"
		#endif
			"uniform vec4 uColor;\n"
			"varying vec2 vTexCoord;\n"
			"void main(){\n"
			"    gl_FragColor = uColor;\n"
			"}";

////////////////////////////////////////////

GLBuffer::GLBuffer()
{
	m_instancedCount = 1;
	m_self = 0;
	m_type = GL_LINES;
	m_width = 1;
	m_color[0] = m_color[1] = m_color[2] = m_color[3] = 1;
	m_light[0] = m_light[1] = m_light[2] = 1;
	m_eye[0] = m_eye[1] = m_eye[2] = 0;

	m_blendStrength.resize(1);
	m_blendStrength[0] = 1;
	m_scales.resize(1);
	m_scales[0] = 1;

	m_specStrength = 1;
	m_diffStrength = 1;
	m_init = false;
	m_is_tex = false;
	m_is_use_tex2 = false;
}

GLBuffer::GLBuffer(const GLBuffer &buffer)
{

}

GLBuffer::GLBuffer(QOpenGLFunctions_3_3_Core *self)
{
	m_self = self;
	m_type = GL_LINES;
	m_width = 1;
	m_color[0] = m_color[1] = m_color[2] = m_color[3] = 1;
	m_init = false;
}

void GLBuffer::setSelf(QOpenGLFunctions_3_3_Core *self)
{
	m_self = self;
}

void GLBuffer::addPtTex(float x1, float x2)
{
	m_texbuffer.push_back(x1);
	m_texbuffer.push_back(x2);
}

void GLBuffer::addPt(float x1, float x2, float x3)
{
	m_buffer.push_back(x1);
	m_buffer.push_back(x2);
	m_buffer.push_back(x3);
}

void GLBuffer::addPtN(float x1, float x2, float x3)
{
	m_nbuffer.push_back(x1);
	m_nbuffer.push_back(x2);
	m_nbuffer.push_back(x3);
}

void GLBuffer::addCountPoint(int cnt)
{
	m_polycnt.push_back(cnt);
}

void GLBuffer::changeLastCountPoint(int cnt)
{
	if(!m_polycnt.empty()){
		m_polycnt.back() = cnt;
	}
}

void GLBuffer::addTextTexture(const QString &text, int w, int h)
{
	uint tex;
	m_self->glGenTextures(1, &tex);

	QImage im(w, h, QImage::Format_RGB32);
	QPainter painter(&im);
	QFont f = painter.font();
	f.setPointSize(12);
	f.setFamily("Source Code Pro");
	painter.setFont(f);
	painter.fillRect(im.rect(), QColor(0, 0, 0, 160));
	painter.setPen(Qt::white);
	painter.setBrush(Qt::white);
	painter.drawText(QRect(0, 3, w, h), text);

	m_self->glBindTexture(GL_TEXTURE_2D, tex);
	m_self->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_self->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    m_self->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, im.bits());
	uint err = m_self->glGetError();

	m_polyTex.push_back(tex);
}

void GLBuffer::initPts(float *data, int len)
{
	if(!data || !len)
		return;
	m_buffer.clear();
	for(int i = 0; i < len; ++i){
		m_buffer.push_back(data[i]);
	}
}

void GLBuffer::initPtsTex(float *data, int len)
{
	if(!data || !len)
		return;
	m_texbuffer.clear();
	for(int i = 0; i < len; ++i){
		m_texbuffer.push_back(data[i]);
	}
}

void GLBuffer::setPt(uint index, float x1, float x2, float x3)
{
	if(m_buffer.empty() || index * 3 >= m_buffer.size())
		return;

	int id = index * 3;

	m_buffer[id + 0] = x1;
	m_buffer[id + 1] = x2;
	m_buffer[id + 2] = x3;
}

void GLBuffer::initBuffer(QOpenGLFunctions_3_3_Core *self, bool tex, bool specularText, bool lighting, int instancedCount)
{
	m_self = self;
	m_is_tex = tex;

	m_instancedCount = instancedCount;
	m_blendStrength.resize(instancedCount);
	m_scales.resize(instancedCount);

	QString vvshader;

	if(instancedCount > 1)
		vvshader = vshaderInstanced.arg(m_instancedCount);
	else
		vvshader = vshader;

    m_shpr.addShaderFromSourceCode(QOpenGLShader::Vertex, vvshader);
	if(tex){
		if(lighting){
			if(specularText)
                m_shpr.addShaderFromSourceCode(QOpenGLShader::Fragment, fTexShaderSpecular);
			else
                m_shpr.addShaderFromSourceCode(QOpenGLShader::Fragment, fTexShader);
		}else{
            m_shpr.addShaderFromSourceCode(QOpenGLShader::Fragment, fTexNoLightShader);
		}
	}else{
        m_shpr.addShaderFromSourceCode(QOpenGLShader::Fragment, fShader);

	}
	m_shpr.link();
	m_shpr.bind();

	m_vecInt = m_shpr.attributeLocation("vPosition");
	m_nrmInt = m_shpr.attributeLocation("vNormal");
	m_texInt = m_shpr.attributeLocation("aTexCoord");
	m_colInt = m_shpr.uniformLocation("uColor");
	m_umodel = m_shpr.uniformLocation("uModel");
	m_uview = m_shpr.uniformLocation("uView");
	m_uproj = m_shpr.uniformLocation("uProj");
	m_lhtInt = m_shpr.uniformLocation("lp");
	m_eyeInt = m_shpr.uniformLocation("eye");
	m_baseTexInt = m_shpr.uniformLocation("uTexture");
	m_specTexInt = m_shpr.uniformLocation("uSpec");
	m_useSpecInt = m_shpr.uniformLocation("useSpec");
	m_blendInt = m_shpr.uniformLocation("blend");
	m_specStrInt = m_shpr.uniformLocation("specStrength");
	m_diffStrInt = m_shpr.uniformLocation("diffStrength");
	m_scaleInt = m_shpr.uniformLocation("scale");

	m_init = true;
}

void GLBuffer::drawBuffer(float* proj, float *model)
{
	if(!m_self || !m_init || m_buffer.empty())
		return;

	m_shpr.bind();

	m_self->glUniformMatrix4fv(m_uproj, 1, false, proj);
	m_self->glUniformMatrix4fv(m_umodel, 1, false, model);

	m_self->glUniform4f(m_colInt, m_color[0], m_color[1], m_color[2], m_color[3]);
	m_self->glLineWidth(m_width);

	if(m_is_tex){
		m_self->glEnableVertexAttribArray(m_texInt);
		m_self->glEnableVertexAttribArray(m_vecInt);
		m_self->glVertexAttribPointer(m_texInt, 2, GL_FLOAT, false, 2 * sizeof(float), &m_texbuffer[0]);
		m_self->glVertexAttribPointer(m_vecInt, 3, GL_FLOAT, false, 3 * sizeof(float), &m_buffer[0]);
		m_self->glDrawArrays(m_type, 0, m_buffer.size() / 3);
		m_self->glDisableVertexAttribArray(m_vecInt);
		m_self->glDisableVertexAttribArray(m_texInt);
	}else{
		m_self->glEnableVertexAttribArray(m_vecInt);
		m_self->glVertexAttribPointer(m_vecInt, 3, GL_FLOAT, false, 3 * sizeof(float), &m_buffer[0]);
		m_self->glDrawArrays(m_type, 0, m_buffer.size() / 3);
		m_self->glDisableVertexAttribArray(m_vecInt);
	}

	m_shpr.release();
}

void GLBuffer::drawBuffers(float *proj, float *model, float *view)
{
	if(!m_self || !m_init || m_buffer.empty())
		return;

	if(m_polycnt.empty()){
		drawBuffer(proj, model);
		return;
	}

	m_shpr.bind();

	m_self->glUniform3fv(m_lhtInt, 1, m_light);
	m_self->glUniform3fv(m_eyeInt, 1, m_eye);
	m_self->glUniform1i(m_useSpecInt, m_is_use_tex2);
	m_self->glUniform1fv(m_blendInt, m_blendStrength.size(), m_blendStrength.data());
	m_self->glUniform1fv(m_scaleInt, m_scales.size(), m_scales.data());
	m_self->glUniform1f(m_specStrInt, m_specStrength);
	m_self->glUniform1f(m_diffStrInt, m_diffStrength);

	m_self->glUniformMatrix4fv(m_uproj, 1, false, proj);
	m_self->glUniformMatrix4fv(m_umodel, 1, false, model);
	m_self->glUniformMatrix4fv(m_uview, 1, false, view);

	m_self->glUniform4f(m_colInt, m_color[0], m_color[1], m_color[2], m_color[3]);
	m_self->glLineWidth(m_width);

	m_self->glEnableVertexAttribArray(m_vecInt);
	m_self->glVertexAttribPointer(m_vecInt, 3, GL_FLOAT, false, 3 * sizeof(float), &m_buffer[0]);

	if(!m_nbuffer.empty()){
		m_self->glEnableVertexAttribArray(m_nrmInt);
		m_self->glVertexAttribPointer(m_nrmInt, 3, GL_FLOAT, false, 3 * sizeof(float), &m_nbuffer[0]);
	}

	if(m_is_tex){
		m_self->glEnableVertexAttribArray(m_texInt);
		m_self->glVertexAttribPointer(m_texInt, 2, GL_FLOAT, false, 2 * sizeof(float), &m_texbuffer[0]);

		m_self->glEnable(GL_TEXTURE_2D);

		m_self->glUniform1i(m_baseTexInt, 0);
		m_self->glUniform1i(m_specTexInt, 1);
	}

	int idx = 0;
	int idtex = 0;

	if(m_is_tex && m_polyTex.size() == m_polycnt.size()){
		for(int cnt: m_polycnt){
			m_self->glActiveTexture(GL_TEXTURE0);
			m_self->glBindTexture(GL_TEXTURE_2D, m_polyTex[idtex]);

			if(!m_polyTex2.empty()){
				m_self->glActiveTexture(GL_TEXTURE1);
				m_self->glBindTexture(GL_TEXTURE_2D, m_polyTex2[idtex]);
			}

			m_self->glDrawArraysInstanced(m_type, idx, cnt, m_instancedCount);
			idx += cnt;
			idtex++;
		}

	}else{
		for(int cnt: m_polycnt){
			m_self->glDrawArraysInstanced(m_type, idx, cnt, m_instancedCount);
			idx += cnt;
		}
	}

	m_self->glDisableVertexAttribArray(m_vecInt);

	if(!m_nbuffer.empty()){
		m_self->glDisableVertexAttribArray(m_nrmInt);
	}

	if(m_is_tex){
		m_self->glDisableVertexAttribArray(m_texInt);
		m_self->glDisable(GL_TEXTURE_2D);
	}

	m_shpr.release();
}

void GLBuffer::setColor(float x1, float x2, float x3, float x4)
{
	m_color[0] = x2;
	m_color[1] = x2;
	m_color[2] = x3;
	m_color[3] = x4;
}

void GLBuffer::setWidth(float w)
{
	m_width = w;
}

void GLBuffer::setType(uint type)
{
	m_type = type;
}

void GLBuffer::clearBuffer()
{
	m_buffer.clear();
	m_texbuffer.clear();
	m_polycnt.clear();
}

void GLBuffer::clearTexBuffer()
{
	for(uint tex: m_polyTex){
		m_self->glDeleteTextures(1, &tex);
	}
	m_polyTex.clear();
}

bool GLBuffer::empty()
{
	return m_buffer.empty();
}

void GLBuffer::setLightPos(float x1, float x2, float x3)
{
	m_light[0] = x1;
	m_light[1] = x2;
	m_light[2] = x3;
}

void GLBuffer::setEyePos(float x1, float x2, float x3)
{
	m_eye[0] = x1;
	m_eye[1] = x2;
	m_eye[2] = x3;
}

void GLBuffer::setBlendStrength(float val)
{
	m_blendStrength[0] = val;
}

void GLBuffer::setScale(float val)
{
	m_scales[0] = val;
}

void GLBuffer::setSpecularStrength(float val)
{
	m_specStrength = val;
}

void GLBuffer::setDiffuseStrength(float val)
{
	m_diffStrength = val;
}

void GLBuffer::setViewMatrix(const QMatrix4x4 &view)
{
	for(int i = 0; i < 16; ++i){
		m_view[i] = view.data()[i];
	}
}

void GLBuffer::setBlendStrength(int index, float val)
{
	if(index < m_blendStrength.size())
		m_blendStrength[index] = val;
}

void GLBuffer::setScale(int index, float val)
{
	if(index < m_scales.size())
		m_scales[index] = val;
}

void GLBuffer::initCylinder(int flates, float h, float rx, float ry, float x, float y, float z)
{
    if(flates <= 1 || h <= 0 || rx < 0 || ry < 0)
        return;
    clearBuffer();

    float NCOUNT = flates - 1;

    for(int i = 0; i < flates; ++i){
        float t = (float)i / NCOUNT;
		float z = -h/2.f;
        addPt(x + rx * sin((float)i / NCOUNT * 2 * _PI), z - h/2.,
              y + ry * cos((float)i / NCOUNT * 2 * _PI));
        addPtTex(t, 1);
        addPt(x + rx * sin((float)i / NCOUNT * 2 * _PI), z + h/2.,
              y + ry * cos((float)i / NCOUNT * 2 * _PI));
        addPtTex(t, 0);
    }
	addCountPoint(flates * 2);
}

void GLBuffer::initSphere(int cnt1, int cnt2, float R)
{
	auto func = [&](float x, float y){
		float dx = 1.f * x / (cnt2 - 1);
		float dy = 1.f * y / cnt1;
		float fx = R * sinf(dx * 2.f * _PI) * sinf(dy * _PI);
		float fy = R * cosf(dy * _PI);
		float fz = R * cosf(dx * 2.f * _PI) * sinf(dy * _PI);
		addPt(fx, fy, fz);

		float fu = dx;
		float fv = dy;
		addPtTex(fu, fv);

		float nx = sinf(dx * 2.f * _PI) * sinf(dy * _PI);
		float ny = cosf(dy * _PI);
		float nz = cosf(dx * 2.f * _PI) * sinf(dy * _PI);
		addPtN(nx, ny, nz);

	};

	int id = 0;
	for(int y = 0; y < cnt1; ++y){
		for(int x = 0; x < cnt2; ++x){
			func(x, y); ++id;
			func(x, y + 1); ++id;
		}
	}
	addCountPoint(id);
}

uint GLBuffer::initTexture(const QImage &image)
{
	if(image.isNull())
		return 0;
	uint tex;
    int w = image.width();
    int h = image.height();
    m_self->glGenTextures(1, &tex);
    m_self->glBindTexture(GL_TEXTURE_2D, tex);
    m_self->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    m_self->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    m_self->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, image.bits());
	m_polyTex.push_back(tex);

	return tex;
}

uint GLBuffer::initTexture(uint tex)
{
	if(!tex)
		return 0;
	m_polyTex.push_back(tex);
	return tex;
}

void GLBuffer::initTexture2(const QImage &image)
{
	if(image.isNull())
		return;
	uint tex;
	int w = image.width();
	int h = image.height();
	m_self->glGenTextures(1, &tex);
	m_self->glBindTexture(GL_TEXTURE_2D, tex);
	m_self->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	m_self->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	m_self->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, image.bits());
	m_polyTex2.push_back(tex);
	m_is_use_tex2 = true;
}

size_t GLBuffer::countTextures() const
{
	return m_polyTex.size();
}

size_t GLBuffer::countCountPoint() const
{
	return m_polycnt.size();
}

/////////////////////////////////

float A2R(float val)
{
    return val * _PI / 180;
}

float R2A(float val)
{
    return val * 180 / _PI;
}

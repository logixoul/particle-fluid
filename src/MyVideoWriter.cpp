#include "precompiled.h"
#include "MyVideoWriter.h"

static cv::Mat dlToMat(gl::TextureRef tex, int mipLevel) {
	ivec2 sz = gl::Texture2d::calcMipLevelSize(mipLevel, tex->getWidth(), tex->getHeight());
	cv::Mat data = cv::Mat(sz.y, sz.x, CV_8UC3);

	bind(tex);
	glGetTexImage(GL_TEXTURE_2D, mipLevel, GL_BGR, GL_UNSIGNED_BYTE, data.data);

	return data;
}

MyVideoWriter::MyVideoWriter()
{
}


MyVideoWriter::~MyVideoWriter()
{
	m_videoWriter.release();
}

void MyVideoWriter::write(gl::TextureRef tex)
{
	if (!m_initialized) {
		init(tex->getWidth(), tex->getHeight());
	}
	auto mat = dlToMat(tex, 0);
	m_videoWriter.write(mat);
}

void MyVideoWriter::init(int w, int h)
{
	m_videoWriter = cv::VideoWriter("testVideo.mp4", //cv::CAP_FFMPEG, // has to be absent because otherwise i get isOpened=false
		cv::VideoWriter::fourcc('m', 'p', '4', 'v'), // lx: has to be lowercase, because otherwise i get isOpened=false.
		60, cv::Size(w, h), true);
	m_initialized = true;
}

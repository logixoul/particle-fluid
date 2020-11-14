#pragma once

#include <opencv2/videoio.hpp>

class MyVideoWriter
{
public:
	MyVideoWriter();
	~MyVideoWriter();
	void write(gl::TextureRef tex);

private:
	cv::VideoWriter m_videoWriter;
	void init(int w, int h);
	bool m_initialized = false;
};

#include "PixBufferRenderer.h"
#include <QPainter>

PixBufferRenderer::PixBufferRenderer(QWidget *parent) : QWidget(parent)
{
    pixels = new QImage(FRIDGE_GPU_FRAME_EGA_WIDTH, FRIDGE_GPU_FRAME_EGA_HEIGHT, QImage::Format_RGB888);
    sys = nullptr;
}

QSize PixBufferRenderer::sizeHint() const
{
    return QSize(FRIDGE_GPU_FRAME_EGA_WIDTH*4, FRIDGE_GPU_FRAME_EGA_HEIGHT*4);
}

QSize PixBufferRenderer::minimumSizeHint() const
{
    return QSize(FRIDGE_GPU_FRAME_EGA_WIDTH, FRIDGE_GPU_FRAME_EGA_HEIGHT);
}

void PixBufferRenderer::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    if (sys)
    {
        FRIDGE_gpu_render_ega_rgb8(sys->gpu, pixels->bits());
    }
    //painter.fillRect(painter.window(), QBrush(QColor(0, 128, 0)));
    painter.drawImage(painter.window(), *pixels);
}

#ifndef PIXBUFFERRENDERER_H
#define PIXBUFFERRENDERER_H

#include <QWidget>

extern "C" {
#include <fridgemulib.h>
}

class PixBufferRenderer : public QWidget
{
    Q_OBJECT
private:
    QImage* pixels;
public:
    FRIDGE_SYSTEM* sys;

    explicit PixBufferRenderer(QWidget *parent = nullptr);

    QSize sizeHint() const;
    QSize minimumSizeHint() const;
    void paintEvent(QPaintEvent* event);
signals:

};

#endif // PIXBUFFERRENDERER_H

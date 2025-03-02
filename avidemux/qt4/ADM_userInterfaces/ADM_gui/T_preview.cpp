
/***************************************************************************
    Handle all redraw operation for QT4

    copyright            : (C) 2006/2015 by mean
    email                : fixounet@free.fr
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QFrame>

#include <QImage>
#include <QPaintEngine>
#include <QPainter>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0) && !defined(__APPLE__)
#include <QWindow>
#endif

/* Probably on unix/X11 ..*/
#ifdef __APPLE__
#include <Carbon/Carbon.h>
#elif !defined(_WIN32)
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <QX11Info> // removed in qt5
#else
extern "C"
{
    extern void *XOpenDisplay(char *name);
}
#endif
#endif
// clang format-off
#include "ADM_assert.h"
#include "DIA_coreToolkit.h"
#include "GUI_ui.h"
#include "GUI_render.h"
#include "GUI_accelRender.h"
#include "T_preview.h"
// clang format-on

void UI_QT4VideoWidget(QFrame *host);
extern QWidget *QuiMainWindows;

static uint32_t displayW = 0, displayH = 0;
static ADM_Qvideo *videoWindow = NULL;

void DIA_previewInit(uint32_t width, uint32_t height)
{
}
uint8_t DIA_previewUpdate(uint8_t *data)
{
    return 1;
}
void DIA_previewEnd(void)
{
}
uint8_t DIA_previewStillAlive(void)
{
    return 1;
}
extern void callBackQtWindowDestroyed();

//****************************************************************************************************
/*
  Function to display
  Warning the incoming data are YUV!
  They are translated to RGB32 by the colorconv instance.
*/
//****************************************************************************************************
/*
  This is the class that will display the video images.
  It is a base QWidget where the image will be put by painter.

*/
bool ADM_QPreviewCleanup(void)
{
    return true;
}
/**
 *
 * @param z
 */
#ifdef __HAIKU__
ADM_Qvideo::ADM_Qvideo(QWidget *z) : QWidget(z)
{
}
#else
ADM_Qvideo::ADM_Qvideo(QFrame *z) : QWidget(z)
{
    useExternalRedraw(true);
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    // Put a transparent background
    // setAutoFillBackground(true);
    QPalette p = palette();
    QColor color(Qt::black);
    color.setAlpha(0);
    p.setColor(QPalette::Window, color);
    setPalette(p);
#endif

    drawer = NULL;
    doOnce = false;
    _width = _height = 0;
    hostFrame = z;

} //{setAutoFillBackground(false);}
#endif // Haiku

ADM_Qvideo::~ADM_Qvideo()
{
    printf("[Qvideo]Destroying QVideo\n");
    callBackQtWindowDestroyed();
}

/**
 *
 * @param ev
 */
void ADM_Qvideo::paintEvent(QPaintEvent *ev)
{
    // printf("Paint event\n");
    if (drawer)
    {
        drawer->draw(this, ev);
        return;
    }
    if (!doOnce)
    {
        printf("[QVideo] Using default redraw-------------->\n");
        QPainter painter(this);
        QColor blackColor(Qt::black);
        QImage allBlack(width(), height(), QImage::Format_RGB32);
        allBlack.fill(blackColor);
        painter.drawImage(0, 0, allBlack);
        printf("<--------------[QVideo]/ Using default redraw\n");
        doOnce = true;
    }
    else
    {
        renderExposeEventFromUI();
    }
}
/**
    \fn setADMSize
*/
void ADM_Qvideo::setADMSize(int width, int height)
{
    _width = width;
    _height = height;
#if !defined(__APPLE__) && !defined(_WIN32) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Work around an issue with Qt 6.2.1 which results in video frame displaying
    // garbage for all subsequently loaded videos after its size was set to zero.
    static uint8_t workaround = 0;
#define RUNTIME_VERSION_CHECKED 1
#define WORKAROUND_NEEDED 2
    if (!(workaround & RUNTIME_VERSION_CHECKED))
    {
        char *runtimeQtVersion = ADM_strdup(qVersion()); // We need the runtime version, not the build version.
        if (runtimeQtVersion && strlen(runtimeQtVersion) > 2)
        {
            char *major = runtimeQtVersion;
            char *minor = strchr(runtimeQtVersion, '.');
            if (minor)
            {
                *minor++ = 0;
                if (!strcmp(major, "6") && strlen(minor) && atoi(minor) > 1)
                    workaround |= WORKAROUND_NEEDED;
            }
        }
        ADM_dealloc(runtimeQtVersion);
        runtimeQtVersion = NULL;
        workaround |= RUNTIME_VERSION_CHECKED;
    }
    if (workaround & WORKAROUND_NEEDED)
    {
        if (width < 1)
            width = 1;
        if (height < 1)
            height = 1;
    }
#endif
    hostFrame->setFixedSize(width, height);
    setFixedSize(width, height);
}

void UI_QT4VideoWidget(QFrame *host)
{
    videoWindow = new ADM_Qvideo(host);
    videoWindow->show();
}
//*************************
/**
    \brief return pointer to the drawing widget that displays video
*/
void *UI_getDrawWidget(void)
{
    return (void *)videoWindow;
}

/**
      \brief Resize the window
*/
void UI_updateDrawWindowSize(void *win, uint32_t w, uint32_t h)
{

    displayW = w;
    displayH = h;

    // Resizing a maximized window results in not refreshed areas where widgets
    // in the maximized state were drawn with Qt5 on Linux, try to avoid this.
    // Instead, resize the window later on restore event if necessary.
    if (!QuiMainWindows->isMaximized())
    {
        UI_setBlockZoomChangesFlag(true);
        UI_resize(w, h);
        UI_setBlockZoomChangesFlag(false);
        UI_setNeedsResizingFlag(false);
    }
    else
    {
        UI_setNeedsResizingFlag(true);
    }
    videoWindow->setADMSize(w, h);
    if (!w || !h)
        QuiMainWindows->update(); // clean up the space previously occupied by the video window on closing
#if 0
	
	UI_purge();

// Trolltech need to get their act together.  Resizing doesn't work well or the same on all platforms.
#if defined(__APPLE__)
	// Hack required for Mac to resize properly.  adjustSize() just doesn't cut the mustard.
	QuiMainWindows->resize(QuiMainWindows->width() + 1, QuiMainWindows->height() + 1);
#else
	// resizing doesn't work unless called twice on Windows and Linux.
	QuiMainWindows->adjustSize();
#endif
#endif

    UI_purge();

    printf("[RDR] Resizing to %u x %u\n", displayW, displayH);
}

/**
      \brief Retrieve info from window, needed for accel layer
*/
void UI_getWindowInfo(void *draw, GUI_WindowInfo *xinfo)
{
    ADM_assert(videoWindow);
    QWidget *widget = videoWindow->parentWidget();
    xinfo->widget = videoWindow;
    xinfo->systemWindowId = 0;
    xinfo->scalingFactor = 1.;

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0) && !defined(__APPLE__)
    QWindow *window = QuiMainWindows->windowHandle();
    if (window)
        xinfo->scalingFactor = (double)window->devicePixelRatio();
#endif

#if defined(_WIN32)
    xinfo->display = (void *)videoWindow->winId();
    xinfo->systemWindowId = videoWindow->winId();
#elif defined(__APPLE__)
    xinfo->display = NULL; // we may not call winId() on a QWidget on macOS, it breaks OpenGL
    xinfo->systemWindowId = 0;
#else // linux
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    const QX11Info &info = videoWindow->x11Info();
    xinfo->display = info.display();
#else
    {
        static void *myDisplay = NULL;
        if (!myDisplay)
            myDisplay = XOpenDisplay(NULL);
        xinfo->display = myDisplay;
    }
#endif
    xinfo->systemWindowId = videoWindow->winId();
#endif
    QPoint localPoint(0, 0);
    QPoint windowPoint = videoWindow->mapToGlobal(localPoint);
    xinfo->x = windowPoint.x();
    xinfo->y = windowPoint.y();
    xinfo->width = displayW;
    xinfo->height = displayH;
}
/**
 * \brief DEPRECATED
 * @return
 */
void UI_rgbDraw(void *widg, uint32_t w, uint32_t h, uint8_t *ptr)
{
    videoWindow->repaint();
}

//****************************************************************************************************
// EOF

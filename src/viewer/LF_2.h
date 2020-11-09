#ifdef _MSC_VER
#pragma once
#endif
#ifndef LF_2_H
#define LF_2_H
#include <string>
#include <vector>
#include <memory>

#include <qopenglwidget.h>
#include <qopenglbuffer.h>
#include <qopenglvertexarrayobject.h>
#include <qopenglfunctions.h>
#include <qopenglshaderprogram.h>
#include <qopengltexture.h>
#include <qtimer.h>
#include <qevent.h>
#include <qpoint.h>
#include <string>
#include <map>
#include <time.h>
#include<iostream>

#include "imageinfo.h"
using namespace std;

#define l_u 9
#define l_v 9
#define l_w 512
#define l_h 512
class LightFieldWidget2 : public QOpenGLWidget, private QOpenGLFunctions {
    Q_OBJECT

public:
    explicit LightFieldWidget2(QWidget* parent = nullptr);
    virtual ~LightFieldWidget2();

    void setLightField(const std::vector<ImageInfo>& viewInfos, int rows, int cols);
    void setLabel(const std::vector<ImageInfo>& viewInfos, int rows, int cols);
    void setFocusPoint(float value);
    void setApertureSize(float value);
    float focusPoint() const;
    float apertureSize() const;
    void test();
    void save_image();
    void save_label();
     void setLabel2(const std::vector<ImageInfo>& viewInfos, int rows, int cols);
    //int label[l_u][l_v][l_w][l_h];
    //map<string,int> label_map;
    //string label_key[label_idx];

    QPointF cameraPosition;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;

protected slots:
    void animate();

private:
    std::unique_ptr<QOpenGLVertexArrayObject> vao;
    std::unique_ptr<QOpenGLBuffer> vbo;
    std::unique_ptr<QOpenGLBuffer> ibo;

    std::unique_ptr<QOpenGLShaderProgram> shaderProgram;
    std::unique_ptr<QOpenGLTexture> lightFieldTexture;

    std::unique_ptr<QTimer> timer;

    float focus, aperture;
    int lfRows, lfCols;
    QSize imageSize;
    QPoint prevMouseClick;

    bool isClick;
};

#endif // LF_2_H


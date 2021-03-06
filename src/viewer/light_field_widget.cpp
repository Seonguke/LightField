#include "light_field_widget.h"

#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <unistd.h>
#include <dirent.h>
#include <QtGui/qmatrix4x4.h>
#include <QtGui/qvector3d.h>
#include <stdio.h>
#include "directories.h"

using namespace std;
static constexpr uint32_t indexBufferSize = 6;
#define label_idx 2000
map<string,int> label_map_s;
int max_label=0;
int label[l_u][l_v][l_w][l_h]={0,};
map<int,int> label_map;
map<string,int>::iterator p;
string label_key[label_idx];
int ll[l_h*l_w]={0,};
struct Vertex {
    QVector3D position;
    QVector2D texcoord;
};
struct col_list{
    int r=0;
    int g=0;
    int b=0;
    int v=0;
};
col_list col_val[2000];

void int2hex(int input,string &s) {

    char hexadecimal[20] = { 0, };

    int position = 0;
    while (1)
    {
        int mod = input % 16;
        if (mod < 10)
        {

            hexadecimal[position] = 48 + mod;
        }
        else
        {
            hexadecimal[position] = 65 + (mod - 10);
        }

        input = input / 16;

        position++;

        if (input == 0)
            break;
    }


    for (int i = position - 1; i >= 0; i--)
    {
         s+=hexadecimal[i];
    }

}
LightFieldWidget::LightFieldWidget(QWidget* parent)
    : QOpenGLWidget(parent)
    , focus(0.0f)
    , aperture(5.0f)
    , lfRows(0)
    , lfCols(0)
    , imageSize(1, 1)
    , cameraPosition(0.5f, 0.5f)
    , isClick(false) {
    // Setup timer
    timer = std::make_unique<QTimer>(this);
    timer->start(10);
    connect(timer.get(), SIGNAL(timeout()), this, SLOT(animate()));
}

LightFieldWidget::~LightFieldWidget() {
}

QSize LightFieldWidget::sizeHint() const {
    //const int h = 512;
    //const int w = h * imageSize.width() / imageSize.height();
    return QSize(l_h, l_w);
}

QSize LightFieldWidget::minimumSizeHint() const {
    return sizeHint();
}


void LightFieldWidget::initializeGL() {
    // Initialize OpenGL functions
    initializeOpenGLFunctions();

    // Clear background color
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Compile and link shader programs
    const QString vShaderFile = QString(SHADER_DIRECTORY) + "lightfield.vert";
    const QString fShaderFile = QString(SHADER_DIRECTORY) + "lightfield.frag";
    shaderProgram = std::make_unique<QOpenGLShaderProgram>(this);
    shaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, vShaderFile);
    shaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, fShaderFile);
    shaderProgram->link();

    if (!shaderProgram->isLinked()) {
        std::cerr << "[ERROR: failed to link shader program!!" << std::endl;
        exit(1);
    }

    // Setup VAO for image plane
    const std::vector<Vertex> vertices = {
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
        {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f}},
        {{ 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}}
    };

    const std::vector<uint32_t> indices = {
        0u, 1u, 2u, 0u, 2u, 3u
    };

    vao = std::make_unique<QOpenGLVertexArrayObject>(this);
    vao->create();
    vao->bind();

    vbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    vbo->create();
    vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
    vbo->bind();
    vbo->allocate(vertices.data(), sizeof(Vertex) * vertices.size());

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3 ,GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texcoord));

    ibo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
    ibo->create();
    ibo->setUsagePattern(QOpenGLBuffer::StaticDraw);
    ibo->bind();
    ibo->allocate(indices.data(), sizeof(uint32_t) * indices.size());

    vao->release();
}

void LightFieldWidget::resizeGL(int width, int height) {
    glViewport(0, 0, width, height);
}

void LightFieldWidget::paintGL() {
    // If texture is not loaded, then return.
    if (!lightFieldTexture) return;

    // Clear buffers
    glClear(GL_COLOR_BUFFER_BIT);

    // Enable shader
    shaderProgram->bind();

    // Set camera parameters
    shaderProgram->setUniformValue("focusPoint", focus);
    shaderProgram->setUniformValue("apertureSize", aperture);
    shaderProgram->setUniformValue("cameraPositionX", (float)cameraPosition.x());
    shaderProgram->setUniformValue("cameraPositionY", (float)cameraPosition.y());

    // Set texture parameters
    lightFieldTexture->bind(0);
    shaderProgram->setUniformValue("textureImages", 0);
    shaderProgram->setUniformValue("focusPoint", focus);
    shaderProgram->setUniformValue("apertureSize", aperture);
    shaderProgram->setUniformValue("rows", lfRows);
    shaderProgram->setUniformValue("cols", lfCols);

    vao->bind();
    glDrawElements(GL_TRIANGLES, indexBufferSize, GL_UNSIGNED_INT, 0);
    vao->release();

    lightFieldTexture->release(0);
}

void LightFieldWidget::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::MouseButton::LeftButton) {
        isClick = true;
        prevMouseClick = ev->pos();
    }
}

void LightFieldWidget::mouseMoveEvent(QMouseEvent* ev) {
    if (ev->buttons() & Qt::MouseButton::LeftButton) {
        if (isClick) {
            const double size = (double)std::min(width(), height());
            const double dx = (double)(ev->pos().x() - prevMouseClick.x()) / size;
            const double dy = (double)(ev->pos().y() - prevMouseClick.y()) / size;
            cameraPosition.setX(std::max(0.0, std::min(cameraPosition.x() + dx, 1.0)));
            cameraPosition.setY(std::max(0.0, std::min(cameraPosition.y() + dy, 1.0)));

        }
    }
}

void LightFieldWidget::mouseReleaseEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::MouseButton::LeftButton) {
        isClick = false;
    }
}
void LightFieldWidget::save_image(){

    int w = width();
    int h = height();//
    int bytesPerPixel = 3;
    int strideSize = (w * (4 - bytesPerPixel)) % 4;
    int bytesPerLine = w * 3 + strideSize;
    int size = bytesPerLine * h;
    GLubyte * pixel = new GLubyte[size];
    glReadPixels(9, 9, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixel);

    QImage image(pixel, w, h, bytesPerLine, QImage::Format::Format_RGB888);
    QImage img=image.mirrored(false,true);
    bool result = img.save("/home/jo/LightField/src/viewer/save/image.png", "PNG");
    cout<< "Save_image"<<endl;

   /*
    pngwriter PNG(width, height, 1.0, fileName);
    size_t x = 1;
    size_t y = 1;
    double R, G, B;
    for(size_t i=0; i<nPixels; i++) // "i" is the index for array "pixels"
    {
             switch(i%3)
            {
                  case 2:
                        B = static_cast<double>(pixels[i]); break;
                  case 1:
                        G = static_cast<double>(pixels[i]); break;
                  case 0:
                        R = static_cast<double>(pixels[i]);
                        PNG.plot(x, y, R, G, B);     // set pixel to position (x, y)
                        if( x == l_w )             // Move to the next row of image
                        {
                              x=1;
                              y++;
                         }
                         else                       // To the next pixel
           */
}
void LightFieldWidget::save_label(){
    int w = width();
    int h = height();//
    int bytesPerPixel = 3;
    int strideSize = (w * (4 - bytesPerPixel)) % 4;
    int bytesPerLine = w * 3 + strideSize;
    int size = bytesPerLine * h;
    GLubyte * pixel = new GLubyte[size];
    glReadPixels(9+w+6, 9, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixel);
    for(int i=0;i<size;i+=3){//not flip first 2d  ->1d
        //printf("R%u   G%u    B%u\n",pixel[i+0],pixel[i+1],pixel[i+2]);
        int aa=0,ss=0,dd=0;
        //a=((pixel[i+1])*1000)+pixel[i+2];
        aa=pixel[i+0];
        ss=pixel[i+1];
        dd=pixel[i+2];
        string r,g,b,k;
        int2hex(aa,r);
        int2hex(ss,g);
        int2hex(dd,b);
        k=r+g+b;
        p=label_map_s.find(k);
        if(p==label_map_s.end()) ll[i/3]=0;
        else{
            ll[i/3]=label_map_s[k];
        }

    }
    QImage image(pixel, w, h, bytesPerLine, QImage::Format::Format_RGB888);
    QImage img=image.mirrored(false,true);
    bool result = img.save("/home/jo/LightField/src/viewer/save/image2.png", "PNG");

    std::ofstream ofile("label2.txt");
    std::string str = "Write test";
    if (ofile.is_open()) {
        for(int i=0;i<label_idx;i++)
        ofile << label_key[i]<<endl;
        ofile.close(); }




std::ofstream sofile("label.txt");
if (sofile.is_open()) {
    int txt_out[l_h][l_w]={0,};
    for(int i=0;i<l_w*l_w;i++)
        //ofile << ll[i]<<endl;
        txt_out[(i/l_h)][i%l_w]=ll[i];
    for(int i=511; i>=0;i--)
        for(int j=0;j<l_w;j++)
            sofile << txt_out[i][j]<<endl;
    sofile.close(); }


cout<< "Save_Label"<<endl;
}

void LightFieldWidget::setLightField(const std::vector<ImageInfo>& viewInfos,
                                     int rows, int cols) {
    makeCurrent();
    this->lfRows = rows;
    this->lfCols = cols;

    // Check image size of each view
    QImage temp(viewInfos[0].path());
    if (temp.isNull()) {
        qDebug("[ERROR] failed to load image: %s",
               viewInfos[0].path().toStdString().c_str());
        std::abort();
    }
    const int imageW = temp.width();
    const int imageH = temp.height();
    this->imageSize = QSize(imageW, imageH);
    updateGeometry();

    // Create large tiled image
    std::vector<uint8_t> imageData((rows * cols) * imageH * imageW * 3);
    for (int k = 0; k < viewInfos.size(); k++) {
        QImage img(viewInfos[k].path());
        if (img.width() != imageW || img.height() != imageH) {
            qDebug("[ERROR] image size invalid!!");
            std::abort();
        }

        for (int y = 0; y < imageH; y++) {
            for (int x = 0; x < imageW; x++) {
                const QColor color = img.pixelColor(x, y); // 여기서 픽셀값 가져가는구만유
                imageData[((k * imageH + y) * imageW + x) * 3 + 0] = (uint8_t)color.red();
                imageData[((k * imageH + y) * imageW + x) * 3 + 1] = (uint8_t)color.green();
                imageData[((k * imageH + y) * imageW + x) * 3 + 2] = (uint8_t)color.blue();
            }
        }
    }

    lightFieldTexture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target3D);
    lightFieldTexture->setAutoMipMapGenerationEnabled(false);
    lightFieldTexture->setMinMagFilters(QOpenGLTexture::Filter::Linear, QOpenGLTexture::Filter::Linear);
    lightFieldTexture->setWrapMode(QOpenGLTexture::CoordinateDirection::DirectionS,
                                   QOpenGLTexture::WrapMode::ClampToEdge);
    lightFieldTexture->setWrapMode(QOpenGLTexture::CoordinateDirection::DirectionT,
                                   QOpenGLTexture::WrapMode::ClampToEdge);
    lightFieldTexture->setWrapMode(QOpenGLTexture::CoordinateDirection::DirectionR,
                                   QOpenGLTexture::WrapMode::ClampToEdge);
    lightFieldTexture->setFormat(QOpenGLTexture::TextureFormat::RGB8_UNorm);
    lightFieldTexture->setSize(imageW, imageH, rows * cols);
    lightFieldTexture->allocateStorage(QOpenGLTexture::PixelFormat::RGB,
                                       QOpenGLTexture::PixelType::UInt8);
    lightFieldTexture->setData(0, 0, QOpenGLTexture::PixelFormat::RGB,
                               QOpenGLTexture::PixelType::UInt8, imageData.data());

    qDebug("[INFO] light field texture is binded!!");

}

void LightFieldWidget::setLabel(const std::vector<ImageInfo>& viewInfos, int rows, int cols) {
    makeCurrent();
    this->lfRows = rows;
    this->lfCols = cols;
    LightFieldWidget::test();

    // Check image size of each view
    QImage temp(viewInfos[0].path());
    if (temp.isNull()) {
        qDebug("[ERROR] failed to load image: %s",
               viewInfos[0].path().toStdString().c_str());
        std::abort();
    }

    const int imageW = temp.width();
    const int imageH = temp.height();
    this->imageSize = QSize(imageW, imageH);
    updateGeometry();
    //int label[l_u][l_v][l_w][l_h];
    //map<string,int> label_map;
    //string label_key[label_idx];//hex
    // Create large tiled image

    //cout<<label_key[3];
    qDebug("Before%d",cols);

    std::vector<uint8_t> imageData((rows * cols) * imageH * imageW * 3);


    qDebug("After");

    for (int k = 0; k < viewInfos.size(); k++) {
        QImage img(viewInfos[k].path());
        if (img.width() != imageW || img.height() != imageH) {
            qDebug("[ERROR] image size invalid!!");
            std::abort();
        }
        //qDebug("col%d row%d",viewInfos[k].col(),viewInfos[k].row());

        int cnt=0;
        for (int y = 0; y < imageH; y++) {
            for (int x = 0; x < imageW; x++) {

                //max_label=max(max_label,label[8-viewInfos[k].row()][8-viewInfos[k].col()][y][x]);

                //imageData[((k * imageH + y) * imageW + x) * 3 + 0]= col_val[label[8-viewInfos[k].row()][8-viewInfos[k].col()][y][x]].r;
                //imageData[((k * imageH + y) * imageW + x) * 3 + 1]=  col_val[label[8-viewInfos[k].row()][8-viewInfos[k].col()][y][x]].g;
                //imageData[((k * imageH + y) * imageW + x) * 3 + 2]=  col_val[label[8-viewInfos[k].row()][8-viewInfos[k].col()][y][x]].b;
                string hx_r,hx_g,hx_b,xx;
                               xx=label_key[label[8-viewInfos[k].row()][viewInfos[k].col()][y][x]];
                               hx_r+=xx[0];
                               hx_r+=xx[1];
                               hx_g+=xx[2];
                               hx_g+=xx[3];
                               hx_b+=xx[4];
                               hx_b+=xx[5];

                               imageData[((k * imageH + y) * imageW + x) * 3 + 0]= (uint8_t)strtol(hx_r.c_str(), NULL, 16);
                               imageData[((k * imageH + y) * imageW + x) * 3 + 1]= (uint8_t)strtol(hx_g.c_str(), NULL, 16);
                               imageData[((k * imageH + y) * imageW + x) * 3 + 2]= (uint8_t)strtol(hx_b.c_str(), NULL, 16);

            }
        }
    }
    //const QString vShaderFile = QString(SHADER_DIRECTORY) + "lightfield.vert";
    //const QString fShaderFile = QString(SHADER_DIRECTORY) + "lightfield3.frag";
    //shaderProgram = std::make_unique<QOpenGLShaderProgram>(this);
    //shaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, vShaderFile);
    //shaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, fShaderFile);
    //shaderProgram->link();
    lightFieldTexture = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target3D);
    lightFieldTexture->setAutoMipMapGenerationEnabled(false);
    lightFieldTexture->setMinMagFilters(QOpenGLTexture::Filter::Nearest, QOpenGLTexture::Filter::Nearest);
    //faker Nearest 얘는 그냥 아에 안변하는 그런느낌 걍 조떄써 color는 안변한다.
    lightFieldTexture->setWrapMode(QOpenGLTexture::CoordinateDirection::DirectionS,
                                   QOpenGLTexture::WrapMode::ClampToEdge);
    lightFieldTexture->setWrapMode(QOpenGLTexture::CoordinateDirection::DirectionT,
                                   QOpenGLTexture::WrapMode::ClampToEdge);
    lightFieldTexture->setWrapMode(QOpenGLTexture::CoordinateDirection::DirectionR,
                                   QOpenGLTexture::WrapMode::ClampToEdge);
    lightFieldTexture->setFormat(QOpenGLTexture::TextureFormat::RGB8_UNorm);//왜 Unormalization 해야되냐 ㅅㅂ ㅅㅄㅄㅄㅄㅂ
    lightFieldTexture->setSize(imageW, imageH, rows * cols);
    lightFieldTexture->allocateStorage(QOpenGLTexture::PixelFormat::RGB,
                                       QOpenGLTexture::PixelType::UInt8);
    lightFieldTexture->setData(0, 0, QOpenGLTexture::PixelFormat::RGB,
                               QOpenGLTexture::PixelType::UInt8, imageData.data());

    qDebug("[INFO] light field texture is binded!!");
}
void LightFieldWidget::test(){
    DIR            *dir_info;
    struct dirent  *dir_entry;
    string D_P = "/home/jo/LightField/src/viewer/stillLife/real_label/";
    dir_info = opendir(D_P.c_str());              // 현재 디렉토리를 열기
    if ( NULL != dir_info)
       {
          while( dir_entry   = readdir( dir_info))  // 디렉토리 안에 있는 모든 파일과 디렉토리 출력
          {
             if(strcmp(dir_entry->d_name,"..")==0||strcmp(dir_entry->d_name,".")==0)continue;
             else {
            // file read
            fstream fs;
            string str_buf;
            fs.open(D_P + dir_entry->d_name, ios::in);
            char str1= dir_entry->d_name[0]; // can connect 0~9 u_v idex
            char str2 = dir_entry->d_name[2];

            int u_ = str1 - '0';//col
            int v_ = str2 - '0';//row
            //cout << v_<< " " << u_ << endl;
            //cout << str2<< " " << str1 << endl;

            int indx=0;

            while (!fs.eof()){

                getline(fs, str_buf);

                label[u_][v_][indx/l_w][indx%l_w] = atoi(str_buf.c_str());
                //cout<<label[u_][v_][indx/l_w][indx%l_w]<<endl;
                 //cout<< indx/l_w<<" "<<indx%l_w<<endl;
                //qDebug("\n CLEAR LOAD IN  %d",label[u_][v_][indx/l_w][indx%l_w]);
                indx++;	//indx must w=h

            }
            //cout<<(indx-1)<<endl;
            str_buf.clear();

             }//else end
          }//while end
          closedir( dir_info);
       }   // if end


    string hex = "123456789ABCDEF";//0~14
        for (int cnt = 0; cnt < label_idx; cnt++) {
            string hex_r, hex_g, hex_b;
            for (int i = 0; i < 2; i++) {
                hex_r += hex[rand() % 15];
                hex_g += hex[rand() % 15];
                hex_b += hex[rand() % 15];

            }// index -> hex color

            label_key[cnt] = hex_r + hex_g + hex_b;
             label_map_s.insert({ label_key[cnt],cnt });

        }//gen hex
       qDebug("\n CLEAR LOAD IN  %s",D_P.c_str());

       //for(int i=0; i<2000;i++){
       //    col_val[i].g=i/256;
       //    col_val[i].b=i%256;
       //    col_val[i].v=(col_val[i].g*1000)+col_val[i].b;
       //    label_map.insert({col_val[i].v,i });
       //}
}
void LightFieldWidget::setFocusPoint(float value) {
    focus = value;
}

void LightFieldWidget::setApertureSize(float value) {
    aperture = value;
}

float LightFieldWidget::focusPoint() const {
    return focus;
}

float LightFieldWidget::apertureSize() const {
    return aperture;
}

void LightFieldWidget::animate() {
    update();
}

#include "BasicParticles.h"

#include <QtGlobal>

#include <QDebug>
#include <QFile>
#include <QImage>
#include <QTime>

#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>

#include <../glm/glm.hpp>

#include <cmath>
#include <cstring>

MyWindow::~MyWindow()
{
    if (mProgram != 0) delete mProgram;
}

MyWindow::MyWindow()
    : mProgram(0), currentTimeMs(0), currentTimeS(0), tPrev(0), angle(M_PI/2.0f)
{
    setSurfaceType(QWindow::OpenGLSurface);
    setFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setMajorVersion(4);
    format.setMinorVersion(3);
    format.setSamples(4);
    format.setProfile(QSurfaceFormat::CoreProfile);
    setFormat(format);
    create();

    resize(800, 600);

    mContext = new QOpenGLContext(this);
    mContext->setFormat(format);
    mContext->create();

    mContext->makeCurrent( this );

    mFuncs = mContext->versionFunctions<QOpenGLFunctions_4_3_Core>();
    if ( !mFuncs )
    {
        qWarning( "Could not obtain OpenGL versions object" );
        exit( 1 );
    }
    if (mFuncs->initializeOpenGLFunctions() == GL_FALSE)
    {
        qWarning( "Could not initialize core open GL functions" );
        exit( 1 );
    }

    initializeOpenGLFunctions();

    QTimer *repaintTimer = new QTimer(this);
    connect(repaintTimer, &QTimer::timeout, this, &MyWindow::render);
    repaintTimer->start(1000/60);

    QTimer *elapsedTimer = new QTimer(this);
    connect(elapsedTimer, &QTimer::timeout, this, &MyWindow::modCurTime);
    elapsedTimer->start(1);       
}

void MyWindow::modCurTime()
{
    currentTimeMs++;
    currentTimeS=currentTimeMs/1000.0f;
}

void MyWindow::initialize()
{
    CreateVertexBuffer();
    initShaders();
    initMatrices();

    PrepareTexture(GL_TEXTURE0, GL_TEXTURE_2D, "../Media/bluewater.png", false);

    glFrontFace(GL_CCW);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    mFuncs->glPointSize(10.0f);
}

void MyWindow::CreateVertexBuffer()
{
    nParticles = 8000;

    // Create and populate the buffer objects
    unsigned int initVelHandle;
    unsigned int startTimeHandle;

    glGenBuffers(1, &initVelHandle);
    glGenBuffers(1, &startTimeHandle);

    int size = nParticles * sizeof(float);

    glBindBuffer(GL_ARRAY_BUFFER, initVelHandle);
    glBufferData(GL_ARRAY_BUFFER, 3 * size, NULL, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, startTimeHandle);
    glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_STATIC_DRAW);

    // Velocity data
    QVector3D v;
    float     velocity, theta, phi;
    GLfloat  *data = new GLfloat[nParticles * 3];
    for (unsigned int i = 0; i < nParticles; i++ )
    {
        theta = glm::mix(0.0f, (float)M_PI / 6.0f, randFloat());
        phi   = glm::mix(0.0f, (float)M_PI * 2.0f, randFloat());

        v.setX(sinf(theta) * cosf(phi));
        v.setY(cosf(theta));
        v.setZ(sinf(theta) * sinf(phi));

        velocity = glm::mix(1.25f, 1.5f, randFloat());
        v = v.normalized() * velocity;

        data[3*i]   = v.x();
        data[3*i+1] = v.y();
        data[3*i+2] = v.z();
    }
    glBindBuffer(GL_ARRAY_BUFFER, initVelHandle);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size * 3, data);
    delete [] data;

    // Start time data
    data = new GLfloat[nParticles];
    float time = 0.0f;
    float rate = 0.00075f;
    for( unsigned int i = 0; i < nParticles; i++ )
    {
        data[i] = time;
        time   += rate;
    }
    glBindBuffer(GL_ARRAY_BUFFER, startTimeHandle);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
    delete [] data;

    // Setup the VAO
    mFuncs->glGenVertexArrays(1, &mVAOParticles);
    mFuncs->glBindVertexArray(mVAOParticles);

    mFuncs->glBindVertexBuffer(0, initVelHandle, 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(0, 0);

    mFuncs->glBindVertexBuffer(1, startTimeHandle, 0, sizeof(GLfloat));
    mFuncs->glVertexAttribFormat(1, 1, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(1, 1);

    mFuncs->glBindVertexArray(0);

}

void MyWindow::initMatrices()
{
    ViewMatrix.lookAt(QVector3D(3.0f * cos(angle), 1.5f, 3.0f * sin(angle)), QVector3D(0.0f, 1.5f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));
}

void MyWindow::resizeEvent(QResizeEvent *)
{
    mUpdateSize = true;

    ProjectionMatrix.setToIdentity();
    ProjectionMatrix.perspective(60.0f, (float)this->width()/(float)this->height(), 0.3f, 100.0f);
}

void MyWindow::render()
{
    if(!isVisible() || !isExposed())
        return;

    if (!mContext->makeCurrent(this))
        return;

    static bool initialized = false;
    if (!initialized) {
        initialize();
        initialized = true;
    }

    if (mUpdateSize) {
        glViewport(0, 0, size().width(), size().height());
        mUpdateSize = false;
    }

    float deltaT = currentTimeS - tPrev;
    if(tPrev == 0.0f) deltaT = 0.0f;
    tPrev = currentTimeS;
    angle += 0.25f * deltaT;
    if (angle > TwoPI) angle -= TwoPI;

    static float EvolvingVal = 0.0f;

    if (animate == true) EvolvingVal += 0.01f;

    glClearColor(0.1f, 0.1f, 0.1f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    mFuncs->glBindVertexArray(mVAOParticles);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    mProgram->bind();
    {

        QMatrix4x4 mv1 = ViewMatrix;
        mProgram->setUniformValue("MVP", ProjectionMatrix * mv1);

        mProgram->setUniformValue("ParticleTex",      0);

        mProgram->setUniformValue("Time",             (float)currentTimeS);
        mProgram->setUniformValue("ParticleLifetime", 3.5f);
        mProgram->setUniformValue("Gravity",          QVector3D(0.0f, -0.2f, 0.0f));

        glDrawArrays(GL_POINTS, 0, nParticles);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    }
    mProgram->release();

    mContext->swapBuffers(this);
}

void MyWindow::initShaders()
{
    QOpenGLShader vShader(QOpenGLShader::Vertex);
    QOpenGLShader fShader(QOpenGLShader::Fragment);    
    QFile         shaderFile;
    QByteArray    shaderSource;

    //Simple ADS
    shaderFile.setFileName(":/vshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "vertex compile: " << vShader.compileSourceCode(shaderSource);

    shaderFile.setFileName(":/fshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "frag   compile: " << fShader.compileSourceCode(shaderSource);

    mProgram = new (QOpenGLShaderProgram);
    mProgram->addShader(&vShader);
    mProgram->addShader(&fShader);
    qDebug() << "shader link: " << mProgram->link();
}

void MyWindow::PrepareTexture(GLenum TextureUnit, GLenum TextureTarget, const QString& FileName, bool flip)
{
    QImage TexImg;

    if (!TexImg.load(FileName)) qDebug() << "Erreur chargement texture " << FileName;
    if (flip==true) TexImg=TexImg.mirrored();

    glActiveTexture(TextureUnit);
    GLuint TexObject;
    glGenTextures(1, &TexObject);
    glBindTexture(TextureTarget, TexObject);
    mFuncs->glTexStorage2D(TextureTarget, 1, GL_RGB8, TexImg.width(), TexImg.height());
    mFuncs->glTexSubImage2D(TextureTarget, 0, 0, 0, TexImg.width(), TexImg.height(), GL_BGRA, GL_UNSIGNED_BYTE, TexImg.bits());
    glTexParameteri(TextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(TextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void MyWindow::keyPressEvent(QKeyEvent *keyEvent)
{
    switch(keyEvent->key())
    {
        case Qt::Key_P:
            break;
        case Qt::Key_Up:
            break;
        case Qt::Key_Down:
            break;
        case Qt::Key_Left:
            break;
        case Qt::Key_Right:
            break;
        case Qt::Key_Delete:
            break;
        case Qt::Key_PageDown:
            break;
        case Qt::Key_Home:
            break;
        case Qt::Key_Z:
            break;
        case Qt::Key_Q:
            break;
        case Qt::Key_S:
            break;
        case Qt::Key_A:
            animate = !animate;
            break;
        case Qt::Key_W:
            break;
        case Qt::Key_E:
            break;
        default:
            break;
    }
}

float MyWindow::randFloat() {
    return ((float)rand() / RAND_MAX);
}

void MyWindow::printMatrix(const QMatrix4x4& mat)
{
    const float *locMat = mat.transposed().constData();

    for (int i=0; i<4; i++)
    {
        qDebug() << locMat[i*4] << " " << locMat[i*4+1] << " " << locMat[i*4+2] << " " << locMat[i*4+3];
    }
}

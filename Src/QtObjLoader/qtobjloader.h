#ifndef QTOBJLOADER_H
#define QTOBJLOADER_H

#include <Qt/qmainwindow.h>
#include <Qt/qgl.h>
#include <QMouseEvent>
#include "ui_qtobjloader.h"
#include "glm.h"
#include "datastructure.h"
#include <boost/shared_ptr.hpp>

class QObjView;
class QtObjLoader : public QMainWindow, public Ui_QtObjLoaderClass
{
	Q_OBJECT

public:
	QtObjLoader(QWidget *parent = 0);
	~QtObjLoader();

private slots:
	void openFile();

private:
	QObjView *m_pView;
};

class QObjView : public QGLWidget
{
	Q_OBJECT

public:
	QObjView(QWidget *parent = 0);
	void openFile(const QString &fileName);

	//build Zbuffer structure from obj file info
	boost::shared_ptr<mytype::Zbuffer> buildZbuffer();
	void printZBufferData(mytype::Zbuffer &buffer, const char* fileName = NULL);

protected:
	void paintEvent(QPaintEvent* event);

	//mouse event for rotation
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void keyPressEvent(QKeyEvent *event);

	//support for drag obj file
	void dragEnterEvent(QDragEnterEvent *event);
	void dragMoveEvent(QDragMoveEvent *event);
	void dropEvent(QDropEvent *event);

private:
	void initializePara();

	//use openGL API draw obj file
	void drawModel(GLMmodel *model, GLuint mode);

	void saveGLState();
	void restoreGLState();
	void initialize();
	void vecLMultiplyRotateMat(std::vector<GLfloat>& vec, const GLfloat *mat);

	//key function for running Zbuffer algorithm
	void runZBuffer(QPainter *painter);

private:
	//Transportation && rotate && scaling para
	GLfloat m_xRotation;
	GLfloat m_yRotation;
	GLfloat m_xTranslation;
	GLfloat m_yTranslation;
	GLfloat m_zTranslation;
	GLfloat m_xScaling;
	GLfloat m_yScaling;
	GLfloat m_zScaling;

	// Colors
	GLfloat m_ClearColorRed;
	GLfloat m_ClearColorGreen;
	GLfloat m_ClearColorBlue;

	QPoint m_ptLast;
	QString m_strOpenFileName;
	std::vector<QColor> m_vecColors;

	//save for modeview and projection array
	GLfloat m_modelViewArray[16];
	GLfloat m_projectArray[16];

	int m_nMaxHeight;
	int m_nMinHeight;

	GLMmodel *m_pModel;
	QObjView *m_pView;
};

inline void QObjView::vecLMultiplyRotateMat(std::vector<GLfloat>& vec, const GLfloat *mat)
{
	std::vector<GLfloat> backup(vec);
	vec[0] = mat[0] * backup[0] + mat[1] * backup[1] + mat[2] * backup[2];
	vec[1] = mat[4] * backup[0] + mat[5] * backup[1] + mat[6] * backup[2];
	vec[2] = mat[8] * backup[0] + mat[9] * backup[1] + mat[10] * backup[2];
}

#endif // QTOBJLOADER_H
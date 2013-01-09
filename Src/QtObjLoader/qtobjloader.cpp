#include "qtobjloader.h"
#include <Qt/qmessagebox.h>
#include <Qt/qfiledialog.h>
#include <QtCore/qurl.h>
#include <GL/glut.h>
#include <vector>
#include <map>
#include <assert.h>
#include <limits>
#include <iterator>
#include <fstream>
#include <ctime>
#include <boost/format.hpp>
#include <omp.h>

using namespace std;

//#define MEASURETIME
#ifdef MEASURETIME
	std::ofstream fMeasuretime("MeasureTime.log");
#endif

QtObjLoader::QtObjLoader(QWidget *parent)
	: QMainWindow(parent)
{
	setupUi(this);
	m_pView = new QObjView(this);
	setCentralWidget(m_pView);
	connect(actionOpenFile, SIGNAL(triggered()), this, SLOT(openFile()));
}

QtObjLoader::~QtObjLoader()
{
}

void QtObjLoader::openFile()
{
	QString fileName = QFileDialog::getOpenFileName(this, tr("请选择打开的文件"), tr("."), tr("OBJ Files(*.obj)"));
	if(fileName.isEmpty())	return;
	m_pView->openFile(fileName);
}

QObjView::QObjView( QWidget *parent /*= 0*/ )
	:QGLWidget(QGLFormat(QGL::DoubleBuffer |  QGL::DepthBuffer | QGL::SampleBuffers), parent)
{
	m_pModel = NULL;

	m_ClearColorRed=1.0f;
	m_ClearColorGreen=1.0f;
	m_ClearColorBlue=1.0f;

	setFocusPolicy(Qt::FocusPolicy::StrongFocus);
	setAcceptDrops(true);

	initialize();
}

void QObjView::openFile(const QString &fileName)
{
	if(fileName.isEmpty())	return;

	m_strOpenFileName = fileName;

	//delete the last model
	if(m_pModel != NULL) delete m_pModel;

	//read obj file from the file
	m_pModel = new GLMmodel;
	std::string errorString;
	if(!glmReadOBJ(fileName.toLatin1().data(), m_pModel, &errorString)) {
		QMessageBox::critical(this, tr("Read File Error!"), tr(errorString.c_str()));
		return;
	}

	glmUnitize(m_pModel);
	//glmFacetNormals(m_pModel);
	//glmVertexNormals(m_pModel, 90);

	m_vecColors.clear();
	GLMgroup *group = m_pModel->groups;
	while(group) {
		//Loop the find a unique color
		Qt::GlobalColor globalColor = (Qt::GlobalColor)((qrand() % 11) + 8);
		while(std::find(m_vecColors.begin(), m_vecColors.end(), globalColor) != m_vecColors.end())
			globalColor = (Qt::GlobalColor)((qrand() % 11) + 8);

		m_vecColors.push_back(QColor(globalColor));
		group = group->next;
	}

	initialize();
	update();
}

void QObjView::initialize()
{
	makeCurrent();

	glEnable(GL_DEPTH_TEST);
	// Default mode
	glPolygonMode(GL_FRONT,GL_FILL);
	glPolygonMode(GL_BACK,GL_FILL);
	glShadeModel(GLM_SMOOTH);
	glEnable(GL_NORMALIZE);

	// Lights, material properties
	GLfloat	ambientProperties[]  = {0.5f, 0.5f, 0.5f, 1.0f};
	GLfloat	diffuseProperties[]  = {0.8f, 0.2f, 0.2f, 1.0f};
	GLfloat	specularProperties[] = {0.0f, 0.8f, 0.2f, 1.0f};

	//glClearDepth( 1.0f );

	glLightfv( GL_LIGHT0, GL_AMBIENT, ambientProperties);
	glLightfv( GL_LIGHT0, GL_DIFFUSE, diffuseProperties);
	glLightfv( GL_LIGHT0, GL_SPECULAR, specularProperties);
	glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, 1.0f);

	// Default : lighting
	glEnable(GL_LIGHT0);
	glEnable(GL_LIGHTING);
	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(m_ClearColorRed,m_ClearColorGreen,m_ClearColorBlue,1.0f);
	initializePara();

	glClearColor(m_ClearColorRed,m_ClearColorGreen,m_ClearColorBlue,1.0f);
}

void QObjView::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	painter.setRenderHints(QPainter::HighQualityAntialiasing);

	//painter.beginNativePainting();
	saveGLState();
	if (m_pModel == NULL)	return;

	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	glViewport(0, 0, width(), height());

	/*
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0, (float)(width())/height(), 0.01f, 128.0f);
	glGetFloatv(GL_PROJECTION_MATRIX, m_projectArray);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	*/

	// scale / translation / rotation
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glScalef(m_xScaling,m_yScaling,m_zScaling);
	//glTranslatef(m_xTranslation,m_yTranslation,m_zTranslation);
	glRotatef(m_xRotation,1.0f,0.0f,0.0f);
	glRotatef(m_yRotation,0.0f,1.0f,0.0f);
	glGetFloatv(GL_MODELVIEW_MATRIX, m_modelViewArray);

	runZBuffer(&painter);
	restoreGLState();

	return;
}

void QObjView::runZBuffer(QPainter *painter)
{
	using namespace mytype;
	boost::shared_ptr<Zbuffer> buffer = buildZbuffer();

	int windowWidth = width();
	static const GLfloat BACKGROUNDVALUE = -100000.0;
	std::vector<GLfloat> zValueBuffer(windowWidth, BACKGROUNDVALUE);

	const PageTable &pageTable = buffer->getPageTable();
	const EdgeTable &edgeTable = buffer->getEdgeTable();

	ActivePageList activePageList;
	ActiveEdgeList activeEdgeList;

	m_nMinHeight = (m_nMinHeight > 0) ? m_nMinHeight : 0;

#ifdef MEASURETIME
	clock_t beforeRunScan = clock();
	clock_t timeFirst, timeSecond, timeThird, timeFouth;
#endif
	//Loop the scan line from the highest value to the lowest value
	for(int scanIndex = m_nMaxHeight; scanIndex >= m_nMinHeight; --scanIndex) {
#ifdef MEASURETIME
		timeFirst = clock();
		fMeasuretime << "-----------------------------------------------------" << endl;
#endif

		const PageList &pageList = pageTable[scanIndex];
		//If current scan line has a new page, put the page and its new edge into tables
		if(pageList.size() != 0) {
			//get the page of current scan line
			for(size_t pageIndex = 0; pageIndex < pageList.size(); ++pageIndex) {
				const Page& page = pageList[pageIndex];
				activePageList.push_back(page);

				//get the two edges of the active page
				const PageEdgeMap &edgeMap = edgeTable[scanIndex];
				PageEdgeMap::const_iterator ite = edgeMap.find(page.index);

				const EdgeList &edgeList = (*ite).second;
				assert(edgeList.size() == 2);

				Edge firstEdge = edgeList[0];
				Edge secondEdge = edgeList[1];
				if(firstEdge.upperXValue > secondEdge.upperXValue)
					std::swap(firstEdge, secondEdge);
				else if(firstEdge.upperXValue == secondEdge.upperXValue
						&& firstEdge.deltaX > secondEdge.deltaX)
					std::swap(firstEdge, secondEdge);

				//make the active edge
				ActiveEdge aEdge(firstEdge, secondEdge, page, scanIndex);

				//put active edge into table
				activeEdgeList.push_back(aEdge);
			}
		}

		//loop all the active pages
		ActivePageList::iterator aPageIte = activePageList.begin();
		while(aPageIte != activePageList.end()) {
			Page &aPage = *aPageIte;

			//the page is not active anymore
			if(aPage.lineCount < 0) {
				aPageIte = activePageList.erase(aPageIte);
				continue;
			}
			aPage.lineCount--;
			aPageIte++;
		}//end loop all the active pages

#ifdef MEASURETIME
		timeSecond = clock();
		fMeasuretime << "before active edges : " << timeSecond - timeFirst << "ms" << std::endl;
#endif

		//loop all the active edges
		ActiveEdgeList::iterator aEdgeIte = activeEdgeList.begin();
		while(aEdgeIte != activeEdgeList.end()) {
			ActiveEdge &aEdge = *aEdgeIte;

			//compare the zValue of all the activeEdge with the zValueBuffer
			float zValue = aEdge.zValue;

			//first make sure the x value is in the screen
			int leftXValue = aEdge.leftXValue;
			int rightXValue = aEdge.rightXValue;
			if(leftXValue > (windowWidth - 1) || rightXValue < 0) {
				++aEdgeIte;
				continue;
			}

			leftXValue = (leftXValue > -1) ? leftXValue : 0;
			rightXValue = (rightXValue > (windowWidth - 1)) ? (windowWidth - 1) : rightXValue;

			for(int xValue = leftXValue; xValue <= rightXValue; ++xValue) {
				if(zValue > zValueBuffer[xValue]) {
					zValueBuffer[xValue] = zValue;
				}
				zValue += aEdge.deltaZXValue;
			}

			//calculate para for next scan line
			--aEdge.leftLineCount;
			--aEdge.rightLineCount;

			if(aEdge.leftLineCount < 0 && aEdge.rightLineCount < 0) {
				aEdgeIte = activeEdgeList.erase(aEdgeIte);
				continue;
			}

			if(aEdge.leftLineCount < 0 || aEdge.rightLineCount < 0) {
				const PageEdgeMap &edgeMap = edgeTable[scanIndex];
				PageEdgeMap::const_iterator ite = edgeMap.find(aEdge.pageIndex);
				if(ite == edgeMap.end()) {
					aEdgeIte = activeEdgeList.erase(aEdgeIte);
					continue;
				}

				const EdgeList &edgeList = (*ite).second;
				assert(edgeList.size() == 1);

				Edge replaceEdge = edgeList[0];

				if(aEdge.leftLineCount < 0) {
					//assert(replaceEdge.upperXValue <= aEdge.rightXValue)
					aEdge.insertLeftEdge(replaceEdge, scanIndex);
				} else {
					//assert(replaceEdge.upperXValue >= aEdge.leftXValue);
					aEdge.insertRightEdge(replaceEdge, scanIndex);
				}
			}

			aEdge.leftXValue += aEdge.leftDeltaX;
			aEdge.rightXValue += aEdge.rightDeltaX;

			aEdge.zValue += aEdge.deltaZYValue + aEdge.leftDeltaX * aEdge.deltaZXValue;

			++aEdgeIte;
		}//end loop all the acitve edges

#ifdef MEASURETIME
		timeThird = clock();
		fMeasuretime << "Loop all active edges : " << timeThird - timeSecond << "ms" << std::endl;
#endif
		for(int xValue = 0; xValue < windowWidth; ++xValue) {
			float zValue = zValueBuffer[xValue];
			if(fabs(zValue - BACKGROUNDVALUE) > 1e-4) {
				int colorValue = (zValue + 0.866f) / 1.732f * 255;
				painter->setPen(QColor(colorValue, colorValue, colorValue));
				painter->drawPoint(xValue, scanIndex);
			}
		}

		//clean the buffer
		zValueBuffer.assign(windowWidth, BACKGROUNDVALUE);

#ifdef MEASURETIME
		timeFouth = clock();
		fMeasuretime << "Paint Time : " << timeFouth - timeThird << "ms" << endl;
		fMeasuretime << "Loop scan line " << scanIndex << " : " << timeFouth - timeFirst << "ms" << endl;
#endif
	}//End Loop for scan line

#ifdef MEASURETIME
		fMeasuretime << "Total Scanline time : " << clock() - beforeRunScan << "ms" << endl;
		fMeasuretime.flush();
#endif
}

void QObjView::mousePressEvent(QMouseEvent *event)
{
	m_ptLast = event->pos();
}

void QObjView::mouseMoveEvent( QMouseEvent *event )
{
	if(event->buttons() & Qt::LeftButton) {
		m_xRotation += 180 * GLfloat(event->y() - m_ptLast.y()) / width();
		m_yRotation += 180 * GLfloat(event->x() - m_ptLast.x()) / height();
		update();
	}
	m_ptLast = event->pos();
}

void QObjView::keyPressEvent(QKeyEvent *event)
{
	if(event->key() == Qt::Key_F5) {
		if(m_strOpenFileName.isEmpty())	return;
		openFile(m_strOpenFileName);
	}
}

void QObjView::initializePara()
{
	m_xRotation = 0.0f;
	m_yRotation = 0.0f;
	m_xTranslation = 0.0f;
	m_yTranslation = 0.0f;
	m_zTranslation = 0.0f;
	m_xScaling = 0.5f;
	m_yScaling = 0.5f;
	m_zScaling = 0.5f;

	m_nMaxHeight = -1;
	m_nMinHeight = 10000;
}

void QObjView::drawModel(GLMmodel *model, GLuint mode)
{
	static GLuint i;
	static GLMgroup* group;
	static GLMtriangle* triangle;
	static GLMmaterial* material;

	assert(model);
	assert(model->vertices);

	/* do a bit of warning */
	if (mode & GLM_FLAT && !model->facetnorms) {
		mode &= ~GLM_FLAT;
	}
	if (mode & GLM_SMOOTH && !model->normals) {
		mode &= ~GLM_SMOOTH;
	}
	if (mode & GLM_TEXTURE && !model->texcoords) {
		mode &= ~GLM_TEXTURE;
	}
	if (mode & GLM_FLAT && mode & GLM_SMOOTH) {
		mode &= ~GLM_FLAT;
	}
	if (mode & GLM_COLOR && !model->materials) {
		mode &= ~GLM_COLOR;
	}
	if (mode & GLM_MATERIAL && !model->materials) {
		mode &= ~GLM_MATERIAL;
	}
	if (mode & GLM_COLOR && mode & GLM_MATERIAL) {
		mode &= ~GLM_COLOR;
	}
	if (mode & GLM_COLOR)
		glEnable(GL_COLOR_MATERIAL);
	else if (mode & GLM_MATERIAL)
		glDisable(GL_COLOR_MATERIAL);

	/* perhaps this loop should be unrolled into material, color, flat,
	smooth, etc. loops?  since most cpu's have good branch prediction
	schemes (and these branches will always go one way), probably
	wouldn't gain too much?  */

	group = model->groups;
	while (group) {
		if (mode & GLM_MATERIAL) {
			material = &model->materials[group->material];
			glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, material->ambient);
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, material->diffuse);
			glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, material->specular);
			glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, material->shininess);
		}

		if (mode & GLM_COLOR) {
			material = &model->materials[group->material];
			glColor3fv(material->diffuse);
		}

		glBegin(GL_TRIANGLES);
		for (i = 0; i < group->numtriangles; i++) {
			triangle = &(model->triangles[(group->triangles[i])]);

			if (mode & GLM_FLAT)
				glNormal3fv(&model->facetnorms[3 * triangle->findex]);

			if (mode & GLM_SMOOTH)
				glNormal3fv(&model->normals[3 * triangle->nindices[0]]);
			if (mode & GLM_TEXTURE)
				glTexCoord2fv(&model->texcoords[2 * triangle->tindices[0]]);
			glVertex3fv(&model->vertices[3 * triangle->vindices[0]]);

			if (mode & GLM_SMOOTH)
				glNormal3fv(&model->normals[3 * triangle->nindices[1]]);
			if (mode & GLM_TEXTURE)
				glTexCoord2fv(&model->texcoords[2 * triangle->tindices[1]]);
			glVertex3fv(&model->vertices[3 * triangle->vindices[1]]);

			if (mode & GLM_SMOOTH)
				glNormal3fv(&model->normals[3 * triangle->nindices[2]]);
			if (mode & GLM_TEXTURE)
				glTexCoord2fv(&model->texcoords[2 * triangle->tindices[2]]);
			glVertex3fv(&model->vertices[3 * triangle->vindices[2]]);
		}
		glEnd();

		group = group->next;
	}
}

void QObjView::saveGLState()
{
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
}

void QObjView::restoreGLState()
{
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glPopAttrib();
}

boost::shared_ptr<mytype::Zbuffer> QObjView::buildZbuffer()
{
	mytype::Model model;

#ifdef MEASURETIME
	clock_t timeFirst = clock();
#endif

	int w = width();
	int h = height();

	//the number of current cpu cores
	int numCores = omp_get_num_procs();

	GLMgroup *group = m_pModel->groups;
	int objCount = 0;

	//Loop all the groups, each group is saved in a mytype::Object structure
	while(group) {
		model.push_back(mytype::Object());

		int number = group->numtriangles;
		int perCoreNumber = ceil((double)(number) / numCores);
		omp_lock_t lock;
		omp_init_lock(&lock);

		/*
		*Loop all the triangles
		*make full use of all the cores of cpu
		*each core run a range of triangles rotation
		*/
#pragma omp parallel for num_threads(numCores)
		for(int coreId = 0; coreId < numCores; coreId++) {
			int startNumber, endNumber;
			startNumber = coreId * perCoreNumber;

			//if this is not the last core
			if(coreId < numCores - 1)
				endNumber = startNumber + perCoreNumber;
			else
				endNumber = number;

			for(int i = startNumber; i < endNumber; ++i)
			{
				mytype::Polygon polygon;

				//Loop the 3 point in a triangle
				for(int k = 0; k < 3; ++k)
				{
					//Get the triangle
					GLMtriangle* triangle = &(m_pModel->triangles[(group->triangles[i])]);

					//Get the 3-D point
					GLfloat * pointArray = &m_pModel->vertices[3 * triangle->vindices[k]];
					std::vector<GLfloat> vec(pointArray, pointArray + 3);

					//Multiple modelview and projection matrix, and convert the the screen coordinate
					vecLMultiplyRotateMat(vec, m_modelViewArray);
					//vecLMultiplyMat(vec, matProject);
					vec[0] = (vec[0] + 1.0f) / 2.0f * w;
					vec[1] = h * (1 - (vec[1] + 1.0f) / 2.0f);

					//Save the result
					polygon.push_back(mytype::Point3D(vec[0], vec[1], vec[2]));

					if(vec[1] > m_nMaxHeight)
						m_nMaxHeight = vec[1];
					if(vec[1] < m_nMinHeight)
						m_nMinHeight = vec[1];
				}
				omp_set_lock(&lock);
				model[objCount].push_back(polygon);
				omp_unset_lock(&lock);
			}
		}

		//Get next object
		++objCount;
		group = group->next;

		omp_destroy_lock(&lock);
	}

#ifdef MEASURETIME
	clock_t timeSecond = clock();
	fMeasuretime << "Build ZBuffer time : " << (int)(timeSecond - timeFirst) << "ms" << std::endl;
	fMeasuretime.flush();
#endif

	return boost::shared_ptr<mytype::Zbuffer>(new mytype::Zbuffer(model, m_nMaxHeight + 1));
}

void QObjView::printZBufferData(mytype::Zbuffer &buffer, const char* fileName)
{
	using namespace mytype;
	if(fileName == NULL) {
		fileName = QString(QFileInfo(m_strOpenFileName).fileName() + ".txt").toStdString().c_str();
	}

	fstream fout(fileName, fstream::out);

	fout<<"ZBuffer Data" <<endl;

	const PageTable &table = buffer.getPageTable();

	fout<<"PageTable Info : "<<endl;
	for(size_t i = 0; i < table.size(); ++i) {
		for(size_t j = 0; j < table.at(i).size(); ++j) {
			Page page = table[i][j];
			boost::format fmt("ObjIndx %7% MaxY %1% : (%2%,%3%,%4%), index is %5%, lineCount is %6%");
			fout << fmt % i % page.a % page.b % page.c % page.index % page.lineCount % page.objIndex <<endl;
		}
	}

	fout<<endl<<"EdgeTable Info : "<<endl;

	const EdgeTable &edgeTable = buffer.getEdgeTable();
	for(size_t i = 0; i < edgeTable.size(); ++i) {
		const PageEdgeMap &pageEdgeMap = edgeTable.at(i);
		if(pageEdgeMap.size() != 0)
			fout<<"****************** Maxy is "<<i <<"***************************"<< endl;
		for(PageEdgeMap::const_iterator ite = pageEdgeMap.begin();
				ite != pageEdgeMap.end();
				++ite) {
			fout<<"----------------------------" << "Page Index is " << (*ite).first << "------------------------" << endl;

			const EdgeList &edgeList = (*ite).second;
			for(size_t j = 0; j < edgeList.size(); ++j) {
				const Edge &edge = edgeList.at(j);
				boost::format fmt("No %2% : page Index %3%, lineCount is %4%, dx is %5% upperX is %6%");
				fout << fmt % i % j % edge.pageIndex % edge.lineCount % edge.deltaX % edge.upperXValue <<endl;
			}
		}
		if(pageEdgeMap.size() != 0)
			fout<<endl;
	}
}

void QObjView::dropEvent(QDropEvent *event)
{
	openFile(m_strOpenFileName);
	event->accept();
}

void QObjView::dragEnterEvent(QDragEnterEvent *event)
{
	const QMimeData *mimeData = event->mimeData();
	QStringList list = mimeData->formats();

	for(int i = 0; i < list.size(); ++i) {
		if(list.at(i) == "text/uri-list") {
			QList<QUrl> urllist = mimeData->urls();
			QString fileName = urllist.at(0).toString();
			if(QFileInfo(fileName).suffix() == "obj") {
				event->acceptProposedAction();
				m_strOpenFileName = fileName.remove(0, 8);
				return;
			}
		}
	}
}

void QObjView::dragMoveEvent(QDragMoveEvent *event)
{
	event->acceptProposedAction();
}
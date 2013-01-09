#include "datastructure.h"
#include <assert.h>
#include <cmath>
#include <ctime>
#include <iostream>

using namespace mytype;

void Zbuffer::buildTables(const Model &model)
{
    m_pageTable.resize(m_nMaxHeight);
    m_edgeTable.resize(m_nMaxHeight);

    int pageTotalCount = 0;
    for(size_t objIndex = 0; objIndex < model.size(); ++objIndex) {
        const Object& object = model[objIndex];
#pragma omp parallel for
        for(size_t pageIndex = 0; pageIndex < object.size(); ++pageIndex) {
            const Polygon& polygon = object[pageIndex];
            if(insertIntoPageTable(polygon, pageTotalCount, objIndex))
                insertIntoEdgeTable(polygon, pageTotalCount);
            ++pageTotalCount;
        }
    }
}

bool Zbuffer::insertIntoPageTable(const Polygon& polygon, int pageIndex, int objIndex)
{
    //Not Consider the polygon is not on the same plain
    if(polygon.size() < 3) return false;

    //get the three first pt to calculate crossproduct
    Vector3D vec1(polygon[1], polygon[0]);
    Vector3D vec2(polygon[1], polygon[2]);

    Vector3D vecCross = crossValue(vec1, vec2);

    //calculate a, b, c, d parameter of the plain
    Page page;
    page.a = vecCross.x;
    page.b = vecCross.y;
    page.c = vecCross.z;
    if(abs(page.c) < 1e-5) return false;

    page.d = -(page.a * polygon[0].x + page.b * polygon[0].y + page.c * polygon[0].z);
    page.index = pageIndex;

    //2D information, convert to int is convenient
    int miny, maxy;
    miny = maxy = (int)(polygon[0].y);

    for(size_t i = 1; i < polygon.size(); ++i) {
        int y = (int)(polygon[i].y);
        if(y > maxy)	maxy = y;
        if(y < miny)	miny = y;
    }
    page.lineCount = maxy - miny;
	if(page.lineCount <= 0) return false;

    page.objIndex = objIndex;

    //push the page into the table on the index of maxy
    assert(maxy > -1);
    m_pageTable[maxy].push_back(page);

    return true;
}

void Zbuffer::insertIntoEdgeTable(const Polygon& polygon, int pageIndex)
{
    //copy the two dimension data into vec
    typedef Point2D<int> IntPoint2D;

    std::vector<IntPoint2D> vec;
    for(size_t i = 0; i < polygon.size(); ++i)
        vec.push_back(IntPoint2D(polygon[i].x, polygon[i].y));

    //record the origin number of the polygon
    size_t pointNum = vec.size();

    vec.push_back(vec[0]);

    for(size_t i = 0; i < pointNum; ++i) {
        const IntPoint2D &pt1 = vec[i];
        const IntPoint2D &pt2 = vec[i + 1];

        float dx, dy;
        dx = pt1.x - pt2.x;
        dy = pt1.y - pt2.y;

        Edge edge;
        edge.pageIndex = pageIndex;
        edge.deltaX = -dx/dy;

        int maxY;

        if(pt1.y == pt2.y)	continue;
        if(pt1.y > pt2.y) {
            edge.lineCount = pt1.y - pt2.y;
            edge.upperXValue = pt1.x;
            maxY = pt1.y;
        } else {
            edge.lineCount = pt2.y - pt1.y;
            edge.upperXValue = pt2.x;
            maxY = pt2.y;
        }

        PageEdgeMap &pageEdgeMap = m_edgeTable[maxY];
        pageEdgeMap[edge.pageIndex].push_back(edge);
    }
}

void Zbuffer::initialize()
{
    m_nMinY = m_nMaxY = -1;
    srand(NULL);
}
// TestProject.cpp : Defines the entry point for the console application.
//

#include "../QtObjLoader/datastructure.h"
#include <iostream>
#include <boost/format.hpp>
using std::cout;
using std::endl;
using namespace mytype;

int main(int argc, char *argv[])
{
    Object test1;
    Polygon poly1, poly2;
    poly1.push_back(Point3D(0, 0, 0));
    poly1.push_back(Point3D(4, 10, 0));
    poly1.push_back(Point3D(4, 15, 0));
    poly1.push_back(Point3D(0, 20, 0));
    poly1.push_back(Point3D(-20, 20, 0));
    poly1.push_back(Point3D(-20, 10, 0));

    poly2.push_back(Point3D(4, 10, 0));
    poly2.push_back(Point3D(0, 0, 0));
    poly2.push_back(Point3D(4, 13, 0));

    Polygon poly3; //in a line
    poly3.push_back(Point3D(0, 0, 0));
    poly3.push_back(Point3D(0, 1, 0));
    poly3.push_back(Point3D(0, 2, 0));

    Polygon poly4; // projection is a line
    poly4.push_back(Point3D(1, 0, 0));
    poly4.push_back(Point3D(1, 1, 1));
    poly4.push_back(Point3D(1, 0, 1));

    Polygon poly5;
    poly5.push_back(Point3D(6, 7, 0));
    poly5.push_back(Point3D(8, 9, 0));
    poly5.push_back(Point3D(11, 1, 0));

    test1.push_back(poly1);
    test1.push_back(poly2);
    test1.push_back(poly3);
    test1.push_back(poly4);

    Object test2;
    test2.push_back(poly5);

    Model model;
    model.push_back(test1);
    model.push_back(test2);

    Zbuffer buffer(model, 500);
    const PageTable &table = buffer.getPageTable();

    for(size_t i = 0; i < table.size(); ++i) {
        for(size_t j = 0; j < table.at(i).size(); ++j) {
            Page page = table[i][j];
            boost::format fmt("ObjIndx %7% MaxY %1% : (%2%,%3%,%4%), index is %5%, lineCount is %6%");
            cout << fmt % i % page.a % page.b % page.c % page.index % page.lineCount % page.objIndex <<endl;
        }
    }

    const EdgeTable &edgeTable = buffer.getEdgeTable();
    for(size_t i = 0; i < edgeTable.size(); ++i) {
        const PageEdgeMap &pageEdgeMap = edgeTable.at(i);
        if(pageEdgeMap.size() != 0)
            cout<<"Maxy is "<<i <<"--------------------"<< endl;
        for(PageEdgeMap::const_iterator ite = pageEdgeMap.begin();
                ite != pageEdgeMap.end();
                ++ite) {
            cout<<"----------------------------" << "Page Index is " << (*ite).first << "------------------------" << endl;

            const EdgeList &edgeList = (*ite).second;
            for(size_t j = 0; j < edgeList.size(); ++j) {
                const Edge &edge = edgeList.at(j);
                boost::format fmt("No %2% : page Index %3%, lineCount is %4%, dx is %5% upperX is %6%");
                cout << fmt % i % j % edge.pageIndex % edge.lineCount % edge.deltaX % edge.upperXValue <<endl;
            }
        }
    }

    return 0;
}


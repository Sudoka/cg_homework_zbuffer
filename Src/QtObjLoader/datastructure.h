#ifndef DATASTRUCTURE_H
#define DATASTRUCTURE_H
#include <vector>
#include <list>
#include <map>

/*************************************************************************
 *Data Structure For Zbuffer Algorithm
 *************************************************************************
 */

namespace mytype
{
	template<typename T>
	class Point2D
	{
	public:
		Point2D(T _x, T _y) : x(_x), y(_y) {}

	public:
		T x;
		T y;
	};

	class Vector2D
	{
	public:
		Vector2D(int _x, int _y) : x(_x), y(_y) {}
		Vector2D(const Point2D<int> &src, const Point2D<int> &dst)
			: x(dst.x - src.x), y(dst.y - src.y)
		{}

	public:
		int x, y;
	};

	class Point3D
	{
	public:
		Point3D(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

	public:
		float x, y, z;
	};

	class Vector3D
	{
	public:
		Vector3D(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
		Vector3D(const Point3D &src, const Point3D &dst)
			: x(dst.x - src.x), y(dst.y - src.y), z(dst.z - src.z)
		{}

	public:
		float x, y, z;
	};

	inline Vector3D crossValue(const Vector3D &u, const Vector3D &v)
	{
		return Vector3D(
			u.y * v.z - u.z * v.y,
			u.z * v.x - u.x * v.z,
			u.x * v.y - u.y * v.x
			);
	}

	inline int operator* (const Vector2D &a, const Vector2D &b)
	{
		return (b.y * a.x) - (b.x * a.y);
	}

	struct Page
	{
		float a, b, c, d;
		int index;
		int lineCount;
		int objIndex;
	};

	typedef std::vector<Page> PageList;

	typedef std::vector<PageList> PageTable;

	typedef std::list<Page> ActivePageList;

	struct Edge
	{
		float upperXValue;
		float deltaX;
		int lineCount;
		int pageIndex;
	};

	//Active Edge
	struct ActiveEdge
	{
		ActiveEdge() {}
		ActiveEdge(const Edge &firstEdge, const Edge &secondEdge, const Page &page, int scanIndex)
		{
			leftXValue = firstEdge.upperXValue; rightXValue = secondEdge.upperXValue;
			leftDeltaX = firstEdge.deltaX; rightDeltaX = secondEdge.deltaX;
			leftLineCount = firstEdge.lineCount; rightLineCount = secondEdge.lineCount;
			a = page.a; b = page.b; c = page.c; d = page.d;
			zValue = -(d + a*leftXValue + b*scanIndex) / (c);
			deltaZXValue = (-a) / c;
			deltaZYValue = b / c;
			pageIndex = page.index;
			objIndex = page.objIndex;
		}

		//insert firstEdge into left part of the active edge
		void insertLeftEdge(const Edge &firstEdge, int scanIndex)
		{
			leftXValue = firstEdge.upperXValue; leftDeltaX = firstEdge.deltaX;
			leftLineCount = firstEdge.lineCount;
			zValue = -(d + a*leftXValue + b*scanIndex) / (c);
		}
		//insert secondEdge into right part of the active edge
		void insertRightEdge(const Edge &secondEdge, int scanIndex)
		{
			rightXValue = secondEdge.upperXValue; rightDeltaX = secondEdge.deltaX;
			rightLineCount = secondEdge.lineCount;
		}

		float leftXValue, rightXValue;
		float leftDeltaX, rightDeltaX;
		int leftLineCount, rightLineCount;
		float zValue;
		float deltaZXValue;
		float deltaZYValue;
		int pageIndex;
		int objIndex;
		float a, b, c, d;
	};

	typedef std::vector<Edge> EdgeList;
	typedef std::map<int, EdgeList> PageEdgeMap;
	typedef std::vector<PageEdgeMap> EdgeTable;
	typedef std::list<ActiveEdge> ActiveEdgeList;

	//a model contains several objects, an object contains several polygons
	typedef std::vector<Point3D> Polygon;
	typedef std::vector<Polygon> Object;
	typedef std::vector<Object> Model;

	class Zbuffer
	{
	public:
		Zbuffer(const Model &model, int nWindowHeight)
		{
			m_nMaxHeight = nWindowHeight;
			initialize();
			buildTables(model);
		}
		const PageTable& getPageTable() const { return m_pageTable; }
		const EdgeTable& getEdgeTable() const { return m_edgeTable; }

	private:
		//insert a page into page table
		bool insertIntoPageTable(const Polygon& polygon, int pageIndex, int objIndex);

		//insert the edges of a page into edge table
		void insertIntoEdgeTable(const Polygon& polygon, int pageIndex);

		//build all the tables from the model
		void buildTables(const Model &model);
		void initialize();

	private:
		PageTable m_pageTable;
		EdgeTable m_edgeTable;
		int m_nMinY, m_nMaxY;
		int m_nMaxHeight;
	};
}

#endif
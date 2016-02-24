/*******************************************************************************************************
 ReadFramework is the basis for modules developed at CVL/TU Wien for the EU project READ. 
  
 Copyright (C) 2016 Markus Diem <diem@caa.tuwien.ac.at>
 Copyright (C) 2016 Stefan Fiel <fiel@caa.tuwien.ac.at>
 Copyright (C) 2016 Florian Kleber <kleber@caa.tuwien.ac.at>

 This file is part of ReadFramework.

 ReadFramework is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 ReadFramework is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 The READ project  has  received  funding  from  the European  Union’s  Horizon  2020  
 research  and innovation programme under grant agreement No 674943
 
 related links:
 [1] http://www.caa.tuwien.ac.at/cvl/
 [2] https://transkribus.eu/Transkribus/
 [3] https://github.com/TUWien/
 [4] http://nomacs.org
 *******************************************************************************************************/

#include "Shapes.h"
#include "Utils.h"
#include "Algorithms.h"

#pragma warning(push, 0)	// no warnings from includes
// Qt Includes
#pragma warning(pop)

namespace rdf {

Polygon::Polygon(const QPolygon& polygon) {
	mPoly = polygon;
}
bool Polygon::isEmpty() const {
	return mPoly.isEmpty();
}
void Polygon::read(const QString & pointList) {
	mPoly = Converter::instance().stringToPoly(pointList);
}

QString Polygon::write() const {
	return Converter::instance().polyToString(mPoly);
}

int Polygon::size() const {
	return mPoly.size();
}

void Polygon::setPolygon(const QPolygon & polygon) {
	mPoly = polygon;
}

QPolygon Polygon::polygon() const {
	return mPoly;
}

QPolygon Polygon::closedPolygon() const {
	
	QPolygon closed = mPoly;
	if (!mPoly.isEmpty())
		closed.append(mPoly.first());

	return closed;
}

// BaseLine --------------------------------------------------------------------
BaseLine::BaseLine(const QPolygon & baseLine) {
	mBaseLine = baseLine;
}

bool BaseLine::isEmpty() const {
	return mBaseLine.isEmpty();
}

void BaseLine::setPolygon(QPolygon & baseLine) {
	mBaseLine = baseLine;
}

QPolygon BaseLine::polygon() const {
	return mBaseLine;
}

void BaseLine::read(const QString & pointList) {
	mBaseLine = Converter::instance().stringToPoly(pointList);
}

QString BaseLine::write() const {
	return Converter::instance().polyToString(mBaseLine);
}

QPoint BaseLine::startPoint() const {
	
	if (!mBaseLine.isEmpty())
		return mBaseLine.first();

	return QPoint();
}

QPoint BaseLine::endPoint() const {

	if (!mBaseLine.isEmpty())
		return mBaseLine.last();

	return QPoint();
}

// Line ------------------------------------------------------------------------------------------------------

Line::Line(const QLine& line, float thickness) {
	mLine = line;
	mThickness = thickness;
}

bool Line::isEmpty() const {
	return (mLine.isNull());
}

void Line::setLine(const QLine& line, float thickness) {
	mLine = line;
	mThickness = thickness;
}

QLine Line::line() const {
	return mLine;
}

float Line::thickness() const {
	return mThickness;
}

float Line::length() const {
	QPoint diff = mLine.p2() - mLine.p1();

	return (float)sqrt(diff.x()*diff.x() + diff.y()*diff.y());

}

double Line::angle() const {

	QPoint diff = mLine.p2() - mLine.p1();

	return atan2(diff.y(), diff.x());

}

QPoint Line::startPoint() const {
	return mLine.p1();
}

QPoint Line::endPoint() const {
	return mLine.p2();
}


float Line::minDistance(const Line& l) const {

	float dist1 = rdf::Algorithms::instance().euclideanDistance(mLine.p1(), l.line().p1());
	float dist2 = rdf::Algorithms::instance().euclideanDistance(mLine.p1(), l.line().p2());
	dist1 = (dist1 < dist2) ? dist1 : dist2;
	dist2 = rdf::Algorithms::instance().euclideanDistance(mLine.p2(), l.line().p1());
	dist1 = (dist1 < dist2) ? dist1 : dist2;
	dist2 = rdf::Algorithms::instance().euclideanDistance(mLine.p2(), l.line().p2());
	dist1 = (dist1 < dist2) ? dist1 : dist2;

	return dist1;
}

Line Line::merge(const Line& l) const {

	cv::Mat dist = cv::Mat(1, 4, CV_32FC1);
	float* ptr = dist.ptr<float>();

	ptr[0] = rdf::Algorithms::instance().euclideanDistance(mLine.p1(), l.line().p1());
	ptr[1] = rdf::Algorithms::instance().euclideanDistance(mLine.p1(), l.line().p2());
	ptr[2] = rdf::Algorithms::instance().euclideanDistance(mLine.p2(), l.line().p1());
	ptr[3] = rdf::Algorithms::instance().euclideanDistance(mLine.p2(), l.line().p2());

	cv::Point maxIdxP;
	minMaxLoc(dist, 0, 0, 0, &maxIdxP);
	int maxIdx = maxIdxP.x;

	float thickness = mThickness < l.thickness() ? mThickness : l.thickness();
	Line mergedLine;

	switch (maxIdx) {
	case 0: mergedLine = Line(QLine(mLine.p1(), l.line().p1()), thickness);	break;
	case 1: mergedLine = Line(QLine(mLine.p1(), l.line().p2()), thickness);	break;
	case 2: mergedLine = Line(QLine(mLine.p2(), l.line().p1()), thickness);	break;
	case 3: mergedLine = Line(QLine(mLine.p2(), l.line().p2()), thickness);	break;
	}

	return mergedLine;
}

Line Line::gapLine(const Line& l) const {

	cv::Mat dist = cv::Mat(1, 4, CV_32FC1);
	float* ptr = dist.ptr<float>();

	ptr[0] = rdf::Algorithms::instance().euclideanDistance(mLine.p1(), l.line().p1());
	ptr[1] = rdf::Algorithms::instance().euclideanDistance(mLine.p1(), l.line().p2());
	ptr[2] = rdf::Algorithms::instance().euclideanDistance(mLine.p2(), l.line().p1());
	ptr[3] = rdf::Algorithms::instance().euclideanDistance(mLine.p2(), l.line().p2());

	cv::Point minIdxP;
	minMaxLoc(dist, 0, 0, &minIdxP);
	int minIdx = minIdxP.x;

	float thickness = mThickness < l.thickness() ? mThickness : l.thickness();
	Line gapLine;

	switch (minIdx) {
	case 0: gapLine = Line(QLine(mLine.p1(), l.line().p1()), thickness);	break;
	case 1: gapLine = Line(QLine(mLine.p1(), l.line().p2()), thickness);	break;
	case 2: gapLine = Line(QLine(mLine.p2(), l.line().p1()), thickness);	break;
	case 3: gapLine = Line(QLine(mLine.p2(), l.line().p2()), thickness);	break;
	}

	return gapLine;
}

float Line::diffAngle(const Line& l) const {

	float angleLine, angleL;

	//angleLine
	angleLine = Algorithms::instance().normAngleRad((float)angle(), 0.0f, (float)CV_PI);
	angleLine = angleLine > (float)CV_PI*0.5f ? (float)CV_PI - angleLine : angleLine;
	angleL = Algorithms::instance().normAngleRad((float)l.angle(), 0.0f, (float)CV_PI);
	angleL = angleL > (float)CV_PI*0.5f ? (float)CV_PI - angleL : angleL;
	
	return fabs(angleLine - angleL);
}


}
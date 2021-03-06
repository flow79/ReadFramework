/*******************************************************************************************************
ReadFramework is the basis for modules developed at CVL/TU Wien for the EU project READ.

Copyright (C) 2016 Markus Diem <diem@cvl.tuwien.ac.at>
Copyright (C) 2016 Stefan Fiel <fiel@cvl.tuwien.ac.at>
Copyright (C) 2016 Florian Kleber <kleber@cvl.tuwien.ac.at>

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

The READ project  has  received  funding  from  the European  Union�s  Horizon  2020
research  and innovation programme under grant agreement No 674943

related links:
[1] https://cvl.tuwien.ac.at/
[2] https://transkribus.eu/Transkribus/
[3] https://github.com/TUWien/
[4] https://nomacs.org
*******************************************************************************************************/

#pragma once

#include "BaseModule.h"
#include "Shapes.h"

#pragma warning(push, 0)	// no warnings from includes
// Qt Includes
#include <QVector>
#pragma warning(pop)

#pragma warning (disable: 4251)	// inlined Qt functions in dll interface

#ifndef DllCoreExport
#ifdef DLL_CORE_EXPORT
#define DllCoreExport Q_DECL_EXPORT
#else
#define DllCoreExport Q_DECL_IMPORT
#endif
#endif

// Qt defines

namespace rdf {


class DllCoreExport GradientVectorConfig : public ModuleConfig {

	public:
		GradientVectorConfig();

		double sigma() const;
		void setSigma(double s);

		bool normGrad() const;
		void setNormGrad(bool n);

		bool perpendAngle() const;
		void setPerpendAngle(bool p);

		QString toString() const override;

	private:
		void load(const QSettings& settings) override;
		void save(QSettings& settings) const override;

		double mSigma = 1.75;	//filter parameter: maximal difference of line orientation compared to the result of the Rotation module (default: 5 deg)
		bool mNormGrad = true;
		bool mPerpendAngle = false;	//if you want to get perpendicular angles compared to the gradient orientation
};

class DllCoreExport GradientVector : public Module {


public:
	GradientVector(const cv::Mat& img, const cv::Mat& mask = cv::Mat());

	cv::Mat debugGaussImg();
	cv::Mat magImg();
	cv::Mat radImg();
	cv::Mat mask();

	void setAnchor(cv::Point a);
	cv::Point anchor() const;

	void setDxKernel(const cv::Mat& m);
	void setDyKernel(const cv::Mat& m);

	double minVal() const;
	double maxVal() const;


	bool isEmpty() const override;
	virtual bool compute() override;

	QSharedPointer<GradientVectorConfig> config() const;

	virtual QString toString() const override;

protected:
	bool checkInput() const override;
	void computeGradients();
	void computeGradMag(bool norm);
	void computeGradAngle();


private:

	double mMinVal;
	double mMaxVal;

	cv::Mat mSrcImg;			// input image
	cv::Mat mMask;			// input mask
	cv::Mat mGaussImg;		// gaussian image 
	cv::Mat mDxImg;			// x derivative image
	cv::Mat mDyImg;			// y derivative image
	cv::Mat mMagImg;			// gradient magnitude image
	cv::Mat mRadImg;			// orientation image (in radians)

	cv::Point mAnchor = cv::Point(-1, -1);
	cv::Mat mDxKernel;
	cv::Mat mDyKernel;

};

}

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

 The READ project  has  received  funding  from  the European  Union’s  Horizon  2020  
 research  and innovation programme under grant agreement No 674943
 
 related links:
 [1] https://cvl.tuwien.ac.at/
 [2] https://transkribus.eu/Transkribus/
 [3] https://github.com/TUWien/
 [4] https://nomacs.org
 *******************************************************************************************************/

#pragma once

#include "BaseModule.h"
#include "PixelSet.h"

#pragma warning(push, 0)	// no warnings from includes

#pragma warning(pop)

#ifndef DllCoreExport
#ifdef DLL_CORE_EXPORT
#define DllCoreExport Q_DECL_EXPORT
#else
#define DllCoreExport Q_DECL_IMPORT
#endif
#endif

// Qt defines

namespace rdf {

class DllCoreExport SuperPixelFeatureConfig : public ModuleConfig {

public:
	SuperPixelFeatureConfig();

	virtual QString toString() const override;

protected:

	//void load(const QSettings& settings) override;
	//void save(QSettings& settings) const override;
};

class DllCoreExport SuperPixelFeature : public Module {

public:
	SuperPixelFeature(const cv::Mat& img, const PixelSet& set);

	bool isEmpty() const override;
	bool compute() override;
	QSharedPointer<SuperPixelFeatureConfig> config() const;

	cv::Mat draw(const cv::Mat& img) const;
	QString toString() const override;

	cv::Mat features() const;
	PixelSet pixelSet() const;

private:
	cv::Mat mImg;
	PixelSet mSet;

	// output
	cv::Mat mDescriptors;

	bool checkInput() const override;
	void syncSuperPixels(const std::vector<cv::KeyPoint>& keyPointsOld, const std::vector<cv::KeyPoint>& keyPointsNew);
};

class DllCoreExport SuperPixelClassifierConfig : public ModuleConfig {

public:
	SuperPixelClassifierConfig();

	virtual QString toString() const override;
	
	void setClassifierPath(const QString& path);
	QString classifierPath() const;

protected:

	QString mClassifierPath;

	void load(const QSettings& settings) override;
	void save(QSettings& settings) const override;
};

class DllCoreExport SuperPixelClassifier : public Module {

public:
	SuperPixelClassifier(const cv::Mat& img, const PixelSet& set);

	bool isEmpty() const override;
	bool compute() override;
	QSharedPointer<SuperPixelClassifierConfig> config() const;

	cv::Mat draw(const cv::Mat& img) const;
	QString toString() const override;

	void setModel(const QSharedPointer<SuperPixelModel>& model);
	PixelSet pixelSet() const;

private:
	cv::Mat mImg;
	PixelSet mSet;
	QSharedPointer<SuperPixelModel> mModel;

	bool checkInput() const override;
};



}

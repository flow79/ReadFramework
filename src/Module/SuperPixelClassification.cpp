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

#include "SuperPixelClassification.h"

#include "Utils.h"
#include "Image.h"
#include "Drawer.h"
#include "ImageProcessor.h"

#include "Elements.h"
#include "ElementsHelper.h"

#pragma warning(push, 0)	// no warnings from includes

#include <QDebug>
#include <QPainter>
#include <QJsonObject>

#include <opencv2/features2d.hpp>
#include <opencv2/ml.hpp>


#include "GCGraph.hpp"
#include "graphcut/GCoptimization.h"

#pragma warning(pop)

namespace rdf {


// SuperPixelClassifierConfig --------------------------------------------------------------------
SuperPixelClassifierConfig::SuperPixelClassifierConfig() : ModuleConfig("Super Pixel Classification") {
}

QString SuperPixelClassifierConfig::toString() const {
	return ModuleConfig::toString();
}

void SuperPixelClassifierConfig::setClassifierPath(const QString & path) {
	mClassifierPath = path;
}

QString SuperPixelClassifierConfig::classifierPath() const {
	return mClassifierPath;
}

void SuperPixelClassifierConfig::load(const QSettings & settings) {
	mClassifierPath = settings.value("classifierPath", mClassifierPath).toString();
}

void SuperPixelClassifierConfig::save(QSettings & settings) const {

	settings.setValue("classifierPath", mClassifierPath);
}

// SuperPixelClassifier --------------------------------------------------------------------
SuperPixelClassifier::SuperPixelClassifier(const cv::Mat& img, const PixelSet& set) {

	mImg = img;
	mSet = set;
	mConfig = QSharedPointer<SuperPixelClassifierConfig>::create();
	mConfig->loadSettings();
}

bool SuperPixelClassifier::isEmpty() const {
	return mImg.empty();
}

bool SuperPixelClassifier::compute() {

	if (!checkInput())
		return false;

	Timer dt;

	// compute features
	SuperPixelFeature spf(mImg, mSet);

	if (!spf.compute()) {
		mWarning << "SuperPixel features could not be computed";
		return false;
	}

	// sync the set
	cv::Mat features = spf.features();
	mSet = spf.pixelSet();

	if (features.rows != mSet.size()) {
		qCritical() << "PixelSet is out-of-sync with # of features, aborting classification";
		return false;
	}

	// classify
	QVector<PixelLabel> labels = mModel->classify(features);
	QVector<QSharedPointer<Pixel> > pixels = mSet.pixels();
	assert(labels.size() == mSet.size());

	for (int idx = 0; idx < mSet.size(); idx++) {

		// don't just assign for we might loose the gt label
		QSharedPointer<PixelLabel> label = pixels[idx]->label();
		label->setVotes(labels[idx].votes());
		label->setLabel(labels[idx].predicted());
	}

	mInfo << mSet.size() << "pixels classified in" << dt;

	return true;
}

QSharedPointer<SuperPixelClassifierConfig> SuperPixelClassifier::config() const {
	return qSharedPointerDynamicCast<SuperPixelClassifierConfig>(mConfig);
}

cv::Mat SuperPixelClassifier::draw(const cv::Mat& img) const {

	if (!checkInput())
		return cv::Mat();

	// draw mser blobs
	Timer dtf;
	QImage qImg = Image::mat2QImage(img, true);

	QPainter p(&qImg);
	mSet.draw(p, PixelSet::draw_pixels);
	
	// draw legend
	mModel->manager().draw(p);

	return Image::qImage2Mat(qImg);
}

QString SuperPixelClassifier::toString() const {
	return Module::toString();
}

void SuperPixelClassifier::setModel(const QSharedPointer<SuperPixelModel>& model) {
	mModel = model;
}

PixelSet SuperPixelClassifier::pixelSet() const {
	return mSet;
}

bool SuperPixelClassifier::checkInput() const {

	if (mModel && mModel->model() && !mModel->model()->isTrained())
		mCritical << "I cannot classify, since the model is not trained";

	return mModel && mModel->model() && mModel->model()->isTrained() && !isEmpty();
}

// SuperPixelFeatureConfig --------------------------------------------------------------------
SuperPixelFeatureConfig::SuperPixelFeatureConfig() :  ModuleConfig("Super Pixel Feature") {
}

QString SuperPixelFeatureConfig::toString() const{
	return ModuleConfig::toString();
}

// SuperPixelFeature --------------------------------------------------------------------
SuperPixelFeature::SuperPixelFeature(const cv::Mat & img, const PixelSet & set) {
	mImg = img;
	mSet = set;
	mConfig = QSharedPointer<SuperPixelFeatureConfig>::create();
	
	// TODO: these settings are still generic...
	//mConfig->loadSettings();
	//mConfig->saveDefaultSettings();
}

bool SuperPixelFeature::isEmpty() const {
	return mImg.empty();
}

bool SuperPixelFeature::compute() {

	if (!checkInput())
		return false;

	Timer dt;
	cv::Mat cImg;
	cImg = IP::grayscale(mImg);

	assert(cImg.type() == CV_8UC1);

	std::vector<cv::KeyPoint> keypoints;
	for (const QSharedPointer<const Pixel>& px : mSet.pixels()) {
		assert(px);
		keypoints.push_back(px->toKeyPoint());
	}

	mInfo << "# keypoints before ORB" << keypoints.size();


	std::vector<cv::KeyPoint> kptsIn(keypoints.begin(), keypoints.end());

	cv::Ptr<cv::ORB> features = cv::ORB::create();
	features->compute(cImg, keypoints, mDescriptors);

	// remove SuperPixels that were removed during feature creation
	syncSuperPixels(kptsIn, keypoints);

	mInfo << mDescriptors.rows << "features computed in" << dt;

	return true;
}

QSharedPointer<SuperPixelFeatureConfig> SuperPixelFeature::config() const {
	return qSharedPointerDynamicCast<SuperPixelFeatureConfig>(mConfig);
}

cv::Mat SuperPixelFeature::draw(const cv::Mat & img) const {
	
	cv::Mat avg;
	cv::reduce(mDescriptors, avg, 0, CV_REDUCE_SUM, CV_32FC1);

	// draw mser blobs
	Timer dtf;
	QImage qImg = Image::mat2QImage(img, true);

	QPainter p(&qImg);

	// draw labeled pixels
	mSet.draw(p);

	p.setPen(ColorManager::getColor(0, 0.3));

	Histogram hist(avg);
	hist.draw(p, Rect(10, 10, mDescriptors.cols*3, 100));

	return Image::qImage2Mat(qImg);//Image::qImage2Mat(createLabelImage(Rect(img)));
}

QString SuperPixelFeature::toString() const {
	
	QString str;
	str += config()->name() + " no info to display";
	return str;
}

cv::Mat SuperPixelFeature::features() const {
	return mDescriptors;
}

PixelSet SuperPixelFeature::pixelSet() const {
	return mSet;
}

bool SuperPixelFeature::checkInput() const {
	return !mImg.empty();
}

/// <summary>
/// Synchronizes the super pixels with the features.
/// This function removes SuperPixels from the PixelSet.
/// This is needed since OpenCV removes keypoints that could
/// not be computed. Using this function guarantees that the
/// ith SuperPixel corresponds with the ith row of the feature
/// matrix. NOTE: if we use descriptors such as SIFT which
/// _add_ KeyPoints we're again out-of-sync.
/// </summary>
/// <param name="keyPointsOld">The key points before feature computation.</param>
/// <param name="keyPointsNew">The key points after feature computation (less than keyPointsOld).</param>
void SuperPixelFeature::syncSuperPixels(const std::vector<cv::KeyPoint>& keyPointsOld, const std::vector<cv::KeyPoint>& keyPointsNew) {

	// sync keypoints
	QVector<cv::KeyPoint> kptsOut = QVector<cv::KeyPoint>::fromStdVector(keyPointsNew);

	QList<int> removeIdx;
	for (size_t idx = 0; idx < keyPointsOld.size(); idx++) {


		int kIdx = kptsOut.indexOf(keyPointsOld[idx]);
		if (kIdx == -1)
			removeIdx << (int)idx;
	}

	qSort(removeIdx.begin(), removeIdx.end(), qGreater<int>());
	for (int ri : removeIdx) 
		mSet.remove(mSet.pixels()[ri]);

}

}
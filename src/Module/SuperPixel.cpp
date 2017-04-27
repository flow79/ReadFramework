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

#include "SuperPixel.h"

#include "Image.h"
#include "ImageProcessor.h"
#include "Drawer.h"
#include "Utils.h"
#include "Settings.h"

#pragma warning(push, 0)	// no warnings from includes
#include <QDebug>

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>

#pragma warning(pop)

namespace rdf {


// SuperPixel --------------------------------------------------------------------
SuperPixel::SuperPixel(const cv::Mat& srcImg) {
	mSrcImg = srcImg;
	mConfig = QSharedPointer<SuperPixelConfig>::create();
	mConfig->loadSettings();
}

bool SuperPixel::checkInput() const {

	if (mSrcImg.empty()) {
		mWarning << "the source image must not be empty...";
		return false;
	}

	return true;
}

QSharedPointer<MserContainer> SuperPixel::getBlobs(const cv::Mat & img, int kernelSize) const {

	if (kernelSize > 0) {
		cv::Size kSize(kernelSize, kernelSize);
		cv::Mat k = cv::getStructuringElement(cv::MORPH_ELLIPSE,
			cv::Size(2 * kSize.width + 1, 2 * kSize.height + 1),
			cv::Point(kSize.width, kSize.height));

		// NOTE: dilate, erode is conter-intuitive because ink is usually dark
		cv::dilate(img, img, k);
		cv::erode(img, img, k);
	}

	return mser(img);
}

QSharedPointer<MserContainer> SuperPixel::mser(const cv::Mat & img) const {
	
	cv::Ptr<cv::MSER> mser = cv::MSER::create();
	mser->setMinArea(config()->mserMinArea());
	mser->setMaxArea(config()->mserMaxArea());

	QSharedPointer<MserContainer> blobs(new MserContainer());
	mser->detectRegions(img, blobs->pixels, blobs->boxes);
	assert(blobs->pixels.size() == blobs->boxes.size());

	Timer dtf;
	int nF = filterDuplicates(*blobs, 7, 10);
	//qDebug() << "[duplicates filter]\tremoves" << nF << "blobs in" << dtf;

	dtf.start();
	nF = filterAspectRatio(*blobs);
	//qDebug() << "[aspect ratio filter]\tremoves" << nF << "blobs in" << dtf;

	return blobs;
}

int SuperPixel::filterAspectRatio(MserContainer& blobs, double aRatio) const {

	assert(blobs.pixels.size() == blobs.boxes.size());

	// filter w.r.t aspect ratio
	std::vector<std::vector<cv::Point> > pixelsClean;
	std::vector<cv::Rect> boxesClean;

	for (unsigned int idx = 0; idx < blobs.pixels.size(); idx++) {

		cv::Rect b = blobs.boxes[idx];
		double cARatio = (double)qMin(b.width, b.height) / qMax(b.width, b.height);

		if (cARatio > aRatio) {
			boxesClean.push_back(b);
			pixelsClean.push_back(blobs.pixels[idx]);
		}
	}

	int numRemoved = (int)(blobs.pixels.size() - pixelsClean.size());

	blobs.pixels = pixelsClean;
	blobs.boxes = boxesClean;

	return numRemoved;
}

int SuperPixel::filterDuplicates(MserContainer& blobs, int eps, int upperBound) const {

	int cnt = 0;
	size_t nBoxes = blobs.boxes.size();

	std::vector<std::vector<cv::Point>> pixelsClean;
	std::vector<cv::Rect> boxesClean;

	for (size_t idx = 0; idx < nBoxes; idx++) {

		const cv::Rect& r = blobs.boxes[idx];
		bool duplicate = false;

		for (size_t cIdx = idx+1; cIdx < nBoxes; cIdx++) {

			// should never happen...
			assert(idx != cIdx);

			if (upperBound != -1 && cIdx > idx + upperBound)
				break;

			const cv::Rect& cr = blobs.boxes[cIdx];

			if (abs(r.x - cr.x) < eps &&
				abs(r.y - cr.y) < eps &&
				abs(r.width - cr.width) < eps &&
				abs(r.height - cr.height) < eps) {

				cnt++;
				duplicate = true;
				break;
			}
		}

		if (!duplicate) {
			pixelsClean.push_back(blobs.pixels[idx]);
			boxesClean.push_back(blobs.boxes[idx]);
		}
	}

	blobs.pixels = pixelsClean;
	blobs.boxes = boxesClean;

	return cnt;
}

bool SuperPixel::isEmpty() const {
	return mSrcImg.empty();
}

bool SuperPixel::compute() {

	if (!checkInput())
		return false;

	Timer dt;

	cv::Mat img = mSrcImg.clone();
	img = IP::grayscale(img);
	cv::normalize(img, img, 255, 0, cv::NORM_MINMAX);

	QSharedPointer<MserContainer> rawBlobs(new MserContainer());

	int maxFilter = config()->erosionStep()*config()->numErosionLayers();

	for (int idx = 0; idx < maxFilter; idx += config()->erosionStep()) {

		Timer dti;
		QSharedPointer<MserContainer> cb = getBlobs(img, idx);
		rawBlobs->append(*cb);
		//qDebug() << cb->size() << "/" << rawBlobs->size() << "collected with kernel size" << 2*idx+1 << "in" << dti;
	}

	// filter duplicates that occur from different erosion sizes
	Timer dtf;
	filterDuplicates(*rawBlobs);
	//qDebug() << "[final duplicates filter] removes" << nf << "blobs in" << dtf;

	// convert to pixels
	mBlobs = rawBlobs->toBlobs();
	for (const QSharedPointer<MserBlob>& b : mBlobs)
		mPixels << b->toPixel();

	mDebug << mBlobs.size() << "regions computed in" << dt;

	return true;
}

QString SuperPixel::toString() const {

	QString msg = debugName();

	return msg;
}

QVector<QSharedPointer<Pixel> > SuperPixel::getSuperPixels() const {
	return mPixels;
}

QVector<QSharedPointer<MserBlob>> SuperPixel::getMserBlobs() const {
	return mBlobs;
}

PixelSet SuperPixel::pixelSet() const {

	PixelSet ps(mPixels);
	
	return ps;
}

QSharedPointer<SuperPixelConfig> SuperPixel::config() const {
	return qSharedPointerDynamicCast<SuperPixelConfig>(mConfig);
}

cv::Mat SuperPixel::draw(const cv::Mat & img, const QColor& col) const {

	// draw super pixels
	Timer dtf;
	QPixmap pm = Image::mat2QPixmap(img);
	QPainter p(&pm);


	//DBScanPixel dbs(mPixels);
	//dbs.compute();
	//QVector<PixelSet> sets = dbs.sets();
	//qDebug() << "dbscan found" << sets.size() << "clusters in" << dtf;

	//for (auto s : sets) {
	//	Drawer::instance().setColor(ColorManager::getColor());
	//	QPen pen = Drawer::instance().pen();
	//	p.setPen(pen);
	//	s.draw(p);
	//}

	p.setPen(col);

	for (int idx = 0; idx < mBlobs.size(); idx++) {
	
		
		if (!col.isValid())
			p.setPen(ColorManager::getColor());

		// uncomment if you want to see MSER & SuperPixel at the same time
		//mBlobs[idx].draw(p);
		mPixels[idx]->draw(p, 0.2, Pixel::DrawFlags() | Pixel::draw_ellipse | Pixel::draw_stats | Pixel::draw_label_colors | Pixel::draw_tab_stops);
		//qDebug() << mPixels[idx].ellipse();
	}

	qDebug() << "drawing takes" << dtf;
	return Image::qPixmap2Mat(pm);
}

cv::Mat SuperPixel::drawMserBlobs(const cv::Mat & img, const QColor& col) const {

	// draw mser blobs
	Timer dtf;
	QPixmap pm = Image::mat2QPixmap(img);
	QPainter p(&pm);
	p.setPen(col);

	for (auto b : mBlobs) {

		if (!col.isValid())
			p.setPen(ColorManager::getColor());

		b->draw(p);
	}

	qDebug() << "drawing takes" << dtf;
	
	return Image::qPixmap2Mat(pm);
}

// SuperPixelConfig --------------------------------------------------------------------
SuperPixelConfig::SuperPixelConfig() : ModuleConfig("Super Pixel") {
}

QString SuperPixelConfig::toString() const {
	return ModuleConfig::toString();
}

/// <summary>
/// The minimum SuperPixel area in pixel.
/// NOTE: the segmented pixels are summed up
/// rather than its resulting ellipses.
/// </summary>
/// <returns></returns>
int SuperPixelConfig::mserMinArea() const {
	return mMserMinArea;
}

/// <summary>
/// The maximum SuperPixel area in pixel.
/// NOTE: the segmented pixels are summed up
/// rather than its resulting ellipses.
/// </summary>
/// <returns></returns>
int SuperPixelConfig::mserMaxArea() const {
	
	return checkParam(mMserMaxArea, mserMinArea(), INT_MAX, "mserMaxArea");
}

/// <summary>
/// The erosion step in pixel.
/// The kernelsize is iteratively increased
/// when computing the erosion layers.
/// </summary>
/// <returns></returns>
int SuperPixelConfig::erosionStep() const {

	return checkParam(mErosionStep, 1, 20, "erosionStep");
}

void SuperPixelConfig::setNumErosionLayers(int numLayers) {
	mNumErosionLayers = numLayers;
}

/// <summary>
/// Numbers the erosion layers.
/// The image is iteratively eroded in order to split
/// cursive handwriting. Specify how many erosion layers
/// should be created - when testing 3 seemed to be a good
/// trade-off between accuracy and speed.
/// </summary>
/// <returns></returns>
int SuperPixelConfig::numErosionLayers() const {
	
	return checkParam(mNumErosionLayers, 1, 20, "numErosionLayers");
}

void SuperPixelConfig::load(const QSettings & settings) {

	// add parameters
	mMserMinArea = settings.value("mserMinArea", mserMinArea()).toInt();
	mMserMaxArea = settings.value("mserMaxArea", mserMaxArea()).toInt();
	mErosionStep = settings.value("erosionStep", erosionStep()).toInt();
	mNumErosionLayers = settings.value("numErosionLayers", numErosionLayers()).toInt();
}

void SuperPixelConfig::save(QSettings & settings) const {

	// add parameters
	settings.setValue("mserMinArea", mserMinArea());
	settings.setValue("mserMaxArea", mserMaxArea());
	settings.setValue("erosionStep", erosionStep());
	settings.setValue("numErosionLayers", numErosionLayers());
}

// MserContainer --------------------------------------------------------------------
void MserContainer::append(const MserContainer & o) {

	std::move(o.pixels.begin(), o.pixels.end(), std::back_inserter(pixels));
	std::move(o.boxes.begin(), o.boxes.end(), std::back_inserter(boxes));
}

QVector<QSharedPointer<MserBlob>> MserContainer::toBlobs() const {
	
	QVector<QSharedPointer<MserBlob> > blobs;
	for (unsigned int idx = 0; idx < pixels.size(); idx++) {

		QSharedPointer<MserBlob> b(new MserBlob(pixels[idx], boxes[idx], QString::number(idx)));
		blobs << b;
	}
	
	return blobs;
}

size_t MserContainer::size() const {
	return pixels.size();
}

// LocalOrientationConfig --------------------------------------------------------------------
LocalOrientationConfig::LocalOrientationConfig() : ModuleConfig("Local Orientation") {
}

QString LocalOrientationConfig::toString() const {
	
	QString msg;
	msg += "scales " + scaleIvl().toString();
	msg += " orientations " + QString::number(numOrientations());

	return msg;
}

int LocalOrientationConfig::maxScale() const {
	return mMaxScale;
}

int LocalOrientationConfig::minScale() const {
	return mMinScale;
}

Vector2D LocalOrientationConfig::scaleIvl() const {
	return Vector2D(mMinScale, mMaxScale);
}

int LocalOrientationConfig::numOrientations() const {
	return mNumOr;
}

int LocalOrientationConfig::histSize() const {
	return mHistSize;
}

void LocalOrientationConfig::setNumOrientations(int numOr) {
	mNumOr = numOr;
}

void LocalOrientationConfig::setMaxScale(int maxScale) {
	mMaxScale = maxScale;
}

void LocalOrientationConfig::setMinScale(int minScale) {
	mMinScale = minScale;
}

void LocalOrientationConfig::load(const QSettings & settings) {

	// add parameters
	mMaxScale = settings.value("MaxScale", mMaxScale).toInt();
	mMinScale = settings.value("MinScale", mMinScale).toInt();
	mNumOr = settings.value("NumOrientations", mNumOr).toInt();
	mHistSize = settings.value("HistSize", mHistSize).toInt();

}

void LocalOrientationConfig::save(QSettings & settings) const {

	// add parameters
	settings.setValue("MaxScale", mMaxScale);
	settings.setValue("MinScale", mMinScale);
	settings.setValue("NumOrientations", mNumOr);
	settings.setValue("HistSize", mHistSize);
}

// LocalOrientation --------------------------------------------------------------------
LocalOrientation::LocalOrientation(const PixelSet& set) {
	mSet = set;
	mConfig = QSharedPointer<LocalOrientationConfig>::create();
	mConfig->loadSettings();
}

bool LocalOrientation::isEmpty() const {
	return mSet.isEmpty();
}

bool LocalOrientation::compute() {
	
	if (!checkInput())
		return false;
	
	//Timer dt;

	// estimate orientation per scale pyramid
	//for (const QSharedPointer<PixelSet>& set : mSet.splitScales()) {
		
		QVector<Pixel*> ptrSet;
		for (const QSharedPointer<Pixel>& p : mSet.pixels())
			ptrSet << p.data();

		for (Pixel* p : ptrSet)
			computeScales(p, ptrSet);
	//}

	//mDebug << config()->toString();
	//mDebug << "computed in" << dt;

	return true;
}

QString LocalOrientation::toString() const {
	return config()->toString();
}

QSharedPointer<LocalOrientationConfig> LocalOrientation::config() const {
	return qSharedPointerDynamicCast<LocalOrientationConfig>(mConfig);
}

PixelSet LocalOrientation::set() const {
	return mSet;
}

bool LocalOrientation::checkInput() const {
	return !mSet.isEmpty();
}

void LocalOrientation::computeScales(Pixel* pixel, const QVector<Pixel*>& set) const {
	
	const Vector2D& ec = pixel->center();
	QVector<Pixel*> cSet = set;
	
	// iterate over all scales
	for (double cRadius = config()->maxScale(); cRadius >= config()->minScale(); cRadius /= 2.0) {

		QVector<Pixel*> neighbors;

		// create neighbor set
		for (Pixel* p : cSet) {

			if (ec.isNeighbor(p->center(), cRadius)) {
				neighbors << p;
			}
		}

		// compute orientation histograms
		computeAllOrHists(pixel, neighbors, cRadius);

		// reduce the set (since we reduce the radius, it must be contained in the current set)
		cSet = neighbors;
	}
}

void LocalOrientation::computeAllOrHists(Pixel* pixel, const QVector<Pixel*>& set, double radius) const {

	const Vector2D& ec = pixel->center();

	QVector<const Pixel*> neighbors;

	// create neighbor set
	for (const Pixel* p : set) {

		if (ec.isNeighbor(p->center(), radius)) {
			neighbors << p;
		}
	}

	// compute all orientations
	int nOr = config()->numOrientations();
	int histSize = config()->histSize();
	cv::Mat orHist(nOr, histSize, CV_32FC1, cv::Scalar(0));
	cv::Mat sparsity(1, nOr, CV_32FC1);

	for (int k = 0; k < nOr; k++) {

		// create orientation vector
		float sp = 0.0f;
		double cAngle = k * CV_PI / nOr;
		Vector2D orVec(radius, 0);
		orVec.rotate(cAngle);

		cv::Mat cRow = orHist.row(k);
		computeOrHist(pixel, neighbors, orVec, cRow, sp);

		sparsity.at<float>(0, k) = sp;
	}

	pixel->addStats(QSharedPointer<PixelStats>(new PixelStats(orHist, sparsity, radius, pixel->id())));
}

void LocalOrientation::computeOrHist(const Pixel* pixel, 
	const QVector<const Pixel*>& set, 
	const Vector2D & histVec, 
	cv::Mat& orHist,
	float& sparsity) const {


	double hl = histVec.length();
	Vector2D histVecNorm = histVec;
	histVecNorm /= hl;
	double scale = 1.0 / (2 * hl) * (orHist.cols - 1);

	const Vector2D pc = pixel->center();

	// prepare histogram
	float* orPtr = orHist.ptr<float>();

	for (const Pixel* p : set) {

		Vector2D lc = p->center() - pc;
		double v = histVecNorm * lc;

		// bin it
		int hIdx = qRound((v + hl) * scale);
		assert(hIdx >= 0 && hIdx < orHist.cols);

		orPtr[hIdx] += 1;

		//// estimate radius (assuming circles)
		//const Vector2D& a = p->ellipse().axis();
		//int r = qRound((a.x() + a.y()) / 2.0 * scale);
		//int start = (hIdx - r < 0) ? 0 : hIdx - r;

		//for (int idx = start; idx < hIdx + r && idx < orHist.cols; idx++) {
		//	orPtr[idx] += 1;
		//}

	}

	// estimate sparsity
	double sumNonZero = 0;
	
	for (int cIdx = 0; cIdx < orHist.cols; cIdx++) {
		if (orPtr[cIdx] != 0)
			sumNonZero++;
	}

	sparsity = (float)std::log(sumNonZero/orHist.cols);
	
	// DFT according to Koo16
	cv::dft(orHist, orHist);
	assert(!orHist.empty());

	//orPtr = orHist.ptr<float>();
	float normValSq = orPtr[0] * orPtr[0];	// the normalization term is always at [0] - we need it sqaured

	for (int cIdx = 0; cIdx < orHist.cols; cIdx++) {
		
		if (cIdx >= 10) {
			// see Koo16: val = -log( (val*val) / (hist[0]*hist[0]) + 1.0);
			orPtr[cIdx] *= orPtr[cIdx];
			orPtr[cIdx] /= normValSq;
			orPtr[cIdx] += 1.0f;	// for log
			orPtr[cIdx] = std::log(orPtr[cIdx]) * -1.0f;
		}
		else {
			// remove very low frequencies - they might create larger peaks than the recurring frequency
			orPtr[cIdx] = 0.0f;
		}
	}
}

cv::Mat LocalOrientation::draw(const cv::Mat & img, const QString & id, double radius) const {

	QSharedPointer<Pixel> pixel;
	for (auto p : mSet.pixels())
		if (p->id() == id) {
			pixel = p;
			break;
		}

	if (!pixel) {
		qInfo() << "cannot draw local orientation for" << id << "because I did not find it...";
		return img;
	}

	// debug - remove
	QPixmap pm = Image::mat2QPixmap(img);
	QPainter painter(&pm);

	Ellipse e(pixel->center(), Vector2D(radius, radius));
	e.draw(painter, 0.3);
	painter.setPen(ColorManager::colors()[2]);

	const Vector2D& ec = pixel->center();
	QVector<const Pixel*> neighbors;

	// create neighbor set
	for (const QSharedPointer<Pixel>& p : mSet.pixels()) {
		
		if (ec.isNeighbor(p->center(), radius)) {
			neighbors << p.data();
			p->draw(painter, 0.3, Pixel::DrawFlags() | Pixel::draw_ellipse | Pixel::draw_stats);
		}
	}

	// draw the selected pixel in a different color
	painter.setPen(ColorManager::colors()[0]);
	pixel->draw(painter, 0.3, Pixel::draw_all);

	// compute all orientations
	int histSize = config()->histSize();
	int nOr = config()->numOrientations();
	cv::Mat orHist(nOr, histSize, CV_32FC1, cv::Scalar(0));
	
	for (int k = 0; k < nOr; k++) {

		// create orientation vector
		float sp = 0.0f;	// not needed
		double cAngle = k * CV_PI / nOr;
		Vector2D orVec(radius, 0);
		orVec.rotate(cAngle);

		cv::Mat cRow = orHist.row(k);
		computeOrHist(pixel.data(), neighbors, orVec, cRow, sp);

		//qDebug().noquote() << Image::printImage(cRow, "row" + QString::number(cAngle * DK_RAD2DEG));

		rdf::Histogram h(cRow);
		Rect r(30 + k * (histSize+5), pixel->center().y()-radius-150, histSize, 50);
		h.draw(painter, r);
		painter.drawText(r.bottomLeft().toQPoint(), QString::number(cAngle * DK_RAD2DEG));
	}

	return Image::qPixmap2Mat(pm);
}

// ScaleSpaceSuperPixel --------------------------------------------------------------------
ScaleSpaceSuperPixel::ScaleSpaceSuperPixel(const cv::Mat & img) {
	mConfig = QSharedPointer<ScaleSpaceSPConfig>::create();
	mSrcImg = img;
}

bool ScaleSpaceSuperPixel::isEmpty() const {
	return mSrcImg.empty();
}

bool ScaleSpaceSuperPixel::compute() {

	if (!checkInput())
		return false;

	Timer dt;

	cv::Mat img = mSrcImg.clone();
	img = IP::grayscale(img);
	cv::normalize(img, img, 255, 0, cv::NORM_MINMAX);

	Config::instance().global().numScales = config()->numLayers();

	for (int idx = 0; idx < config()->numLayers(); idx++) {
	
		// compute super pixel
		if (config()->minLayer() <= idx) {

			SuperPixel spm(img);

			// do not erode large pyramid levels
			if (idx > 3) {
				auto cc = spm.config();
				cc->setNumErosionLayers(1);
			}

			// get super pixels of the current scale
			if (!spm.compute())
				mWarning << "could not compute super pixels for layer #" << idx;

			PixelSet set = spm.getSuperPixels();

			if (idx > 0) {

				// assign the pyramid level
				for (auto p : set.pixels())
					p->setPyramidLevel(idx);

				// re-scale
				double sf = std::pow(2, idx);
				set.scale(sf);
			}

			mSet += set;
		}

		cv::resize(img, img, cv::Size(), 0.5, 0.5, CV_INTER_AREA);
	}

	// filter from all scales
	mSet.filterDuplicates();
	mInfo << mSet.size() << "pixels extracted in" << dt;

	return true;
}

QString ScaleSpaceSuperPixel::toString() const {
	return config()->toString();
}

QSharedPointer<ScaleSpaceSPConfig> ScaleSpaceSuperPixel::config() const {
	return qSharedPointerDynamicCast<ScaleSpaceSPConfig>(mConfig);
}

PixelSet ScaleSpaceSuperPixel::superPixels() const {
	return mSet;
}

cv::Mat ScaleSpaceSuperPixel::draw(const cv::Mat & img) const {
	
	// debug - remove
	QPixmap pm = Image::mat2QPixmap(img);
	QPainter p(&pm);

	p.setPen(ColorManager::blue());

	for (auto px : mSet.pixels()) {
		px->draw(p, 0.3, Pixel::DrawFlags() | Pixel::draw_center | Pixel::draw_stats);
	}

	return Image::qPixmap2Mat(pm);
}

bool ScaleSpaceSuperPixel::checkInput() const {

	return !mSrcImg.empty();
}

// ScaleSpaceSpConfig --------------------------------------------------------------------
ScaleSpaceSPConfig::ScaleSpaceSPConfig() : ModuleConfig("Scale Space Super Pixel") {
}

QString ScaleSpaceSPConfig::toString() const {
	return ModuleConfig::toString();
}

int ScaleSpaceSPConfig::numLayers() const {
	return checkParam(mNumLayers, 1, 10, "numLayers");
}

int ScaleSpaceSPConfig::minLayer() const {
	return checkParam(mMinLayer, 0, numLayers()-1, "minLayer");
}

void ScaleSpaceSPConfig::load(const QSettings & settings) {

	mNumLayers = settings.value("numLayers", numLayers()).toInt();
	mMinLayer = settings.value("minLayer", minLayer()).toInt();
}

void ScaleSpaceSPConfig::save(QSettings & settings) const {
	settings.setValue("numLayers", numLayers());
	settings.setValue("minLayer", minLayer());
}



}
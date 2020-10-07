/**
 * @file    ProbabilityHandler.cpp
 *
 * @author  btran
 *
 */

#include <opencv2/opencv.hpp>

#include <sensors_calib/ProbabilityHandler.hpp>
#include <sensors_calib/utils/utils.hpp>

namespace perception
{
ProbabilityHandler::ProbabilityHandler(int numBins)
    : m_numBins(numBins)
{
    if (m_numBins <= 0 || m_numBins > perception::MAX_BINS) {
        throw std::runtime_error("invalid number of bins");
    }

    m_grayProb = Probability::zeros(1, m_numBins, CV_64FC1);
    m_intensityProb = Probability::zeros(1, m_numBins, CV_64FC1);
    m_jointProb = JointProbability::zeros(m_numBins, m_numBins, CV_64FC1);
}

ProbabilityHandler::~ProbabilityHandler()
{
}

bool ProbabilityHandler::estimateMLE(const HistogramHandler::Ptr& histogram)
{
    if (histogram->totalPoints() == 0) {
        DEBUG_LOG("empty sample datas");
        return false;
    }
    m_totalPoints = histogram->totalPoints();

    auto stds = histogram->calculateStds();
    double sigmaGray = stds[0];
    double sigmaIntensity = stds[1];

    for (int i = 0; i < m_numBins; ++i) {
        for (int j = 0; j < m_numBins; ++j) {
            m_jointProb.at<double>(i, j) = histogram->jointHist().at<double>(i, j) / m_totalPoints;
        }

        m_grayProb.at<double>(i) = histogram->grayHist().at<double>(i) / m_totalPoints;
        m_intensityProb.at<double>(i) = histogram->intensityHist().at<double>(i) / m_totalPoints;
    }

    // bandwidths for kernel density estimation based on Silverman's rule of thumb
    m_sigmaGrayBandwidth = 1.06 * std::sqrt(sigmaGray) / std::pow(m_totalPoints, 0.2);
    m_sigmaIntensityBandwidth = 1.06 * std::sqrt(sigmaIntensity) / std::pow(m_totalPoints, 0.2);

    this->smoothKDE();

    return true;
}

double ProbabilityHandler::calculateMICost() const
{
    double grayEntropy = 0.0, intensityEntropy = 0.0, jointEntropy = 0.0;

    for (int i = 0; i < m_numBins; ++i) {
        for (int j = 0; j < m_numBins; ++j) {
            double jointV = m_jointProb.at<double>(i, j);
            if (!perception::almostEquals(jointV, 0.0)) {
                jointEntropy += -jointV * std::log2(jointV);
            }
        }
        double grayV = m_grayProb.at<double>(i);
        double intensityV = m_intensityProb.at<double>(i);

        if (!perception::almostEquals(grayV, 0.0)) {
            grayEntropy += -grayV * std::log2(grayV);
        }
        if (!perception::almostEquals(intensityV, 0.0)) {
            intensityEntropy += -intensityV * std::log2(intensityV);
        }
    }

    return grayEntropy + intensityEntropy - jointEntropy;
}

void ProbabilityHandler::smoothKDE()
{
    cv::GaussianBlur(m_grayProb, m_grayProb, cv::Size(0, 0), m_sigmaGrayBandwidth);
    cv::GaussianBlur(m_intensityProb, m_intensityProb, cv::Size(0, 0), m_sigmaIntensityBandwidth);
    cv::GaussianBlur(m_jointProb, m_jointProb, cv::Size(0, 0), m_sigmaGrayBandwidth, m_sigmaIntensityBandwidth);
}
}  // namespace perception

// FeatureDetect.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

using namespace cv;

Mat in_img;

void Th_d_onChange(int value, void* userdata) {
	std::vector<Mat> mat_data = *(std::vector<Mat>*)(userdata);
	Mat3b img = mat_data[0].clone();
	Mat1f dValueMat = (Mat1f)mat_data[1];
	Mat1f out;
	threshold(dValueMat, out, value, 1, THRESH_BINARY);
	for (int i = 0; i < dValueMat.cols; i++) {
		for (int j = 0; j < dValueMat.rows; j++) {
			if (out(j, i) != 0) {
				circle(img, Point(i, j), 1, Scalar(0, 0, 255), -1);
			}
		}
	}
	imshow("Candidate Grid Points", img);
}
void CandidateGP(const Mat3b& input, Mat& output) {
	
	int L = 20; //mask size;
	std::vector<Mat> bgr;
	split(input, bgr);
	Mat1f B_f, G_f, R_f;
	bgr[0].convertTo(B_f, CV_32FC1);
	bgr[1].convertTo(G_f, CV_32FC1);
	bgr[2].convertTo(R_f, CV_32FC1);

	Mat1f dValueMat,dValueMat_bin;
	Mat1f B_out, G_out, R_out;

	Mat1f K = Mat1f::zeros(Size(2 * L + 1, 2 * L + 1));//convolve mask
	K.row(L).setTo(1);									//. -1  . 
	K.col(L).setTo(-1);									//1  0  1
	K.at<float>(L, L) = 0;								//. -1  .

	filter2D(B_f, B_out, CV_32FC1, K, Point(-1, -1), 0, BORDER_REPLICATE);
	filter2D(G_f, G_out, CV_32FC1, K, Point(-1, -1), 0, BORDER_REPLICATE);
	filter2D(R_f, R_out, CV_32FC1, K, Point(-1, -1), 0, BORDER_REPLICATE);
	B_out = abs(B_out);
	G_out = abs(G_out);
	R_out = abs(R_out);
	dValueMat = max(max(B_out, G_out), R_out);
	Mat1b borderMask = Mat1b::ones(dValueMat.size()); //Mask out the results on the border pixels
	borderMask(Rect(L, L, dValueMat.cols - 2 * L, dValueMat.rows - 2 * L)).setTo(0);
	dValueMat.setTo(0, borderMask);

	namedWindow("Candidate Grid Points");
	imshow("Candidate Grid Points", input);
	int th_d = 255;
	int th_d_max = 255 * (2 * L + 1);
	std::vector<Mat> userdata;
	userdata.push_back(input);
	userdata.push_back(dValueMat);
	createTrackbar("th_d", "Candidate Grid Points", &th_d, th_d_max, Th_d_onChange, &userdata);

	waitKey();
	threshold(dValueMat, dValueMat_bin, th_d, 1, THRESH_BINARY);
	//std::cout << dValueMat_bin << std::endl;
	Mat dValueMat_byte;
	dValueMat_bin.convertTo(dValueMat_byte,CV_8UC1);
	findNonZero(dValueMat_byte, output);
	
}

Mat CircRoi(Mat& img, Rect& region) {
	Mat rectRoi = img(region);
	Mat circMask(rectRoi.size(), CV_8UC1, Scalar(0));
	Point center = Point((circMask.cols - 1) / 2, (circMask.rows - 1) / 2);
	int r = (min(circMask.cols, circMask.rows) - 1) / 2;
	circle(circMask, center, r, Scalar(255), -1, LINE_AA); // filled circle
	Mat circRoi;
	rectRoi.copyTo(circRoi, circMask);
	return circRoi;
	//bitwise_and(roi, roi, circRoi, mask); // retain only pixels inside the circle
}

double PMCC(Mat1b& roi, int n) {
	double a_sum = 0;
	double b_sum = 0;
	double ab_sum = 0;
	double a2_sum = 0;
	double b2_sum = 0;

	for (int i = 0; i < roi.cols; i++) {
		for (int j = 0; j < roi.rows; j++) {
			double a = roi.at<uchar>(j, i);
			double b = roi.at<uchar>(roi.rows - j - 1, roi.cols - i - 1);
			a_sum += a;
			b_sum += b;
			ab_sum += a * b;
			a2_sum += a * a;
			b2_sum += b * b;
		}
	}

	double rho = n * ab_sum - a_sum * b_sum;
	rho /= sqrt(n*a2_sum - a_sum * a_sum) * sqrt(n*b2_sum - b_sum * b_sum);
	return rho;
}

double MSD(Mat1b& roi, int n) {
	double a_minus_b_2_sum = 0;
	double a_minus_avg_2_sum = 0;
	double avg = sum(roi)[0] / (double)n;
	for (int i = 0; i < roi.cols; i++) {
		for (int j = 0; j < roi.rows; j++) {
			double a = roi.at<uchar>(j, i);
			double b = roi.at<uchar>(roi.rows - j - 1, roi.cols - i - 1);
			a_minus_b_2_sum += (a - b)*(a - b);
			a_minus_avg_2_sum += (a - avg)*(a - avg);
		}
	}

	double rho = a_minus_b_2_sum/ a_minus_avg_2_sum;
	
	return rho;
}

void RefineGP(const Mat1b& grayImg, Mat& gpCandidates, String ccMethod) {
	float roi_r = 20;
	Point center(roi_r, roi_r);
	Mat circMask(roi_r * 2 + 1, roi_r * 2 + 1, CV_8UC1, Scalar(0));
	circle(circMask, center, roi_r, Scalar(255), -1, LINE_AA); // filled circle
	int n = countNonZero(circMask);

	Mat1d rhoValueMat(grayImg.size(), 0);
	if (ccMethod == "PMCC") {
		for (auto i = gpCandidates.begin<Point>(); i != gpCandidates.end<Point>(); ++i) {
			int rect_x = (*i).x - roi_r;
			int rect_y = (*i).y - roi_r;
			Mat1b rectRoi = grayImg(Rect(rect_x, rect_y, roi_r * 2 + 1, roi_r * 2 + 1));
			Mat1b circRoi;
			rectRoi.copyTo(circRoi, circMask);
			double rho = PMCC(circRoi, n);
			//double rho = MSD(circRoi, n);
			rhoValueMat.at<double>(*i) = rho;
		}
	}

	Mat1b rhoValueMat_gray;
	rhoValueMat.convertTo(rhoValueMat_gray, CV_8UC1,255);
	normalize(rhoValueMat_gray, rhoValueMat_gray, 0, 255, NORM_MINMAX);
	Mat rhoValueMat_color;
	applyColorMap(rhoValueMat_gray, rhoValueMat_color, COLORMAP_JET);
	imshow("rhoValueMat_color", rhoValueMat_color);
	waitKey();

}
int main()
{
	//String in_path = "./pattern.png";
	String in_path = "./cam.jpg";
	in_img = imread(in_path);

	Mat1b gray;
	cvtColor(in_img, gray, CV_BGR2GRAY);

	Mat gp;
	CandidateGP(in_img, gp);
	RefineGP(gray, gp, "PMCC");

	waitKey();

	return 0;
}


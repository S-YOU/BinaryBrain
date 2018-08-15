#include <iostream>
#include <random>
#include <opencv2/opencv.hpp>
#include "NeuralNet.h"
#include "NeuralNetAffine.h"
#include "NeuralNetSigmoid.h"
#include "NeuralNetSoftmax.h"
#include "mnist_read.h"


void evaluation_net(NeuralNet<>& net, std::vector< std::vector<float> >& images, std::vector<std::uint8_t>& labels);

void img_show(std::vector<float>& image)
{
	cv::Mat img(28, 28, CV_32F);
	memcpy(img.data, &image[0], sizeof(float) * 28 * 28);
	cv::imshow("img", img);
	cv::waitKey();
}


int main()
{
	std::mt19937_64 mt(1);

#ifdef _DEBUG
	int train_max_size = 3;
	int test_max_size = 1;
#else
	int train_max_size = -1;
	int test_max_size = -1;
#endif

	// MNISTデータ読み込み
	auto train_image = mnist_read_images_real<float>("train-images-idx3-ubyte", train_max_size);
	auto train_label = mnist_read_labels_real<float, 10>("train-labels-idx1-ubyte", train_max_size);
	auto test_image = mnist_read_images_real<float>("t10k-images-idx3-ubyte", test_max_size);
	auto test_label = mnist_read_labels("t10k-labels-idx1-ubyte", test_max_size);

	// NET構築
	NeuralNet<> net;
	NeuralNetAffine<> affine0(28*28, 50);
	NeuralNetSigmoid<> sigmoid0(50);
	NeuralNetAffine<> affine1(50, 10);
	NeuralNetSoftmax<> softmax1(10);
	net.AddLayer(&affine0);
	net.AddLayer(&sigmoid0);
	net.AddLayer(&affine1);
	net.AddLayer(&softmax1);
	

	evaluation_net(net, test_image, test_label);

	// インデックス作成
	std::vector<size_t> train_index(train_image.size());
	for (size_t i = 0; i < train_index.size(); ++i) {
		train_index[i] = i;
	}

	size_t batch_size = 10000;

	batch_size = std::min(batch_size, train_image.size());
	
	for (; ; ) {
		std::shuffle(train_index.begin(), train_index.end(), mt);

		net.SetBatchSize(batch_size);

		for (size_t frame = 0; frame < batch_size; ++frame) {
			net.SetInputValue(frame, train_image[train_index[frame]]);
		}

		net.Forward();

		for (size_t frame = 0; frame < batch_size; ++frame) {
			auto values = net.GetOutputValue(frame);

			for (size_t node = 0; node < values.size(); ++node) {
				values[node] -= train_label[train_index[frame]][node];
				values[node] /= (float)batch_size;
			
	//			std::cout << train_label[train_index[frame]][node] << " ";
			}
	//		std::cout << std::endl;

	//		img_show(train_image[train_index[frame]]);

			net.SetOutputError(frame, values);
		}

		net.Backward();
		net.Update(0.1);

		evaluation_net(net, test_image, test_label);
	}

	return 0;
}


void evaluation_net(NeuralNet<>& net, std::vector< std::vector<float> >& images, std::vector<std::uint8_t>& labels)
{
	// 評価サイズ設定
	net.SetBatchSize(images.size());
	
	// 評価画像設定
	for ( size_t frame = 0; frame < images.size(); ++frame ){
		net.SetInputValue(frame, images[frame]);
	}

	// 評価実施
	net.Forward();

	// 結果集計
	int ok_count = 0;
	for (size_t frame = 0; frame < images.size(); ++frame) {
		int max_idx = argmax<float>(net.GetOutputValue(frame));
		ok_count += (max_idx == (int)labels[frame] ? 1 : 0);

//		std::cout << (int)labels[frame] << std::endl;
//		img_show(images[frame]);

	}

	std::cout << ok_count << " / " << images.size() << std::endl;
}




#if 0

#include <windows.h>
#include <tchar.h>
#pragma comment(lib, "winmm.lib")

#include <omp.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <utility>
#include "Eigen/Core"
#include "opencv2/opencv.hpp"
#include "mnist_read.h"
#include "ShuffleSet.h"


int main()
{
#if 0
	Eigen::MatrixXf a(2, 1);
	Eigen::MatrixXf b(1, 2);
	a(0, 0) = 1;
//	a(0, 1) = 2;
	a(1, 0) = 3;
//	a(1, 1) = 4;
	b(0, 0) = 10;
	b(0, 1) = 20;
	Eigen::MatrixXf c = a * b;

	std::cout << "A = \n" << a << std::endl;
	std::cout << "B = \n" << b << std::endl;
	std::cout << "C = \n" << c << std::endl;
#endif

#if 1
	float a[4] = { 1,2,3,4 };
	float b[4] = { 10,20,30,40 };
	float c[4] = { 0, 0, 0, 0 };
	float d[2] = { 1000, 2000 };

	Eigen::Matrix<float, -1, -1, Eigen::RowMajor> mat;
//	Eigen::Matrix<float, -1, -1> mat;


	Eigen::Map< Eigen::Matrix<float, -1, -1, Eigen::RowMajor> > mat_a(a, 2, 2);
	Eigen::Map<Eigen::MatrixXf> mat_b(b, 2, 2);
	Eigen::Map<Eigen::MatrixXf> mat_c(c, 2, 2);
	Eigen::Map<Eigen::VectorXf> mat_d(d, 2);

	mat_c = mat_a * mat_b;
	std::cout << "A = \n" << mat_a << std::endl;
	std::cout << "B = \n" << mat_b << std::endl;
	std::cout << "C = \n" << mat_c << std::endl;
	std::cout << "D = \n" << mat_d << std::endl;
	mat_c.colwise() += mat_d;
	std::cout << "C = \n" << mat_c << std::endl;

	std::cout << "c =" << std::endl;
	std::cout << c[0] << std::endl;
	std::cout << c[1] << std::endl;
	std::cout << c[2] << std::endl;
	std::cout << c[3] << std::endl;
#endif

	getchar();

	return 0;
}

#endif
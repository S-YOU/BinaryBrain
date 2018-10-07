#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>

#include <opencv2/opencv.hpp>


#include "bb/NeuralNet.h"
#include "bb/NeuralNetUtility.h"

#include "bb/NeuralNetSigmoid.h"
#include "bb/NeuralNetReLU.h"
#include "bb/NeuralNetSoftmax.h"
#include "bb/NeuralNetBinarize.h"

#include "bb/NeuralNetBatchNormalization.h"
#include "bb/NeuralNetSparseAffineSigmoid.h"

#include "bb/NeuralNetAffine.h"
#include "bb/NeuralNetSparseAffine.h"
#include "bb/NeuralNetSparseAffineBc.h"
#include "bb/NeuralNetSparseBinaryAffine.h"

#include "bb/NeuralNetRealToBinary.h"
#include "bb/NeuralNetBinaryToReal.h"
#include "bb/NeuralNetBinaryLut6.h"
#include "bb/NeuralNetBinaryLut6VerilogXilinx.h"
#include "bb/NeuralNetBinaryFilter.h"

#include "bb/NeuralNetConvolution.h"
#include "bb/NeuralNetMaxPooling.h"

#include "bb/NeuralNetConvolutionPack.h"
#include "bb/NeuralNetBinaryMultiplex.h"

#include "bb/NeuralNetOptimizerAdam.h"

#include "bb/NeuralNetLossCrossEntropyWithSoftmax.h"
#include "bb/NeuralNetAccuracyCategoricalClassification.h"


std::vector<std::uint8_t>				train_labels;
std::vector< std::vector<float> >		train_images;
std::vector< std::vector<float> >		train_onehot;

std::vector<std::uint8_t>				test_labels;
std::vector< std::vector<float> >		test_images;
std::vector< std::vector<float> >		test_onehot;


void read_cifer10(std::istream& is, std::vector<std::uint8_t>& vec_label, std::vector< std::vector<std::uint8_t> >& vec_image)
{
	while (!is.eof()) {
		std::uint8_t label;
		is.read((char*)&label, 1);
		if (is.eof()) { break; }

		vec_label.push_back(label);

		std::vector<std::uint8_t> image(32 * 32 * 3);
		is.read((char*)&image[0], 32*32*3);
		vec_image.push_back(image);
	}
}

void read_cifer10(std::string filename, std::vector<std::uint8_t>& vec_label, std::vector< std::vector<std::uint8_t> >& vec_image)
{
	std::ifstream ifs(filename, std::ios::binary);
	read_cifer10(ifs, vec_label, vec_image);
}


// ���Ԍv��
std::chrono::system_clock::time_point m_base_time;

void reset_time(void) {
	m_base_time = std::chrono::system_clock::now();
}

double get_time(void)
{
	auto now_time = std::chrono::system_clock::now();
	return std::chrono::duration_cast<std::chrono::milliseconds>(now_time - m_base_time).count() / 1000.0;
}

// �i���\��
void PrintProgress(float loss, size_t progress, size_t size)
{
	size_t rate = progress * 100 / size;
	std::cout << "[" << rate << "% (" << progress << "/" << size << ")] loss : " << loss << "\r" << std::flush;
}

void ClearProgress(void) {
	std::cout << "                                                                \r" << std::flush;
}




// �l�b�g�̐��𗦕]��
double CalcAccuracy(bb::NeuralNet<>& net, std::vector< std::vector<float> >& images, std::vector<std::uint8_t>& labels)
{
	const size_t max_batch_size = 128;

	int ok_count = 0;
	for (size_t x_index = 0; x_index < images.size(); x_index += max_batch_size) {
		// �����̃o�b�`�T�C�Y�N���b�v
		size_t batch_size = std::min(max_batch_size, images.size() - x_index);

		// �f�[�^�Z�b�g
		net.SetBatchSize(batch_size);
		for (size_t frame = 0; frame < batch_size; ++frame) {
			net.SetInputSignal(frame, images[x_index + frame]);
		}

		// �]�����{
		net.Forward(false);

		// ���ʏW�v
		for (size_t frame = 0; frame < batch_size; ++frame) {
			auto out_val = net.GetOutputSignal(frame);
			for (size_t i = 10; i < out_val.size(); i++) {
				out_val[i % 10] += out_val[i];
			}
			out_val.resize(10);
			int max_idx = bb::argmax<float>(out_val);
			ok_count += ((max_idx % 10) == (int)labels[x_index + frame] ? 1 : 0);
		}
	}

	// ���𗦂�Ԃ�
	return (double)ok_count / (double)images.size();
}


// �]���p�f�[�^�Z�b�g�Ő��𗦕]��
double CalcAccuracy(bb::NeuralNet<>& net)
{
	return CalcAccuracy(net, test_images, test_labels);
}


// ����(float)�̑S�ڑ��w�ŁA�t���b�g�ȃl�b�g��]��
void RunDenseAffineSigmoid(int epoc_size, size_t max_batch_size, double learning_rate)
{
	std::cout << "start [RunDenseAffineSigmoid]" << std::endl;
	reset_time();

	// ������NET�\�z
	bb::NeuralNet<> net;
	bb::NeuralNetAffine<>  layer0_affine(3 * 32 * 32, 200);
	bb::NeuralNetSigmoid<> layer0_sigmoid(200);
	bb::NeuralNetAffine<>  layer1_affine(200, 10);
	bb::NeuralNetSoftmax<> layer1_softmax(10);
	net.AddLayer(&layer0_affine);
	net.AddLayer(&layer0_sigmoid);
	net.AddLayer(&layer1_affine);
	net.AddLayer(&layer1_softmax);

	for (int epoc = 0; epoc < epoc_size; ++epoc) {

		// �w�K�󋵕]��
		std::cout << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << CalcAccuracy(net) << std::endl;

		for (size_t x_index = 0; x_index < train_images.size(); x_index += max_batch_size) {
			// �����̃o�b�`�T�C�Y�N���b�v
			size_t batch_size = std::min(max_batch_size, train_images.size() - x_index);

			// �f�[�^�Z�b�g
			net.SetBatchSize(batch_size);
			for (size_t frame = 0; frame < batch_size; ++frame) {
				net.SetInputSignal(frame, train_images[x_index + frame]);
			}

			// �\��
			net.Forward();

			// �덷�t�`�d
			for (size_t frame = 0; frame < batch_size; ++frame) {
				auto signals = net.GetOutputSignal(frame);
				for (size_t node = 0; node < signals.size(); ++node) {
					signals[node] -= train_onehot[x_index + frame][node];
					signals[node] /= (float)batch_size;
				}
				net.SetOutputError(frame, signals);
			}
			net.Backward();

			// �X�V
			net.Update();
		}
	}
	std::cout << "end\n" << std::endl;
}


// ����(float)�̑S�ڑ��w�ŁA�t���b�g�ȃl�b�g��]��
void RunDenseAffineSigmoid2(int epoc_size, size_t max_batch_size)
{
	std::cout << "start [RunDenseAffineSigmoid2]" << std::endl;
	reset_time();

	std::mt19937_64 mt(1);

	// ������NET�\�z
	bb::NeuralNet<> net;
	bb::NeuralNetAffine<>  layer0_affine(3 * 32 * 32, 200);
	bb::NeuralNetSigmoid<> layer0_sigmoid(200);
	bb::NeuralNetAffine<>  layer1_affine(200, 10);
//	bb::NeuralNetSoftmax<> layer1_softmax(10);
	net.AddLayer(&layer0_affine);
	net.AddLayer(&layer0_sigmoid);
	net.AddLayer(&layer1_affine);
//	net.AddLayer(&layer1_softmax);

	// �I�v�e�B�}�C�U�ݒ�
	net.SetOptimizer(&bb::NeuralNetOptimizerAdam<>(0.001f, 0.9f, 0.999f));

	bb::NeuralNetLossCrossEntropyWithSoftmax<>			lossFunc;
	bb::NeuralNetAccuracyCategoricalClassification<>	accFunc(10);

	for (int epoc = 0; epoc < epoc_size; ++epoc) {
		// �w�K�󋵕]��
		auto accuracy = net.RunCalculation(train_images, train_onehot, max_batch_size, &accFunc);
		std::cout << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << accuracy << std::endl;

		// �w�K���{
		net.RunCalculation(train_images, train_onehot, max_batch_size, &accFunc, &lossFunc, true, true);

		// Shuffle
		bb::ShuffleDataSet(mt(), train_images, train_onehot);
	}
	std::cout << "end\n" << std::endl;
}


// ����(float)�̑S�ڑ��w�ŁA�t���b�g�ȃl�b�g��]��
void RunDenseAffineSigmoid3(int epoc_size, size_t max_batch_size)
{
	std::cout << "start [DenseAffineSigmoid3]" << std::endl;
	reset_time();

	std::mt19937_64 mt(1);

	// ������NET�\�z
	bb::NeuralNet<> net;
	bb::NeuralNetAffine<>  layer0_affine(3 * 32 * 32, 200);
	bb::NeuralNetSigmoid<> layer0_sigmoid(200);
	bb::NeuralNetAffine<>  layer1_affine(200, 10);
	//	bb::NeuralNetSoftmax<> layer1_softmax(10);
	net.AddLayer(&layer0_affine);
	net.AddLayer(&layer0_sigmoid);
	net.AddLayer(&layer1_affine);
	//	net.AddLayer(&layer1_softmax);

	// �I�v�e�B�}�C�U�ݒ�
	net.SetOptimizer(&bb::NeuralNetOptimizerAdam<>(0.001f, 0.9f, 0.999f));

	bb::NeuralNetLossCrossEntropyWithSoftmax<>			lossFunc;
	bb::NeuralNetAccuracyCategoricalClassification<>	accFunc(10);
	net.Fitting("DenseAffineSigmoid", train_images, train_onehot, epoc_size, max_batch_size, &accFunc, &lossFunc);

	/*
	for (int epoc = 0; epoc < epoc_size; ++epoc) {
		// �w�K�󋵕]��
		auto accuracy = net.RunCalculation(train_images, train_onehot, max_batch_size, &accFunc);
		std::cout << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << accuracy << std::endl;

		// �w�K���{
		net.RunCalculation(train_images, train_onehot, max_batch_size, &accFunc, &lossFunc, true, true);

		// Shuffle
		bb::ShuffleDataSet(mt(), train_images, train_onehot);
	}
	*/


	std::cout << "end\n" << std::endl;
}


#if 0
// LUT6���͂̃o�C�i���ł̃t���b�g�ȃl�b�g��]��
void RunFlatBinaryLut6(int epoc_size, size_t max_batch_size, int max_iteration = -1)
{
	std::cout << "start [RunFlatBinaryLut6]" << std::endl;
	reset_time();

	std::mt19937_64 mt(1);

	// �w�K���ƕ]�����ő��d����(������ς��ĕ������ʂ��ďW�v�ł���悤�ɂ���)��ς���
	int train_mux_size = 1;
	int test_mux_size = 16;

	// �w�\����`
	size_t input_node_size = 3 * 32 * 32;
	size_t layer0_node_size = 2160 * 1;
	size_t layer1_node_size = 360 * 4;
	size_t layer2_node_size = 60 * 6;
	size_t layer3_node_size = 10 * 8;
	size_t output_node_size = 10;

	// �w�K�pNET�\�z
	bb::NeuralNet<> net;
	bb::NeuralNetRealToBinary<>	layer_real2bin(input_node_size, input_node_size * 4, mt());
	bb::NeuralNetBinaryLut6<>	layer_lut0(input_node_size * 4, layer0_node_size, mt());
	bb::NeuralNetBinaryLut6<>	layer_lut1(layer0_node_size, layer1_node_size, mt());
	bb::NeuralNetBinaryLut6<>	layer_lut2(layer1_node_size, layer2_node_size, mt());
	bb::NeuralNetBinaryLut6<>	layer_lut3(layer2_node_size, layer3_node_size, mt());
	bb::NeuralNetBinaryToReal<>	layer_bin2real(layer3_node_size, output_node_size, mt());
	auto last_lut_layer = &layer_lut3;
	net.AddLayer(&layer_real2bin);
	net.AddLayer(&layer_lut0);
	net.AddLayer(&layer_lut1);
	net.AddLayer(&layer_lut2);
	net.AddLayer(&layer_lut3);
	//	net.AddLayer(&layer_bin2real);	// �w�K���� bin2real �s�v

	// �]���pNET�\�z(�m�[�h�͋��L)
	bb::NeuralNet<> net_eva;
	net_eva.AddLayer(&layer_real2bin);
	net_eva.AddLayer(&layer_lut0);
	net_eva.AddLayer(&layer_lut1);
	net_eva.AddLayer(&layer_lut2);
	net_eva.AddLayer(&layer_lut3);
	net_eva.AddLayer(&layer_bin2real);

	// �w�K���[�v
	int iteration = 0;
	for (int epoc = 0; epoc < epoc_size; ++epoc) {
		// �w�K�󋵕]��
		layer_real2bin.InitializeCoeff(1);
		layer_bin2real.InitializeCoeff(1);
		net_eva.SetMuxSize(test_mux_size);
		std::cout << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << CalcAccuracy(net_eva) << std::endl;

		for (size_t x_index = 0; x_index < train_images.size(); x_index += max_batch_size) {
			// �����̃o�b�`�T�C�Y�N���b�v
			size_t batch_size = std::min(max_batch_size, train_images.size() - x_index);

			// �o�b�`�w�K�f�[�^�̍쐬
			std::vector< std::vector<float> >	batch_images(train_images.begin() + x_index, train_images.begin() + x_index + batch_size);
			std::vector< std::uint8_t >			batch_labels(train_labels.begin() + x_index, train_labels.begin() + x_index + batch_size);

			// �f�[�^�Z�b�g
			net.SetMuxSize(train_mux_size);
			net.SetBatchSize(batch_size);
			for (size_t frame = 0; frame < batch_size; ++frame) {
				net.SetInputSignal(frame, batch_images[frame]);
			}

			// �\��
			net.Forward();

			// �o�C�i���Ńt�B�[�h�o�b�N(�͋Z�w�K)
			while (net.Feedback(last_lut_layer->GetOutputOnehotLoss<std::uint8_t, 10>(batch_labels)))
				;

			// ���ԕ\��()
#if 0
			layer_real2bin.InitializeCoeff(1);
			layer_bin2real.InitializeCoeff(1);
			net_eva.SetMuxSize(test_mux_size);
			std::cout << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << CalcAccuracy(net_eva) << std::endl;
#endif

			iteration++;
			if (max_iteration > 0 && iteration >= max_iteration) {
				goto loop_end;
			}
		}
	}
loop_end:


	{
		std::ofstream ofs("lut_net.v");
		bb::NeuralNetBinaryLut6VerilogXilinx(ofs, layer_lut0, "lutnet_layer0");
		bb::NeuralNetBinaryLut6VerilogXilinx(ofs, layer_lut1, "lutnet_layer1");
		bb::NeuralNetBinaryLut6VerilogXilinx(ofs, layer_lut2, "lutnet_layer2");
	}

	std::cout << "end\n" << std::endl;
}


// �u�[��������v�̍\����Sigmoid
void RunSimpleConvSigmoid(int epoc_size, size_t max_batch_size, double learning_rate)
{
	std::cout << "start [RunSimpleConvSigmoid]" << std::endl;
	reset_time();

	// ������NET�\�z
	bb::NeuralNet<> net;
	bb::NeuralNetConvolution<>  layer0_conv(3, 32, 32, 30, 5, 5);
	bb::NeuralNetSigmoid<>		layer0_sigmoid(30 * 28 * 28);
	bb::NeuralNetMaxPooling<>	layer0_maxpol(30, 28, 28, 2, 2);

	bb::NeuralNetAffine<>		layer1_affine(30 * 14 * 14, 100);
	bb::NeuralNetSigmoid<>		layer1_sigmoid(100);

	bb::NeuralNetAffine<>		layer2_affine(100, 10);
	bb::NeuralNetSoftmax<>		layer2_softmax(10);
	net.AddLayer(&layer0_conv);
	net.AddLayer(&layer0_sigmoid);
	net.AddLayer(&layer0_maxpol);
	net.AddLayer(&layer1_affine);
	net.AddLayer(&layer1_sigmoid);
	net.AddLayer(&layer2_affine);
	net.AddLayer(&layer2_softmax);

	for (int epoc = 0; epoc < epoc_size; ++epoc) {

		// �w�K�󋵕]��
		std::cout << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << CalcAccuracy(net) << std::endl;


		for (size_t x_index = 0; x_index < train_images.size(); x_index += max_batch_size) {
			// �����̃o�b�`�T�C�Y�N���b�v
			size_t batch_size = std::min(max_batch_size, train_images.size() - x_index);

			// �f�[�^�Z�b�g
			net.SetBatchSize(batch_size);
			for (size_t frame = 0; frame < batch_size; ++frame) {
				net.SetInputSignal(frame, train_images[x_index + frame]);
			}

			// �\��
			net.Forward();

			// �덷�t�`�d
			for (size_t frame = 0; frame < batch_size; ++frame) {
				auto signals = net.GetOutputSignal(frame);
				for (size_t node = 0; node < signals.size(); ++node) {
					signals[node] -= train_onehot[x_index + frame][node];
					signals[node] /= (float)batch_size;
				}
				net.SetOutputError(frame, signals);
			}
			net.Backward();

			// �X�V
			net.Update(learning_rate);
		}
	}
	std::cout << "end\n" << std::endl;
}
#endif


// ����(float)�̑S�ڑ��w�ŁA�t���b�g�ȃl�b�g��]��
void RunSimpleDenseConvolution(int epoc_size, size_t max_batch_size, bool binary_mode)
{
	std::cout << "start [SimpleDenseConvolution]" << std::endl;
	reset_time();

	std::string JsonName = "SimpleDenseConvolution.json";

	std::mt19937_64 mt(1);

	// ������NET�\�z
	bb::NeuralNetBatchNormalization<>	input_batch_norm(3 * 32 * 32);
	bb::NeuralNetSigmoid<>				input_activation(3 * 32 * 32);

	bb::NeuralNetConvolution<>			layer0_convolution(3, 32, 32, 32, 3, 3);
	bb::NeuralNetBatchNormalization<>	layer0_batch_norm(32 * 30 * 30);
	bb::NeuralNetSigmoid<>				layer0_activation(32 * 30 * 30);

	bb::NeuralNetConvolution<>			layer1_convolution(32, 30, 30, 32, 3, 3);
	bb::NeuralNetBatchNormalization<>	layer1_batch_norm(32 * 28 * 28);
	bb::NeuralNetSigmoid<>				layer1_activation(32 * 28 * 28);

	bb::NeuralNetMaxPooling<>			layer2_pooling(32, 28, 28, 2, 2);

	bb::NeuralNetConvolution<>			layer3_convolution(32, 14, 14, 64, 3, 3);
	bb::NeuralNetBatchNormalization<>	layer3_batch_norm(64 * 12 * 12);
	bb::NeuralNetSigmoid<>				layer3_activation(64 * 12 * 12);

	bb::NeuralNetConvolution<>			layer4_convolution(64, 12, 12, 64, 3, 3);
	bb::NeuralNetBatchNormalization<>	layer4_batch_norm(64 * 10 * 10);
	bb::NeuralNetSigmoid<>				layer4_activation(64 * 10 * 10);

	bb::NeuralNetMaxPooling<>			layer5_pooling(64, 10, 10, 2, 2);

	bb::NeuralNetAffine<>				layer6_affine(64 * 5 * 5, 512);
	bb::NeuralNetBatchNormalization<>	layer6_batch_norm(512);
	bb::NeuralNetSigmoid<>				layer6_activation(512);

	bb::NeuralNetAffine<>				layer7_affine(512, 10);
	bb::NeuralNetBatchNormalization<>	layer7_batch_norm(10);
	bb::NeuralNetSigmoid<>				layer7_activation(10);

	bb::NeuralNetSoftmax<>				output_softmax(10);

	bb::NeuralNet<> net;
	net.AddLayer(&input_batch_norm);
	net.AddLayer(&input_activation);
	net.AddLayer(&layer0_convolution);
	net.AddLayer(&layer0_batch_norm);
	net.AddLayer(&layer0_activation);
	net.AddLayer(&layer1_convolution);
	net.AddLayer(&layer1_batch_norm);
	net.AddLayer(&layer1_activation);
	net.AddLayer(&layer2_pooling);
	net.AddLayer(&layer3_convolution);
	net.AddLayer(&layer3_batch_norm);
	net.AddLayer(&layer3_activation);
	net.AddLayer(&layer4_convolution);
	net.AddLayer(&layer4_batch_norm);
	net.AddLayer(&layer4_activation);
	net.AddLayer(&layer5_pooling);
	net.AddLayer(&layer6_affine);
	net.AddLayer(&layer6_batch_norm);
	net.AddLayer(&layer6_activation);
	net.AddLayer(&layer7_affine);
	net.AddLayer(&layer7_batch_norm);
	net.AddLayer(&layer7_activation);
	net.AddLayer(&output_softmax);

	// �I�v�e�B�}�C�U�ݒ�
	net.SetOptimizer(&bb::NeuralNetOptimizerAdam<>(0.001f, 0.9f, 0.999f));

	// �o�C�i���ݒ�
	std::cout << "binary mode : " << binary_mode << std::endl;
	net.SetBinaryMode(binary_mode);

	if (1) {
		std::ofstream ofs(JsonName);
		cereal::JSONOutputArchive ar(ofs);
		net.Save(ar);
	}

	{
		std::ifstream ifs(JsonName);
		cereal::JSONInputArchive ar(ifs);
		net.Load(ar);
	}

	// �w�K���[�v
	for (int epoc = 0; epoc < epoc_size; ++epoc) {

		// �w�K�󋵕]��
		std::cout << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << CalcAccuracy(net) << std::endl;

		for (size_t x_index = 0; x_index < train_images.size(); x_index += max_batch_size) {
			// �����̃o�b�`�T�C�Y�N���b�v
			size_t batch_size = std::min(max_batch_size, train_images.size() - x_index);

			// �f�[�^�Z�b�g
			net.SetBatchSize(batch_size);
			for (size_t frame = 0; frame < batch_size; ++frame) {
				net.SetInputSignal(frame, train_images[x_index + frame]);
			}

			// �\��
			net.Forward();

			// �덷�t�`�d
			float loss = 0;
			for (size_t frame = 0; frame < batch_size; ++frame) {
				auto signals = net.GetOutputSignal(frame);
				for (size_t node = 0; node < signals.size(); ++node) {
					signals[node] -= train_onehot[x_index + frame][node];
					loss += signals[node] * signals[node];
					signals[node] /= (float)batch_size;
				}
				net.SetOutputError(frame, signals);
			}
			loss = sqrt(loss / batch_size);
			net.Backward();

			// �X�V
			net.Update();

			// �i���\��
			PrintProgress(loss, x_index + batch_size, train_images.size());
		}
		ClearProgress();

		// Shuffle
		bb::ShuffleDataSet(mt(), train_images, train_onehot);

		// �ۑ�
		std::ofstream ofs(JsonName);
		cereal::JSONOutputArchive ar(ofs);
		net.Save(ar);
	}
	std::cout << "end\n" << std::endl;
}



// ����(float)�̑S�ڑ��w�ŁA�t���b�g�ȃl�b�g��]��
void RunSimpleSparseConvolution(int epoc_size, size_t max_batch_size, bool binary_mode)
{
	std::cout << "start [SimpleSparseConvolution]" << std::endl;
	reset_time();

	std::mt19937_64 mt(1);

	// Conv�psub�l�b�g�\�z (3x3)
	bb::NeuralNetSparseAffineSigmoid<>	real_sub0_affine0(24 * 3 * 3, 192);
	bb::NeuralNetSparseAffineSigmoid<>	real_sub0_affine1(192, 32);
	bb::NeuralNetGroup<>				real_sub0_net;
	real_sub0_net.AddLayer(&real_sub0_affine0);
	real_sub0_net.AddLayer(&real_sub0_affine1);

	// Conv�psub�l�b�g�\�z (3x3)
	bb::NeuralNetSparseAffineSigmoid<>	real_sub1_affine0(32 * 3 * 3, 192);
	bb::NeuralNetSparseAffineSigmoid<>	real_sub1_affine1(192, 32);
	bb::NeuralNetGroup<>				real_sub1_net;
	real_sub1_net.AddLayer(&real_sub1_affine0);
	real_sub1_net.AddLayer(&real_sub1_affine1);

	// Conv�psub�l�b�g�\�z (3x3)
	bb::NeuralNetSparseAffineSigmoid<>	real_sub3_affine0(32 * 3 * 3, 192);
	bb::NeuralNetSparseAffineSigmoid<>	real_sub3_affine1(192, 32);
	bb::NeuralNetGroup<>				real_sub3_net;
	real_sub3_net.AddLayer(&real_sub3_affine0);
	real_sub3_net.AddLayer(&real_sub3_affine1);

	// Conv�psub�l�b�g�\�z (3x3)
	bb::NeuralNetSparseAffineSigmoid<>	real_sub4_affine0(32 * 3 * 3, 192);
	bb::NeuralNetSparseAffineSigmoid<>	real_sub4_affine1(192, 32);
	bb::NeuralNetGroup<>				real_sub4_net;
	real_sub4_net.AddLayer(&real_sub4_affine0);
	real_sub4_net.AddLayer(&real_sub4_affine1);

	// �o�C�i���l�b�g
	bb::NeuralNetConvolutionPack<>		real_layer0_conv(&real_sub0_net, 24, 32, 32, 32, 3, 3);
	bb::NeuralNetConvolutionPack<>		real_layer1_conv(&real_sub1_net, 32, 30, 30, 32, 3, 3);
	bb::NeuralNetMaxPooling<>			real_layer2_maxpol(32, 28, 28, 2, 2);
	bb::NeuralNetConvolutionPack<>		real_layer3_conv(&real_sub3_net, 32, 14, 14, 32, 3, 3);
	bb::NeuralNetConvolutionPack<>		real_layer4_conv(&real_sub4_net, 32, 12, 12, 32, 3, 3);
	bb::NeuralNetMaxPooling<>			real_layer5_maxpol(32, 10, 10, 2, 2);
	bb::NeuralNetSparseAffineSigmoid<>	real_layer6_affine(32 * 5 * 5, 420);
	bb::NeuralNetSparseAffineSigmoid<>	real_layer7_affine(420, 80);
	bb::NeuralNetGroup<>				real_mux_group;
	real_mux_group.AddLayer(&real_layer0_conv);
	real_mux_group.AddLayer(&real_layer1_conv);
	real_mux_group.AddLayer(&real_layer2_maxpol);
	real_mux_group.AddLayer(&real_layer3_conv);
	real_mux_group.AddLayer(&real_layer4_conv);
	real_mux_group.AddLayer(&real_layer5_maxpol);
	real_mux_group.AddLayer(&real_layer6_affine);
	real_mux_group.AddLayer(&real_layer7_affine);

	// �g�b�v�l�b�g
	bb::NeuralNetBinaryMultiplex<float>	real_mux(&real_mux_group, 3*32*32, 10, 8, 8);
	bb::NeuralNetSoftmax<>				output_softmax(10);
	bb::NeuralNet<> real_net;
	real_net.AddLayer(&real_mux);
	real_net.AddLayer(&output_softmax);

	if(0){
		std::ofstream ofs("SimpleSparseConvolution.json");
		cereal::JSONOutputArchive ar(ofs);
		real_net.Save(ar);
	}
	
	{
		std::ifstream ifs("SimpleSparseConvolution.json");
		cereal::JSONInputArchive ar(ifs);
		real_net.Load(ar);
	}


	// �I�v�e�B�}�C�U�ݒ�
	real_net.SetOptimizer(&bb::NeuralNetOptimizerAdam<>(0.001f, 0.9f, 0.999f));

	// �o�C�i���ݒ�
	std::cout << "binary mode : " << binary_mode << std::endl;
	real_net.SetBinaryMode(binary_mode);

	real_mux.SetMuxSize(1);

	// �w�K���[�v
	for (int epoc = 0; epoc < epoc_size; ++epoc) {

		// �w�K�󋵕]��
		real_mux.SetMuxSize(3);
		std::cout << get_time() << "s " << "epoc[" << epoc << "] accuracy : " << CalcAccuracy(real_net) << std::endl;

		real_mux.SetMuxSize(1);
		for (size_t x_index = 0; x_index < train_images.size(); x_index += max_batch_size) {
			// �����̃o�b�`�T�C�Y�N���b�v
			size_t batch_size = std::min(max_batch_size, train_images.size() - x_index);

			// �f�[�^�Z�b�g
			real_net.SetBatchSize(batch_size);
			for (size_t frame = 0; frame < batch_size; ++frame) {
				real_net.SetInputSignal(frame, train_images[x_index + frame]);
			}

			// �\��
			real_net.Forward();

			// �덷�t�`�d
			float loss = 0;
			for (size_t frame = 0; frame < batch_size; ++frame) {
				auto signals = real_net.GetOutputSignal(frame);
				for (size_t node = 0; node < signals.size(); ++node) {
					signals[node] -= train_onehot[x_index + frame][node];
					loss += signals[node] * signals[node];
					signals[node] /= (float)batch_size;
				}
				real_net.SetOutputError(frame, signals);
			}
			loss = sqrt(loss / batch_size);
			real_net.Backward();

			// �X�V
			real_net.Update();

			// �i���\��
			PrintProgress(loss, x_index + batch_size, train_images.size());
		}
		ClearProgress();

		{
			std::ofstream ofs("SimpleSparseConvolution.json");
			cereal::JSONOutputArchive ar(ofs);
			real_net.Save(ar);
		}

		// Shuffle
		bb::ShuffleDataSet(mt(), train_images, train_onehot);
	}
	std::cout << "end\n" << std::endl;
}



int main()
{
	omp_set_num_threads(6);
	
	std::vector< std::vector<int> >::iterator exp_begin;

	// �t�@�C���ǂݍ���
	std::vector< std::vector<std::uint8_t> > train_images_u8;
	std::vector< std::vector<std::uint8_t> > test_images_u8;

	read_cifer10("data_batch_1.bin", train_labels, train_images_u8);
	read_cifer10("data_batch_2.bin", train_labels, train_images_u8);
	read_cifer10("data_batch_3.bin", train_labels, train_images_u8);
	read_cifer10("data_batch_4.bin", train_labels, train_images_u8);
	read_cifer10("data_batch_5.bin", train_labels, train_images_u8);
	read_cifer10("test_batch.bin", test_labels, test_images_u8);

#ifdef _DEBUG
	std::cout << "!!! DEBUG !!!\n" << std::endl;
	int train_size = 512;
	int test_size  = 256;

	train_labels.resize(train_size);
	train_images_u8.resize(train_size);
	test_labels.resize(test_size);
	test_images_u8.resize(test_size);
#endif

	// ���K��
	train_images = bb::DataTypeConvert<std::uint8_t, float, float>(train_images_u8, 1.0f / 255.0f);
	train_onehot = bb::LabelToOnehot<std::uint8_t, float>(train_labels, 10);
	test_images = bb::DataTypeConvert<std::uint8_t, float, float>(test_images_u8, 1.0f / 255.0f);
	test_onehot = bb::LabelToOnehot<std::uint8_t, float>(test_labels, 10);

	// �\���m�F
#if 0
	cv::Mat img(32, 32, CV_32FC3);
	for (int frame = 0; frame < 100; frame++) {
		for (int i = 0; i < 32 * 32; i++) {
			img.at<cv::Vec3f>(i / 32, i % 32)[2] = train_images[frame][(32 * 32) * 0 + i];
			img.at<cv::Vec3f>(i / 32, i % 32)[1] = train_images[frame][(32 * 32) * 1 + i];
			img.at<cv::Vec3f>(i / 32, i % 32)[0] = train_images[frame][(32 * 32) * 2 + i];
		}
		std::cout << (int)train_labels[frame] << std::endl;
		cv::imshow("img", img);
		cv::waitKey();
	}
#endif

	RunDenseAffineSigmoid3(64, 128);

	//////
//	RunSimpleDenseConvolution(1000, 64, false);
//	RunSimpleSparseConvolution(1000, 128, true);
//	RunSimpleConvolution(1000, 128, true);

#if 0
	// �o�C�i��6����LUT�Ŋw�K����(�d���ł�)
	RunFlatBinaryLut6(100, 16*8192, -1);
#endif

#if 0
	RunDenseAffineSigmoid(100, 256, 1.0);
#endif

	getchar();

	return 0;
}
